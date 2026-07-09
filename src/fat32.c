#include "fat32.h"
#include "mmc.h"

#include <libopencm3/cm3/assert.h>

#define FAT_BS_SIGN           ( 0x0000AA55U )
#define FAT_FSI_LEAD_SIGN     ( 0x41615252U )
#define FAT_FSI_STRUC_SIG     ( 0x61417272u )
#define FAT_FSI_TRAIL_SIG     ( 0xAA550000U )

#define FAT_EOC               ( 0x0FFFFFFF )

#define FD_TABLE_LENGTH       ( 4 )

#define CLUS_LO_PLUS_HI( CLUS_LO, CLUS_HI ) \
        ( ( ( CLUS_HI ) << 16 ) | ( CLUS_LO ) )

#define BYTES_PER_CLUSTER ( ( fs.bytes_per_sector ) * ( fs.sectors_per_cluster ) )

static fat_file_t fd_table[FD_TABLE_LENGTH];

static fat32_fs_info_t fs_info;

static fat_fs_t fs;

/**
 * @brief  Copy memory contents from source to destination.
 * @param  dest  Pointer to the destination buffer.
 * @param  src   Pointer to the source buffer.
 * @param  len   Number of bytes to copy.
 * @return 0 on success, -1 if dest or src is NULL.
 */
static int _fat_memcpy( void *restrict dest, void const *restrict src, size_t len ) {
    if ( dest == 0 || src == 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | unexpected input: null_pointer dereference\r\n", __LINE__ );
#endif
        return -1;
    } 
    size_t i;
    for ( i = 0; i < len / 4; i++ ) ( ( uint32_t * )dest )[i] = ( ( uint32_t * )src )[i]; 
    for ( i = len & ~3; i < len; i++ ) ( ( uint8_t * )dest )[i] = ( ( uint8_t * )src )[i];
    return 0;
}

/**
 * @brief  Calculate the length of a null-terminated string.
 * @param  str  Pointer to the input string.
 * @return Number of characters before the terminating null byte.
 */
static inline uint32_t _fat_strlen( const char* str ) {
    uint32_t len = 0;
    while ( *str++ != '\0' ) len++;
    return len;
}

/**
 * @brief  Compare two strings up to a given length.
 * @param  str1  First string to compare.
 * @param  str2  Second string to compare.
 * @param  len   Maximum number of characters to compare.
 * @return 0 if strings match, -1 if a mismatch is found.
 */
static inline int _fat_strcmp( const char *restrict str1, const char *restrict str2, size_t len ) {
    for ( size_t s_idx = 0; s_idx < len; s_idx++ ) {
        if ( str1[s_idx] != str2[s_idx] ) return -1;
    }
    return 0;
}


/**
 * @brief  Count occurrences of a character in a bounded string.
 * @param  in   Pointer to the input string.
 * @param  c    Character to count.
 * @param  len  Maximum number of characters to search.
 * @return Number of occurrences of @p c.
 */
static inline uint8_t _fat_strcount( const char *in, const char c, size_t len ) {
    size_t counter = 0;
    for ( size_t idx = 0; idx < len; idx++ ) {
        if ( in[idx] == c ) counter++;
    }
    return counter;
}

/**
 * @brief  Find the first occurrence of a character in a bounded string.
 * @param  in   Pointer to the input string.
 * @param  c    Character to locate.
 * @param  len  Maximum number of characters to search.
 * @return Index of the first occurrence, or len if not found.
 */
static inline int _fat_strfind( const char *in, const char c, size_t len ) {
    size_t counter = 0;
    while ( *in++ != c ) {
        counter++;
        if ( *in == '\0' || counter > len ) break;
    }
    return counter;
}


/**
 * @brief  Read a single sector from the MMC into a buffer.
 * @param  sector  Sector index to read.
 * @param  length  Number of bytes to read.
 * @param  data    Destination buffer.
 * @note   Updates fs.last_sector_read on completion.
 */
static inline void fat_get_sector( uint32_t sector, size_t length, uint8_t *data ) {
    int r = mmc_read_single_block( sector, length, data );
#if defined( DEBUG_INFO_ENABLE )
    if ( r == -1 ) {
        printf_( "fat32.c:%d | Read of sector #%d was not successful\r\n", __LINE__, sector );
    }
#endif
    fs.last_sector_read = sector;
}

/**
 * @brief  Read multiple consecutive sectors from the MMC.
 * @param  sector  Starting sector index.
 * @param  length  Number of bytes to read.
 * @param  data    Destination buffer.
 * @note   Updates fs.last_sector_read to the last sector actually read.
 */
static inline void fat_get_multiple_sectors( uint32_t sector, size_t length, uint8_t *data ) {
    int off = mmc_read_multiple_blocks( sector, length, data );
    fs.last_sector_read = sector + off;
}

/**
 * @brief  Write a single sector to the MMC.
 * @param  sector  Sector index to write.
 * @param  length  Number of bytes to write.
 * @param  data    Source buffer containing data to write.
 * @note   Updates fs.last_sector_wrote on completion.
 */
static inline void fat_put_sector( uint32_t sector, size_t length, const uint8_t *data ) {
    ( void )mmc_write_single_block( sector, data, length );
    fs.last_sector_wrote = sector;
}

/**
 * @brief  Write multiple consecutive sectors to the MMC.
 * @param  sector  Starting sector index.
 * @param  length  Number of bytes to write.
 * @param  data    Source buffer.
 * @note   Updates fs.last_sector_wrote to the last sector actually written.
 */
static inline void fat_put_multiple_sectors( uint32_t sector, size_t length, const uint8_t *data ) {
    int off = mmc_write_multiple_blocks( sector, length, data );
    fs.last_sector_wrote = sector + off;
}

/**
 * @brief  Compute the first sector number of a given cluster.
 * @param  cluster  Cluster index (2-based).
 * @return First sector number of the cluster, or -1 if cluster is out of range.
 * @note   Cluster 2 maps to fs.data_start_sector.
 */
static inline int fat_cluster_offset( uint32_t cluster ) {
    if ( cluster > ( fs.data_sectors / fs.sectors_per_cluster ) ) return -1;
    return fs.data_start_sector + ( cluster - 2 ) * fs.sectors_per_cluster; 
}

/**
 * @brief  Compute the sector number containing a given FAT entry.
 * @param  entry  FAT entry index.
 * @return Sector number containing the entry, or -1 on error (invalid entry or sub type).
 * @note   Handles FAT16 and FAT32 entry sizes; FAT12 is not implemented.
 */
static inline int fat_entry_sector_num( uint32_t entry ) {
    switch ( fs.sub_type ) {
        // TODO: Reserach implementation details.
        //case FAT_SUB_TYPE_FAT12: { 
            //if ( entry < 0 || entry > ( fs.fat_sectors / 1.5f ) ) return -1; 
            //return fs.fat_start_sector + ( int )( ( entry * 1.5f ) / fs->bytes_per_sector );
        //} break;
        case FAT_SUB_TYPE_FAT16: {
            if ( entry > ( fs.fat_sectors / 2 ) ) return -1; 
            return fs.fat_start_sector + ( int )( ( entry * 2 ) / fs.bytes_per_sector );
        } break;
        case FAT_SUB_TYPE_FAT32: {
            if ( entry > ( fs.fat_sectors / 4 ) ) return -1; 
            return fs.fat_start_sector + ( int )( ( entry * 4 ) / fs.bytes_per_sector );
        } break;
        default: {
            printf_( "fat32.c:%d , Malformed FAT sub type\r\n", __LINE__ );
            return -1;
        } break;
    }
}

/**
 * @brief  Compute the byte offset of a FAT entry within its sector.
 * @param  entry  FAT entry index.
 * @return Byte offset within the sector, or -1 on error.
 * @note   Handles FAT16 and FAT32 entry sizes; FAT12 is not implemented.
 */
static inline int fat_entry_sec_offset( uint32_t entry ) {
    switch ( fs.sub_type ) {
        //case FAT_SUB_TYPE_FAT12: { 
        //} break;
        case FAT_SUB_TYPE_FAT16: {
            if ( entry > ( fs.fat_sectors / 2 ) ) return -1; 
            return ( ( entry * 2 ) % fs.bytes_per_sector ); 
        } break;
        case FAT_SUB_TYPE_FAT32: {
            if ( entry > ( fs.fat_sectors / 4 ) ) return -1; 
            return ( ( entry * 4 ) % fs.bytes_per_sector ); 
        } break;
        default: {
            printf_( "fat32.c:%d | Malformed FAT sub type", __LINE__ );
            return -1;
        } break;
    }
}

/**
 * @brief  Read the value of a FAT entry from the File Allocation Table.
 * @param  entry  FAT entry index (cluster number).
 * @return The FAT entry value masked with FAT_EOC (0x0FFFFFFF).
 * @note   Currently reads the entire sector (512 bytes) to extract a 4-byte value.
 */
static inline uint32_t fat_entry( uint32_t entry ) {
    int sector = fat_entry_sector_num( entry );
    int offset = fat_entry_sec_offset( entry );
    fat_get_sector( sector, sizeof( fs.sector_buffer ), fs.sector_buffer ); 
    fs.last_fat_read = entry;
    return *( uint32_t * )&fs.sector_buffer[offset] & FAT_EOC;
}



/**
 * @brief  Find a free file descriptor slot in the global fd_table.
 * @return Index of the first unused slot (mode has no O_OPEN flag), or -1 if all slots are in use.
 */
static int fat_find_fd( void ) {
    for ( int table_index = 0; table_index < FD_TABLE_LENGTH; table_index++ ) {
        if ( !(  fd_table[table_index].mode & O_OPEN  ) ) return table_index;
    }
    return -1;
}

/**
 * @brief  Convert a character to its DOS-compatible representation.
 * @param  in  Input character.
 * @return DOS-compatible character (uppercase, '\\' for '/'), 0 for null,
 *         or 1 for invalid/unrepresentable characters.
 */
static char fat_to_doschar( char in ) {
    if ( in == 0 )  {
        return 0;
    } else if ( in >= 'a' && in <= 'z' ) {
        return in - 32;
    } else if ( in >= '0' && in <= '9' ) {
        return in;
    } else if ( in < 32 || in == 127 || in == ' ' ||
        in == '+' || in == '[' || in == ']' ) {
        return 1;
    } else if ( in == '/' ) {
        return '\\';
    } else if ( in > 126 ) {
        return 1;
    } else return in;
}

/**
 * @brief  Convert a filename to an 8.3 DOS short filename.
 * @param  dosname  Output buffer (must be at least 11 bytes).
 * @param  name     Input filename.
 * @param  name_len Length of the input filename.
 * @return 0 on success, -1 if the name is invalid (too long, starts with '.',
 *         or contains invalid characters).
 * @note   Names without an extension are left-justified and space-padded to 11 chars.
 *         Names with an extension use up to 8 chars for the base and 3 for the ext.
 */
static int fat_name_to_dosname( char *restrict dosname, const char *restrict name, size_t name_len ) {
    uint8_t nm_ptr = 0, dnm_ptr = 0;
    char c;
    if ( name_len > 11 ) return -1;
    if ( name[nm_ptr] == '.' ) return -1;
    int with_ext = _fat_strcount(name, '.', name_len);
    if ( with_ext > 1 ) return -1;

    if ( with_ext == 0 ) {
        while ( nm_ptr < name_len ) {
            if ( ( c = fat_to_doschar( name[nm_ptr] ) ) != 1 ) {
                dosname[dnm_ptr] = c;
            } else return -1;
            dnm_ptr++; nm_ptr++;
        }
        while ( dnm_ptr < 11 ) {
            dosname[dnm_ptr] = ' ';
            dnm_ptr++;
        }
    }

    if ( with_ext == 1 ) {
        int base_len = _fat_strfind( name, '.', name_len );
        while ( nm_ptr < base_len ) {
            if ( ( c = fat_to_doschar( name[nm_ptr] ) ) != 1 ) {
                dosname[dnm_ptr] = c;
            } else return -1;
            nm_ptr++; dnm_ptr++;
        }
        while ( dnm_ptr < 8 ) dosname[dnm_ptr++] = ' ';

        if ( name[nm_ptr++] != '.' ) { return -1; } 
        while ( nm_ptr < name_len ) {
            if ( ( c = fat_to_doschar( name[nm_ptr] ) ) != 1 ) {
                dosname[dnm_ptr] = c;
            } else return -1;
            nm_ptr++; dnm_ptr++;
        }
        while ( dnm_ptr < 11 ) dosname[dnm_ptr++] = ' ';
    }

    return 0;
}

/**
 * @brief  Convert a UNIX-style path to a DOS-style path.
 * @param  dospath  Output buffer for the DOS path.
 * @param  path     Input UNIX path (e.g. "/dir/file.txt").
 * @return 0 on success.
 * @note   Each path component is converted to an 11-byte DOS name prefixed by '\\'.
 *         Long file names (LFN) are not supported.
 */
static int fat_path_to_dospath( char *restrict dospath, const char *restrict path ) {
    // TODO: Possible create another function or expand this one to support LFN format
    size_t i_ptr = 0, j_ptr = 0, d_ptr = 0;
    do {
        dospath[d_ptr++] = '\\';
        i_ptr++; j_ptr++;
        j_ptr += _fat_strfind( &path[i_ptr], '/', 11 );
        fat_name_to_dosname( &dospath[d_ptr], ( void * )&path[i_ptr], j_ptr - i_ptr );
        i_ptr = j_ptr;
        d_ptr += 11;
    } while ( path[j_ptr] != '\0' );
    dospath[d_ptr] = '\0';
    return 0;
}

/**
 * @brief  Search a sector buffer for a directory entry matching a DOS name.
 * @param  sector    Sector buffer (MMC_BLOCK_SIZE bytes).
 * @param  dir_entry Output: populated with the matching directory entry.
 * @param  dos_name  11-byte DOS name to match.
 * @return 0 if a matching entry is found, -1 otherwise.
 * @note   Stops early if an entry with dir_name[0] == 0 (end of directory) is encountered.
 */
static int fat_traverse_dir_entries( uint8_t sector[MMC_BLOCK_SIZE], fat_sdir_entry_t *dir_entry, char *dos_name ) {
    // TODO: Do conditional compilational blocks for other sub systems
    for ( uint16_t i = 0; i < MMC_BLOCK_SIZE; i += 32 ) {
        _fat_memcpy(dir_entry, &sector[i], 32);
        if ( dir_entry->dir_name[0] ==  0 ) break;
    // TODO: Possible create another function or expand this one to support LFN format
        if ( _fat_strcmp( dos_name, dir_entry->dir_name, 11 ) == 0 ) return 0;
    }
    return -1;
}

/**
 * @brief  Search a cluster chain for a file matching the given DOS name.
 * @param  dir_name  11-byte DOS name to find.
 * @param  dir_entry Output: populated with the matching directory entry.
 * @param  clus_num  Starting cluster number to search.
 * @return The sector number where the entry was found, or 0 if not found.
 * @note   Walks the entire cluster chain via fat_entry() until FAT_EOC.
 *         Breaks early if more than 10 consecutive unused entries are found.
 */
static size_t fat_file_is_present( char *dir_name, fat_sdir_entry_t *dir_entry, uint32_t clus_num ) {
    uint32_t sector_offset = 0;
    do {
        sector_offset = fat_cluster_offset( clus_num );
        int not_found_c = 0;
        for ( size_t sect_idx = sector_offset; sect_idx < sector_offset + fs.sectors_per_cluster; sect_idx++ ) {
            if ( not_found_c > 10 ) break;

            fat_get_sector( sect_idx, sizeof( fs.sector_buffer ), fs.sector_buffer );
            if ( fat_traverse_dir_entries( fs.sector_buffer, dir_entry, dir_name ) == 0 ) {
                return sect_idx;
            } else not_found_c++;
        }
    } while ( ( clus_num = fat_entry( clus_num ) ) != FAT_EOC );
    return 0;
}

/**
 * @brief  Calculate the required buffer length for a DOS path.
 * @param  path  Input UNIX path.
 * @param  len   Length of the input path.
 * @return Required buffer length (12 * levels + 1), or -1 if the path has no components.
 */
static inline int fat_calculate_dospath_length( const char *path, size_t len ) {
    uint8_t levels = _fat_strcount( path, '/', len );
    if ( levels == 0 ) return -1;
    return 12 * levels + 1;
}


/**
 * @brief  Resolve a full UNIX path to its directory entry.
 * @param  path      Full path to resolve (e.g. "/dir1/dir2/file.txt").
 * @param  len       Length of the path string.
 * @param  curr_dir  Output: directory entry of the resolved file or directory.
 * @return Sector number where the entry was found, or 0 if the path does not exist.
 * @note   Walks the directory tree from the root cluster, resolving each path component.
 *         Returns failure if any intermediate component is not a directory.
 */
static size_t fat_lookup( const char *path, size_t len, fat_sdir_entry_t *curr_dir ) {
    //TODO: Do conditional execution is len > 12 
    if ( _fat_strlen( path ) > MAX_FILE_PATH_LENGTH ) return 0;
    uint8_t levels = _fat_strcount( path, '/', len );
    uint32_t curr_clus_num = fs.root_clus_num;


    if ( levels == 0 ) return 0;

    int dospath_len = 12 * levels + 1;
    char dospath[dospath_len]; size_t sect;
    fat_path_to_dospath( dospath, path );

    for ( int dpath_ptr = 1; dpath_ptr < dospath_len; dpath_ptr += 12 ) {       // offset = dospath_len + 1 ('\')
        if ( ( sect = fat_file_is_present( &dospath[dpath_ptr], curr_dir, curr_clus_num ) ) == 0 ) return 0;
        if ( ! ( curr_dir->dir_attr & DIR_ATTR_DIRECTORY ) ) break; 
        if ( dpath_ptr + 12 == dospath_len ) break;
        curr_clus_num = CLUS_LO_PLUS_HI( curr_dir->dir_fst_clus_lo, curr_dir->dir_fst_clus_hi );
    }

#if defined(DEBUG_INFO_ENABLE)
    uint32_t sector_offset = fat_cluster_offset( curr_clus_num );
    fat_sdir_entry_t dir_entry;
    printf_( "Sector offset of the returned cluster: %d\r\n",  sector_offset );
#endif
    return sect;
}

/**
 * @brief  Walk the cluster chain forward by a given number of steps.
 * @param  file  File descriptor whose cluster chain to walk.
 * @param  off   Number of clusters to advance.
 * @param  err   Pointer to error flags (FAT_ERR_IO set on premature FAT_EOC).
 * @return The cluster number after advancing @p off steps, or 0 on error.
 */
static size_t fat_clus_lookup( fat_file_t *file, size_t off, fat_err_e *err ) {
    size_t tmp = file->curr_clus;
    for ( size_t clus_idx = 0; clus_idx < off; clus_idx++ ) {
        tmp = fat_entry( tmp );            
        if ( tmp == FAT_EOC ) {
            *err |= FAT_ERR_IO; return 0;
        } 
    }
    return tmp;
}

/**
 * @brief  Allocate a free cluster by marking it with FAT_EOC in the FAT.
 * @param  hint  Preferred starting cluster number for the search, or 0 to use FSInfo hint.
 * @return The allocated cluster number, or 0 if allocation fails.
 * @note   Searches from the hint (or FSInfo next_free) upward for a zero-valued entry.
 *         Updates the FSInfo next_free field and writes the modified sector back.
 */
static uint32_t fat_allocate_cluster( uint32_t hint ) {
    uint32_t entry;
    if ( hint > fs.cluster_count ) return 0;
    if ( hint < fs.root_clus_num ) {
        if ( *( uint32_t * )fs_info.fsi_next_free > fs.cluster_count )
            entry = fs.root_clus_num; 
        else entry = *( uint32_t * )fs_info.fsi_next_free + 1;
    } else entry = hint + 1;

    while ( 1 ) {
        if ( fat_entry( entry ) == 0 ) {
            int sect_off = fat_entry_sec_offset( entry );
            uint32_t eoc = FAT_EOC;
            _fat_memcpy( &fs.sector_buffer[sect_off], &eoc, 4 );
            _fat_memcpy( fs_info.fsi_next_free, &entry, 4 );
#if defined( DEBUG_INFO_ENABLE )
            printf_( "fat32.d:%d | FS Info next_free field changed to: %d\r\n", __LINE__, *( uint32_t * )fs_info.fsi_next_free );
#endif
            break;
        } else entry++; 
    }
    fat_put_sector( fs.last_sector_read, fs.bytes_per_sector, fs.sector_buffer );
    return entry;
}

/**
 * @brief  Get the size of an open file.
 * @param  file  Pointer to the open file descriptor.
 * @return File size in bytes.
 */
uint32_t fat32_get_file_size( fat_file_t *file ) {
    return file->dir_entry.dir_file_size;
}

/**
 * @brief  Write data to an open file.
 * @param  buffer  Pointer to the data to write.
 * @param  size    Number of bytes to write.
 * @param  file    Pointer to the open file descriptor.
 * @param  err     Pointer to error flags (set on failure).
 * @return Number of bytes written on success, 0 on failure, -1 on fsync failure.
 * @note   Rejects writes on read-only files. Automatically allocates clusters
 *         (first cluster and when current cluster is full). Handles partial
 *         sectors via char_off buffering. Calls fat32_fsync() to flush directory
 *         entry changes to disk.
 */
size_t fat32_fwrite( const void *buffer, size_t size, fat_file_t *file, fat_err_e *err ) {
    if ( file->mode & O_RDONLY ) {
        *err |= FAT_ERR_MODE_CONFILICT;
        return 0;
    }

    if ( size > fs.sectors_per_cluster * fs.bytes_per_sector ) return 0;

    if ( file->curr_clus == 0 ) {
        size_t entry = fat_allocate_cluster( 0 ); 
        if ( entry < 0 ) {
            *err |= FAT_ERR_IO;
            return 0;
        }
        file->curr_clus = entry;
        file->dir_entry.dir_fst_clus_lo = entry & 0xFFFF;
        file->dir_entry.dir_fst_clus_hi = ( entry >> 16 ) & 0xFFFF;
    }

    uint32_t mod_cluster = file->dir_entry.dir_file_size % BYTES_PER_CLUSTER;
    if ( mod_cluster == 0 || mod_cluster + size > BYTES_PER_CLUSTER ) {
        uint32_t clus_fat_entry = fat_entry( file->curr_clus );
        if ( clus_fat_entry == FAT_EOC ) {
            uint32_t clus = fat_allocate_cluster( file->curr_clus );
            if ( clus == 0 ) { *err |= FAT_ERR_UNDEFINED; return 0; }
            int sec = fat_entry_sector_num( file->curr_clus );
            if ( sec < 0 ) { *err |= FAT_ERR_UNDEFINED; return 0; }
            fat_get_sector( sec, fs.bytes_per_sector, fs.sector_buffer );
            _fat_memcpy( &fs.sector_buffer[fat_entry_sec_offset( file->curr_clus )],
                        &clus, sizeof( clus ) );
            fat_put_sector( sec, fs.bytes_per_sector, fs.sector_buffer );
        }
    } 


    size_t buf_idx = 0, sect_null = fat_cluster_offset( file->curr_clus );     // First sector of the current cluster

    if ( file->char_off > 0 ) {
        fat_get_sector( sect_null + file->sect_off, fs.bytes_per_sector, file->file_buf );
        size_t l_len = fs.bytes_per_sector - file->char_off; 
        _fat_memcpy( &file->file_buf[file->char_off], buffer, l_len );
        buf_idx += l_len; // wrote_bytes += l_len;
        file->char_off = 0;
        file->sect_off++;
    }

    while ( buf_idx < size ) {

        size_t max_in_cluster = fs.sectors_per_cluster - file->sect_off;
        size_t max_in_buf = ( size - buf_idx ) / fs.bytes_per_sector;

        size_t batch = max_in_cluster;
        if ( max_in_buf  < batch ) batch = max_in_buf;

        if ( batch > 1 ) {
            fat_put_multiple_sectors( sect_null + file->sect_off, batch, &buffer[buf_idx] );
            size_t l_len = batch * fs.bytes_per_sector;
            buf_idx += l_len; 
            file->sect_off += batch;
        } else if ( batch == 1 ) {
            fat_put_sector( sect_null + file->sect_off, fs.bytes_per_sector, &buffer[buf_idx] );
            buf_idx += fs.bytes_per_sector;
            file->sect_off++;
        } else if ( batch == 0 ) {
            size_t l_len = size - buf_idx;
            fat_put_sector( sect_null + file->sect_off, fs.bytes_per_sector, &buffer[buf_idx] );
            _fat_memcpy( file->file_buf, &buffer[buf_idx], l_len );  // sect_off is not increasing until it is fully in buffer
            buf_idx += l_len; file->char_off += l_len; 
        } else {
            *err |= FAT_ERR_UNDEFINED;
            break;
        }

        if ( file->sect_off >= fs.sectors_per_cluster ) {
            sect_null = fat_cluster_offset( file->curr_clus );
            file->sect_off = 0;
        }
    }

    if ( file->off + size > file->dir_entry.dir_file_size ) {
        file->dir_entry.dir_file_size = file->off + size;
    }

    if ( fat32_fsync( file ) < 0 ) {
#if defined ( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | Flush of new data of %s was failed \r\n", __LINE__, file->path );
#endif
        return -1;
    }

    return buf_idx;
}

/**
 * @brief  Reposition the read/write offset of an open file.
 * @param  offset  Number of bytes to seek.
 * @param  file    Pointer to the open file descriptor.
 * @param  whence  Origin: SEEK_CUR (from current position) or SEEK_SET (from start).
 * @param  err     Pointer to error flags (set on failure).
 * @return The resulting absolute offset on success, 0 on failure.
 * @note   Walks the cluster chain via fat_clus_lookup() when crossing cluster boundaries.
 *         Recalculates sector offset and character offset within the sector.
 */
size_t fat32_lseek( size_t offset, fat_file_t *file, fat_whence_e whence, fat_err_e *err ) {

    // TODO: Optimize algebraic formulas for calculation of offsets, there is some redundancy in current approach

    size_t clus_idx = 0;
    size_t sect_off = 0, char_off = 0, clus_off = 0;
    switch ( whence ) {
        case SEEK_CUR: {

            sect_off = file->sect_off + ( ( file->char_off + offset ) / fs.bytes_per_sector ); 
            char_off = ( file->char_off + offset ) % fs.bytes_per_sector;
            file->off += offset;

            if ( sect_off >= fs.sectors_per_cluster ) {
                clus_off = sect_off / fs.sectors_per_cluster;
                sect_off = sect_off - ( clus_off * fs.sectors_per_cluster );

                size_t tmp_clus = fat_clus_lookup( file, clus_off, err );
                if ( tmp_clus > 0 ) file->curr_clus = tmp_clus;
                else return 0;
            }

        } break;
        case SEEK_SET: {

            file->off = offset;
            sect_off = offset / fs.bytes_per_sector; 
            char_off = offset % fs.bytes_per_sector;
            size_t clus_bup = file->curr_clus;
            file->curr_clus = CLUS_LO_PLUS_HI ( file->dir_entry.dir_fst_clus_lo, file->dir_entry.dir_fst_clus_hi );

            if ( sect_off >= fs.sectors_per_cluster ) {
                clus_off = sect_off / fs.sectors_per_cluster;
                sect_off = sect_off - ( clus_off * fs.sectors_per_cluster );

                size_t clus_bup = file->curr_clus;   
                size_t tmp_clus = fat_clus_lookup( file, clus_off, err );   

                if ( tmp_clus < 0 ) { file->curr_clus = clus_bup; return 0; }     
                file->curr_clus = tmp_clus;
            }

        } break;
        default: {
            *err |= FAT_ERR_IO;
            return 0;
        } break;
    }
    file->sect_off = sect_off;
    file->char_off = char_off;
    return offset;
}


/**
 * @brief  Read data from an open file.
 * @param  buffer  Destination buffer.
 * @param  size    Number of bytes to read (must be a multiple of bytes_per_sector).
 * @param  file    Pointer to the open file descriptor.
 * @param  err     Pointer to error flags (set on failure).
 * @return Number of bytes read on success, 0 on error or EOF.
 * @note   Only works with O_RDONLY or O_RDWR modes. Reads are batched:
 *         full clusters use multi-block reads, partial clusters use single-block reads.
 *         Sets FAT_ERR_EOF when all file data has been consumed.
 */
size_t fat32_fread( void* buffer, size_t size, fat_file_t *file, fat_err_e *err ) {

    if ( !( file->mode & O_RDONLY ) && !( file->mode & O_RDWR ) ) {
        *err |= FAT_ERR_MODE_CONFILICT;
        return 0;
    } 

    if ( size % fs.bytes_per_sector != 0 ) {
        *err |= FAT_ERR_IO;
        return 0;
    }

    static size_t read_bytes = 0;

    size_t buf_idx = 0, sect_null = fat_cluster_offset( file->curr_clus );     // First sector of the current cluster

    if ( file->char_off > 0 ) {
        fat_get_sector( sect_null + file->sect_off, fs.bytes_per_sector, file->file_buf );
        size_t l_len = fs.bytes_per_sector - file->char_off; 
        _fat_memcpy( buffer, &file->file_buf[file->char_off], l_len );
        buf_idx += l_len; read_bytes += l_len;
        file->char_off = 0;
        file->sect_off++;
    }

    while ( buf_idx < size ) {

        size_t max_in_cluster = fs.sectors_per_cluster - file->sect_off;
        size_t max_in_buf = ( size - buf_idx ) / fs.bytes_per_sector;
        size_t max_in_file = ( file->dir_entry.dir_file_size - read_bytes ) / fs.bytes_per_sector;

        size_t batch = max_in_cluster;
        if ( max_in_buf  < batch ) batch = max_in_buf;
        if ( max_in_file < batch ) batch = max_in_file;

        if ( batch > 1 ) {
            fat_get_multiple_sectors( sect_null + file->sect_off, batch, &buffer[buf_idx] );
            size_t l_len = batch * fs.bytes_per_sector;
            buf_idx += l_len; read_bytes += l_len;
            file->sect_off += batch;
        } else if ( batch == 1 ) {
            fat_get_sector( sect_null + file->sect_off, fs.bytes_per_sector, &buffer[buf_idx] );
            buf_idx += fs.bytes_per_sector;
            read_bytes += fs.bytes_per_sector;
            file->sect_off++; 
        } else if ( batch == 0 ) {
            size_t l_len = size - buf_idx;
            fat_get_sector( sect_null + file->sect_off, fs.bytes_per_sector, file->file_buf );
            _fat_memcpy( &buffer[buf_idx], file->file_buf, l_len );  // sect_off is not increasing until it is fully in buffer
            buf_idx += l_len; file->char_off += l_len; read_bytes += l_len; 
        } else {
            *err |= FAT_ERR_UNDEFINED;
            break;
        }

        if ( file->sect_off >= fs.sectors_per_cluster ) {
            if ( ( file->curr_clus = fat_entry( file->curr_clus ) ) == FAT_EOC ) {
                *err |= FAT_ERR_EOF;
                read_bytes = 0;
                break;
            } 
            sect_null = fat_cluster_offset( file->curr_clus ); file->sect_off = 0;
        }
    }
    if ( read_bytes >= file->dir_entry.dir_file_size ) {
        *err |= FAT_ERR_EOF;
        read_bytes = 0;
    } 


    return buf_idx;
}

/**
 * @brief  Synchronize the directory entry of an open file to disk.
 * @param  file  Pointer to the open file descriptor.
 * @return 0 on success, -1 if the entry could not be found in its directory sector.
 * @note   Reads the directory sector, locates the matching entry by name,
 *         copies the in-memory directory entry over it, and writes the sector back.
 */
int fat32_fsync( fat_file_t *file ) {
    BPOINT();
    fat_get_sector( file->dir_sector, fs.bytes_per_sector, fs.sector_buffer );
    fat_sdir_entry_t *sector = ( fat_sdir_entry_t * )fs.sector_buffer;
    size_t dir_idx = 0, dir_ent_per_sector = fs.bytes_per_sector / sizeof( fat_sdir_entry_t );
    while ( _fat_strcmp ( sector[dir_idx].dir_name, file->dir_entry.dir_name, 
            sizeof( file->dir_entry.dir_name ) ) != 0 && dir_idx < dir_ent_per_sector ) {
        dir_idx++;
    }

    if ( dir_idx == dir_ent_per_sector ) return -1;
   
    _fat_memcpy( &sector[dir_idx], &file->dir_entry, sizeof( fat_sdir_entry_t ) );
    fat_put_sector( file->dir_sector, fs.bytes_per_sector, ( uint8_t *)sector );
    return 0;
}

/**
 * @brief  Close an open file descriptor.
 * @param  file  Pointer to the file descriptor to close.
 * @return Always 0.
 * @note   Clears the file mode (setting it to O_NONE) and flushes the FSInfo
 *         sector back to disk.
 */
int fat32_fclose( fat_file_t *file ) {
    file->mode = O_NONE;
    fat_put_sector( fs.fs_info_sector, fs.bytes_per_sector, ( uint8_t *)&fs_info );

    return 0;
}



/**
 * @brief  Open a file by path.
 * @param  path  Absolute path to the file (must start with '/').
 * @param  mode  File access mode flags (e.g. O_RDONLY, O_WRONLY, O_RDWR, O_OPEN).
 * @return Pointer to the file descriptor on success, NULL on failure.
 * @note   Finds a free slot in the global fd_table, resolves the path via
 *         fat_lookup(), reads the first cluster into the file buffer, and
 *         locates the last cluster for append operations.
 */
fat_file_t *fat32_fopen( const char *path, uint8_t mode ) {
    // TODO: Define mutex exectuion mode for parallel MCUs
    // TODO: Add err input parameter
    int fd = fat_find_fd();
    if ( fd < 0 ) return NULL;
    if ( path[0] != '/' ) return NULL;

    fat_sdir_entry_t sdir;
    size_t sect_num = fat_lookup( path, _fat_strlen( path ), &sdir );
    if ( sect_num == 0 ) return NULL;

    _fat_memcpy( fd_table[fd].path, ( void * )path, _fat_strlen( path ) );
    _fat_memcpy( &fd_table[fd], ( void * )&sdir, sizeof( fat_sdir_entry_t ) );
    size_t clus_off = fat_cluster_offset( CLUS_LO_PLUS_HI( sdir.dir_fst_clus_lo, sdir.dir_fst_clus_hi ) );
    if ( clus_off != 0 ) fat_get_sector( clus_off, sizeof( fd_table[fd].file_buf ), fd_table[fd].file_buf );
    fd_table[fd].curr_clus = CLUS_LO_PLUS_HI( sdir.dir_fst_clus_lo, sdir.dir_fst_clus_hi );
    fd_table[fd].sect_off = fd_table[fd].char_off = 0;   
    fd_table[fd].mode = mode; 
    fd_table[fd].dir_sector = sect_num;
    uint32_t clus_amount = fd_table[fd].dir_entry.dir_file_size / ( fs.sectors_per_cluster * fs.bytes_per_sector );
    fat_err_e err = FAT_ERR_NONE;
    BPOINT();
    fd_table[fd].last_clus = fat_clus_lookup( &fd_table[fd], clus_amount, &err );
    if ( err != FAT_ERR_NONE ) return NULL;

    return &fd_table[fd];
}

/**
 * @brief  Mount a FAT file system at the given sector offset.
 * @param  start_addr  Sector address of the boot sector.
 * @return 0 on success, -1 on validation failure, 1 on non-critical warning.
 * @note   Reads and validates the BPB (bytes per sector, sectors per cluster,
 *         reserved sectors, root entry count, media descriptor, signatures).
 *         Computes FAT geometry (FAT start, data region, root cluster, cluster count)
 *         and, for FAT32, validates the FSInfo sector signatures.
 *         Detects FAT12/FAT16/FAT32 based on cluster count.
 */
int fat32_mount( uint32_t start_addr ) {
    fat_boot_sector_t boot_sector = { 0 };

    fat_get_sector( start_addr, sizeof( fs.sector_buffer ), fs.sector_buffer );
    _fat_memcpy( &boot_sector, fs.sector_buffer, sizeof( fs.sector_buffer ) );

    if ( *( uint16_t * )boot_sector.bpb_bytes_per_sec != MMC_BLOCK_SIZE ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bytes per sector is not equal to 512, actual value: %d\r\n",
                __LINE__, *( uint16_t * )boot_sector.bpb_bytes_per_sec );
#endif
        return -1;
    }
    if ( boot_sector.bpb_sec_per_clus % 2 != 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | sector per cluster value is not multiple of 2: actual value %d\r\n",
                __LINE__, boot_sector.bpb_sec_per_clus );
#endif
        return -1;
    }
    if ( boot_sector.bpb_sec_per_clus * *( uint16_t * )boot_sector.bpb_bytes_per_sec > 32 * 1024 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_(  "fat32.c:%d | bytes per cluster > 32KB, which is not allowed. actual value:  %d\r\n",
                __LINE__, boot_sector.bpb_sec_per_clus  );
#endif
        return -1;
    }

    if ( *( uint16_t * )boot_sector.bpb_resvd_sec_cnt == 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bpb reserved sector count = 0, which is impossible\r\n", __LINE__ );
#endif
        return -1;
    } 
    if ( 32 * *( uint16_t * )boot_sector.bpb_root_ent_cnt % *( uint16_t * )boot_sector.bpb_bytes_per_sec != 0  ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | root entry count value * 32 should be even multiple of bytes per sector\r\n",
                __LINE__ );
#endif
        return -1;
    }
#if defined( MMC_FS_TYPE_FAT16 )
    if ( boot_sector.bpb_root_ent_cnt != 512 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | for fat16 fs root entry count value should be equal to 512: actual value: %d\r\n",
                __LINE__, boot_sector.bpb_root_ent_cnt );
#endif
#elif defined( MMC_FS_TYPE_FAT32 )
    if ( *( uint16_t * )boot_sector.bpb_root_ent_cnt != 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | for fat32 fs root entry count value should be equal to 0: actual value: %d\r\n",
                __LINE__, * ( uint16_t * )boot_sector.bpb_root_ent_cnt );
#endif
        return -1;
    }
#endif

    // _fat_memcpy(  boot_sector.bpb_tot_sec16, sys_buffer + OFFSET_BPB_TOT_SEC16, sizeof(  boot_sector.bpb_tot_sec16 )  );
#if defined( MMC_FS_TYPE_FAT32 ) 
    if ( *( uint16_t * )boot_sector.bpb_tot_sec16 != 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | for fat32 total amount of sectors should be 0: actual value: %d\r\n",
                __LINE__, *( uint16_t * )boot_sector.bpb_tot_sec16 );
#endif
        return 1;
    }
#endif
    if (  !( boot_sector.bpb_media == 0xF0 || boot_sector.bpb_media == 0xF8 || boot_sector.bpb_media == 0xF9 ||
           boot_sector.bpb_media == 0xFA || boot_sector.bpb_media == 0xFB || boot_sector.bpb_media == 0xFC ||
           boot_sector.bpb_media == 0xFD || boot_sector.bpb_media == 0xFE || boot_sector.bpb_media == 0xFF  ) ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | invalid bpb media value: actual value: %d\r\n",
                __LINE__, boot_sector.bpb_media );
#endif
        return 1;
    }
#if defined( MMC_FS_TYPE_FAT32 )
    if ( *( uint16_t * )boot_sector.bpb_fatsz16 != 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bpb fat size value is not 0 for fat32: actual value: %d\r\n",
                __LINE__, boot_sector.bpb_fatsz16 );
#endif
        return 1;
    }
#endif
    // TODO: add volume size check 
    // TODO: add read of the internal data register in mmc_init(  )
#if defined( MMC_FS_TYPE_FAT12 )
    if (  boot_sector.bs_boot_sig != 0x29  ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs boot signature value is not 0x29: actual value %d\r\n", __LINE__, boot_sector.bs_boot_sig );
#endif
        return 1; 
    } 
    if ( *( uint16_t * )boot_sector.bs_sign != FAT_BS_SIGN ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs signature value incorrect: actual value %d\r\n", __LINE__, *( uint16_t * )boot_sector.bs_sign );
#endif
        return 1;
    }
#endif

#if defined( MMC_FS_TYPE_FAT16 )
    if ( boot_sector.bs_boot_sig != 0x29 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs boot signature value is not 0x29: actual value %d\r\n", __LINE__, boot_sector.bs_boot_sig );
#endif
        return 1; 
    } 
    if ( *( uint16_t * )boot_sector.bs_sign != FAT_BS_SIGN ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs signature value incorrect: actual value %d\r\n", __LINE__, boot_sector.bs_sign );
#endif
        return 1;
    }
#endif

#if defined( MMC_FS_TYPE_FAT32 )
    if (  boot_sector.bs_boot_sig != 0x29  ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs boot signature value is not 0x29: actual value %d\r\n",
                __LINE__, boot_sector.bs_boot_sig );
#endif
        return 1; 
    } 
    if ( *( uint16_t * )boot_sector.bs_sign != FAT_BS_SIGN ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bs signature value incorrect: actual value %d\r\n",
                __LINE__, *( uint16_t * )boot_sector.bs_sign );
#endif
        return 1;
    }
#endif


    // Since the FAT area is next to the reserved area, its offset and size are:
    fs.fat_start_sector = start_addr + *( uint16_t * )boot_sector.bpb_resvd_sec_cnt;
    if ( *( uint16_t * )boot_sector.bpb_fatsz16 == 0 ) {
        fs.fat_sectors = *( uint32_t * )boot_sector.bpb_fatsz32 * boot_sector.bpb_num_fats;
    } else {
        fs.fat_sectors = *( uint16_t * )boot_sector.bpb_fatsz16 * boot_sector.bpb_num_fats;
    }

    // The offset and size of root directory are:
    fs.root_dir_start_sector = fs.fat_start_sector + fs.fat_sectors;
    fs.root_dir_sectors = (  32 * *( uint16_t * )boot_sector.bpb_root_ent_cnt + *( uint16_t * )boot_sector.bpb_bytes_per_sec - 1  ) / 
                            *( uint16_t * )boot_sector.bpb_bytes_per_sec;
    // NOTE: On the FAT32 volumes, root entry count is always 0 ( there is no root directory ). The data become the rest of the volume

    fs.data_start_sector = fs.root_dir_start_sector + fs.root_dir_sectors;

    if ( *( uint32_t * )boot_sector.bpb_tot_sec32 == 0 ) {
        fs.data_sectors = *( uint16_t * )boot_sector.bpb_tot_sec16 - fs.data_start_sector;
    } else {
        fs.data_sectors = *( uint32_t * )boot_sector.bpb_tot_sec32 - fs.data_start_sector;
    }

    fs.cluster_count = fs.data_sectors / boot_sector.bpb_sec_per_clus;
    if (  fs.cluster_count <= 4085  ) fs.sub_type = FAT_SUB_TYPE_FAT12;
    else if (  fs.cluster_count > 4085 && fs.cluster_count <= 65525  ) fs.sub_type = FAT_SUB_TYPE_FAT16;
    else if (  fs.cluster_count > 65525  ) fs.sub_type = FAT_SUB_TYPE_FAT32;
    // NOTE: This numbers are relative, they are not representing physical memory layout.
    // The reasons are disk partitioning and disk manufacturer reserved areas

    fs.bytes_per_sector = *( uint16_t * )boot_sector.bpb_bytes_per_sec;
    fs.sectors_per_cluster = boot_sector.bpb_sec_per_clus;
    fs.root_clus_num = *( uint32_t * )boot_sector.bpb_root_clus;
    fs.fs_info_sector = start_addr + *( uint16_t * )boot_sector.bpb_fs_info;

   
    if ( fs.sub_type == FAT_SUB_TYPE_FAT32 ) {
        fat_get_sector( fs.fs_info_sector, sizeof( fat_fsinfo_t ), (uint8_t *)&fs_info);
        if ( *( uint32_t * )fs_info.fsi_lead_sig != FAT_FSI_LEAD_SIGN ) {
#if defined( DEBUG_INFO_ENABLE ) 
        printf_("fat32.c:%d |  fsi_lead_sig is not equal to %x: actual number: %x\r\n",
               __LINE__, FAT_FSI_LEAD_SIGN, fs_info.fsi_lead_sig );
#endif
            return -1;
        }
        if ( *( uint32_t * )fs_info.fsi_struc_sig != FAT_FSI_STRUC_SIG ) {
#if defined( DEBUG_INFO_ENABLE ) 
        printf_("fat32.c:%d |  fsi_struc_sig is not equal to %x: actual number: %x\r\n",
               __LINE__, FAT_FSI_STRUC_SIG, fs_info.fsi_struc_sig );
#endif
            return -1;
        }

        if ( *( uint32_t * )fs_info.fsi_trail_sig != FAT_FSI_TRAIL_SIG ) {
#if defined( DEBUG_INFO_ENABLE ) 
        printf_("fat32.c:%d |  fsi_struc_sig is not equal to %x: actual number: %x\r\n",
               __LINE__, FAT_FSI_TRAIL_SIG, fs_info.fsi_struc_sig );
#endif
            return -1;
        }
#if defined( DEBUG_INFO_ENABLE ) 
        printf_( "fat32.c:%d | amount of free clusters: %x, next free cluster: %x\r\n",
               __LINE__, *( uint32_t * )fs_info.fsi_free_count, *( uint32_t *)fs_info.fsi_next_free );
        printf_( "fat32.c:%d | amount of free clusters: %x, total amount of clusters: %x\r\n",
               __LINE__, *( uint32_t * )fs_info.fsi_free_count, *( uint32_t * )boot_sector.bpb_fatsz32 );
        if ( fs_info.fsi_next_free > boot_sector.bpb_fatsz32 )
            printf_( "Next free cluster number is bigger than the maxim number of clusters\r\n" );
#endif
    }

    return 0;
}

/**
 * @brief  Convert a byte buffer from little-endian to big-endian in place.
 * @param  buf  Destination buffer (may be the same as src).
 * @param  src  Source buffer.
 * @param  len  Length of the data in bytes.
 * @return 0 on success, -1 if len is 0.
 * @note   Marked __unused__; not currently called in the codebase.
 */
static int __attribute__ (( __unused__ )) _fat_to_big_endian( uint8_t *buf, uint8_t *src, size_t len ) {
    // Check if len has a valid value
    if ( len == 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | unexpected input: len = 0\r\n", __LINE__ );
#endif
        return -1;
    } 

    if ( len == 1 ) {
        src[0] = buf[0];
        return 0;
    }

    // Copy values in buffer starting from the end one by one
    size_t i = 0, j = len - 1;
    while ( i < len ) buf[i++] = src[j--]; 
    return 0;
}

