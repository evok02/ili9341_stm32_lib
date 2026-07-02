#include "fat32.h"
#include "mmc.h"

#include <libopencm3/cm3/assert.h>

#define FAT_BS_SIGN           ( 0x0000AA55U )
#define FAT_FSI_LEAD_SIGN     ( 0x41615252U )
#define FAT_FSI_STRUC_SIG     ( 0x61417272U )
#define FAT_FSI_TRAIL_SIG     ( 0xAA550000U )

#define FAT_EOC               ( 0x0FFFFFFF )

#define FD_TABLE_LENGTH       ( 4 )

#define CLUS_LO_PLUS_HI( CLUS_LO, CLUS_HI ) \
        ( ( ( CLUS_HI ) << 16 ) | ( CLUS_LO ) )

static fat_file_t fd_table[FD_TABLE_LENGTH];

static int _fat_memcpy( void *restrict dest, void *const restrict src, size_t len ) {
    if ( dest == 0 || src == 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | unexpected input: null_pointer dereference\r\n", __LINE__ );
#endif
        return -1;
    } 
    for ( size_t i = 0; i < len; i++ ) ( ( uint8_t * )dest )[i] = ( ( uint8_t * )src )[i]; 
    return 0;
}

static inline uint32_t _fat_strlen( const char* str ) {
    uint32_t len = 0;
    while ( *str++ != '\0' ) len++;
    return len;
}

static inline int _fat_strcmp( const char *restrict str1, const char *restrict str2, size_t len ) {
    for ( size_t s_idx = 0; s_idx < len; s_idx++ ) {
        if ( str1[s_idx] != str2[s_idx] ) return -1;
    }
    return 0;
}


static inline uint8_t _fat_strcount( const char *in, const char c, size_t len ) {
    size_t counter = 0;
    for ( size_t idx = 0; idx < len; idx++ ) {
        if ( in[idx] == c ) counter++;
    }
    return counter;
}

static inline int _fat_strfind( const char *in, const char c, size_t len ) {
    //for ( int idx = 0; idx < len; idx++ ) {
        //if ( in[idx] == 'c' ) return idx;
    //}
    //return -1;
    size_t counter = 0;
    while ( *in++ != c ) {
        counter++;
        if ( *in == '\0' || counter > len ) break;
    }
    return counter;
}


static inline void fat_get_sector( fat_fs_t *fs, uint32_t sector, size_t length, uint8_t *data ) {
    ( void )mmc_read_single_block( sector, length, data );
    fs->last_sector_read = sector;
}

static inline void fat_get_multiple_sectors( fat_fs_t *fs, uint32_t sector, size_t length, uint8_t *data ) {
    int off = mmc_read_multiple_blocks( sector, length, data );
    fs->last_sector_read = sector + off;
}

static inline int fat_cluster_offset( fat_fs_t *fs, uint32_t cluster ) {
    if ( cluster > ( fs->data_sectors / fs->sectors_per_cluster ) ) return -1;
    return fs->data_start_sector + ( cluster - 2 ) * fs->sectors_per_cluster; 
}

static inline int fat_entry_sector_num( fat_fs_t *fs, uint32_t entry ) {
    switch ( fs->sub_type ) {
        // TODO: Reserach implementation details.
        //case FAT_SUB_TYPE_FAT12: { 
            //if ( entry < 0 || entry > ( fs->fat_sectors / 1.5f ) ) return -1; 
            //return fs->fat_start_sector + ( int )( ( entry * 1.5f ) / fs->bytes_per_sector );
        //} break;
        case FAT_SUB_TYPE_FAT16: {
            if ( entry > ( fs->fat_sectors / 2 ) ) return -1; 
            return fs->fat_start_sector + ( int )( ( entry * 2 ) / fs->bytes_per_sector );
        } break;
        case FAT_SUB_TYPE_FAT32: {
            if ( entry > ( fs->fat_sectors / 4 ) ) return -1; 
            return fs->fat_start_sector + ( int )( ( entry * 4 ) / fs->bytes_per_sector );
        } break;
        default: {
            printf_( "fat32.c:%d , Malformed FAT sub type\r\n", __LINE__ );
            return -1;
        } break;
    }
}

static inline int fat_entry_sec_offset( fat_fs_t *fs, uint32_t entry ) {
    switch ( fs->sub_type ) {
        //case FAT_SUB_TYPE_FAT12: { 
        //} break;
        case FAT_SUB_TYPE_FAT16: {
            if ( entry > ( fs->fat_sectors / 2 ) ) return -1; 
            return ( ( entry * 2 ) % fs->bytes_per_sector ); 
        } break;
        case FAT_SUB_TYPE_FAT32: {
            if ( entry > ( fs->fat_sectors / 4 ) ) return -1; 
            return ( ( entry * 4 ) % fs->bytes_per_sector ); 
        } break;
        default: {
            printf_( "fat32.c:%d | Malformed FAT sub type", __LINE__ );
            return -1;
        } break;
    }
}

// TODO: Implement cash stored value in fs_t, current approach read 512 bytes for a 4-byte value
static inline uint32_t fat_entry( fat_fs_t *fs, uint32_t entry ) {
    int sector = fat_entry_sector_num( fs, entry );
    int offset = fat_entry_sec_offset( fs, entry );
    fat_get_sector( fs, sector, sizeof( fs->sector_buffer ), fs->sector_buffer ); 
    fs->last_fat_read = entry;
    return *( uint32_t * )&fs->sector_buffer[offset] & FAT_EOC;
}



static int fat_find_fd( void ) {
    for ( int table_index = 0; table_index < FD_TABLE_LENGTH; table_index++ ) {
        if ( !(  fd_table[table_index].mode & O_OPEN  ) ) return table_index;
    }
    return -1;
}

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

// TODO: Optimize complexity, as this version really hard to debug and has an average perfomance, approach should be reconsidered
// NOTE: Check existing version of this function 
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

static int fat_file_is_present( fat_fs_t *fs, char *dir_name, fat_sdir_entry_t *dir_entry, uint32_t clus_num ) {
    uint32_t sector_offset = 0;
    do {
        sector_offset = fat_cluster_offset( fs, clus_num );
        int not_found_c = 0;
        for ( size_t sect_idx = sector_offset; sect_idx < sector_offset + fs->sectors_per_cluster; sect_idx++ ) {
            if ( not_found_c > 10 ) break;

            fat_get_sector( fs, sect_idx, sizeof( fs->sector_buffer ), fs->sector_buffer );
            if ( fat_traverse_dir_entries( fs->sector_buffer, dir_entry, dir_name ) == 0 ) {
                return 0;
            } else not_found_c++;
        }
    } while ( ( clus_num = fat_entry( fs, clus_num ) ) != FAT_EOC );
    return -1;
}

static inline int fat_calculate_dospath_length( const char *path, size_t len ) {
    uint8_t levels = _fat_strcount( path, '/', len );
    if ( levels == 0 ) return -1;
    return 12 * levels + 1;
}


static int fat_lookup(  fat_fs_t *fs, const char *path, size_t len, fat_sdir_entry_t *curr_dir ) {
    //TODO: Do conditional execution is len > 12 
    if ( _fat_strlen(path)  > MAX_FILE_PATH_LENGTH ) return -1;
    uint8_t levels = _fat_strcount( path, '/', len );
    uint32_t curr_clus_num = fs->root_clus_num;


    if ( levels == 0 ) return -1;

    int dospath_len = 12 * levels + 1;
    char dospath[dospath_len]; int op_res;
    fat_path_to_dospath( dospath, path );

    for ( int dpath_ptr = 1; dpath_ptr < dospath_len; dpath_ptr += 12 ) {       // offset = dospath_len + 1 ('\')
        if ( ( op_res = fat_file_is_present( fs, &dospath[dpath_ptr], curr_dir, curr_clus_num ) ) != - 1 ) {
            if ( curr_dir->dir_attr & DIR_ATTR_DIRECTORY ) {
                if ( dpath_ptr + 12 == dospath_len ) break;
                else curr_clus_num = CLUS_LO_PLUS_HI( curr_dir->dir_fst_clus_lo, curr_dir->dir_fst_clus_hi );
            } else break;
        } else return -1;
    }

#if defined(DEBUG_INFO_ENABLE)
    uint32_t sector_offset = fat_cluster_offset( fs, curr_clus_num );
    fat_sdir_entry_t dir_entry;
    printf_( "Sector offset of the returned cluster: %d\r\n",  sector_offset );
#endif
    return 0;
}

static size_t fat_clus_lookup( fat_fs_t *fs, fat_file_t *file, size_t off, fat_err_e *err ) {
    size_t tmp = file->curr_clus;
    for ( size_t clus_idx = 0; clus_idx < off; clus_idx++ ) {
        tmp = fat_entry( fs, tmp );            
        if ( tmp == FAT_EOC ) {
            *err |= FAT_ERR_IO; return 0;
        } 
    }
    return tmp;
}

size_t fat32_fwrite( const void* buffer, size_t size, fat_file_t *file, fat_fs_t *fs, fat_err_e *err ) {
    if ( file->mode & O_RDONLY ) {
        *err |= FAT_ERR_MODE_CONFILICT;
        return 0;
    }
    
    return 0;
}

size_t fat32_lseek( size_t offset, fat_file_t *file, fat_whence_e whence, fat_fs_t *fs, fat_err_e *err ) {

    // TODO: Optimize algebraic formulas for calculation of offsets, there is some redundancy in current approach
    size_t clus_idx = 0;
    size_t sect_off = 0, char_off = 0, clus_off = 0;
    if ( whence == SEEK_CUR ) {
        sect_off = file->sect_off + ( ( file->char_off + offset ) / fs->bytes_per_sector ); 
        char_off = ( file->char_off + offset ) % fs->bytes_per_sector;

        if ( sect_off >= fs->sectors_per_cluster ) {
            clus_off = sect_off / fs->sectors_per_cluster;
            sect_off = sect_off - ( clus_off * fs->sectors_per_cluster );

            size_t tmp_clus = fat_clus_lookup( fs, file, clus_off, err );
            if ( tmp_clus > 0 ) file->curr_clus = tmp_clus;
            else return 0;
        }
    } else if ( whence == SEEK_SET ) {
        sect_off = offset / fs->bytes_per_sector; 
        char_off = offset % fs->bytes_per_sector;
        size_t clus_bup = file->curr_clus;
        file->curr_clus = CLUS_LO_PLUS_HI ( file->dir_entry.dir_fst_clus_lo, file->dir_entry.dir_fst_clus_hi );

        if ( sect_off >= fs->sectors_per_cluster ) {
            clus_off = sect_off / fs->sectors_per_cluster;
            sect_off = sect_off - ( clus_off * fs->sectors_per_cluster );

            size_t clus_bup = file->curr_clus;   
            size_t tmp_clus = fat_clus_lookup( fs, file, clus_off, err );   

            if ( tmp_clus  > 0 ) {
                file->curr_clus = tmp_clus;
            } else {
                file->curr_clus = clus_bup;
                return 0;
            }
        }
    } else {
        *err |= FAT_ERR_IO;
        return 0;
    }
    file->sect_off = sect_off;
    file->char_off = char_off;
    return offset;
}


// TODO: Implement new version using multiple read function, mesure perfomance
size_t fat32_fread( void* buffer, size_t size, fat_file_t *file, fat_fs_t *fs, fat_err_e *err ) {

    if ( !( file->mode & O_RDONLY ) && !( file->mode & O_RDWR ) ) {
        *err |= FAT_ERR_MODE_CONFILICT;
        return 0;
    } 

    if ( size % fs->bytes_per_sector != 0 ) {
        *err |= FAT_ERR_IO;
        return 0;
    }

    static size_t read_bytes = 0;

    size_t buf_idx = 0, sect_null = fat_cluster_offset( fs, file->curr_clus );     // First sector of the current cluster

    if ( file->char_off > 0 ) {
        fat_get_sector( fs, sect_null + file->sect_off, fs->bytes_per_sector, file->file_buf );
        _fat_memcpy( buffer, &file->file_buf[file->char_off], fs->bytes_per_sector - file->char_off );
        size_t l_len = fs->bytes_per_sector - file->char_off; 
        buf_idx += l_len; read_bytes += l_len;
        file->char_off = 0;
        file->sect_off++;
    }

    while ( buf_idx < size ) {

        size_t max_in_cluster = fs->sectors_per_cluster - file->sect_off;
        size_t max_in_buf = ( size - buf_idx ) / fs->bytes_per_sector;
        size_t max_in_file = ( file->dir_entry.dir_file_size - read_bytes ) / fs->bytes_per_sector;

        size_t batch = max_in_cluster;
        if ( max_in_buf  < batch ) batch = max_in_buf;
        if ( max_in_file < batch ) batch = max_in_file;

        if ( batch > 1 ) {
            fat_get_multiple_sectors( fs, sect_null + file->sect_off, batch, &buffer[buf_idx] );
            size_t l_len = batch * fs->bytes_per_sector;
            buf_idx += l_len; read_bytes += l_len;
            file->sect_off += batch;
        } else if ( batch == 1 ) {
            fat_get_sector( fs, sect_null + file->sect_off, fs->bytes_per_sector, &buffer[buf_idx] );
            buf_idx += fs->bytes_per_sector;
            read_bytes += fs->bytes_per_sector;
            file->sect_off++;
        } else if ( batch == 0 ) {
            size_t l_len = size - buf_idx;
            fat_get_sector( fs, sect_null + file->sect_off, fs->bytes_per_sector, file->file_buf );
            _fat_memcpy( &buffer[buf_idx], file->file_buf, l_len );  // sect_off is not increasing until it is fully in buffer
            buf_idx += l_len; file->char_off += l_len; read_bytes += l_len;
        } else {
            *err |= FAT_ERR_UNDEFINED;
            break;
        }

        if ( file->sect_off >= fs->sectors_per_cluster ) {
            if ( ( file->curr_clus = fat_entry( fs, file->curr_clus ) ) == FAT_EOC ) {
                *err |= FAT_ERR_EOF;
                read_bytes = 0;
                break;
            } 
            sect_null = fat_cluster_offset( fs, file->curr_clus ); file->sect_off = 0;
        }
    }

    if ( read_bytes >= file->dir_entry.dir_file_size ) {
        *err |= FAT_ERR_EOF;
        read_bytes = 0;
    } 

    return buf_idx;
}

void fat32_fclose( fat_file_t *file ) {
    file->mode = O_NONE;
}



fat_file_t *fat32_fopen(  fat_fs_t *fs, const char *path, uint8_t mode ) {
    // TODO: Define syslock exectuion mode for parallel MCUs
    // TODO: Add err input parameter
    int fd = fat_find_fd();
    if ( fd < 0 ) return NULL;
    if ( path[0] != '/' ) return NULL;

    fat_sdir_entry_t sdir;
    if ( fat_lookup( fs, path, _fat_strlen( path ), &sdir ) != 0  ) return NULL;

    _fat_memcpy( fd_table[fd].path, ( void * )path, _fat_strlen( path ) );
    _fat_memcpy( &fd_table[fd], ( void * )&sdir, sizeof( fat_sdir_entry_t ) );
    size_t clus_off = fat_cluster_offset( fs, CLUS_LO_PLUS_HI( sdir.dir_fst_clus_lo, sdir.dir_fst_clus_hi ) );
    fat_get_sector( fs, clus_off, sizeof( fd_table[fd].file_buf ), fd_table[fd].file_buf );
    fd_table[fd].curr_clus = CLUS_LO_PLUS_HI( sdir.dir_fst_clus_lo, sdir.dir_fst_clus_hi );
    fd_table[fd].sect_off = fd_table[fd].char_off = 0;   
    fd_table[fd].mode = mode; 

    return &fd_table[fd];
}

// start addr - address of boot sector on your drive ( in sectors )
// fs - pointer to file system object
int fat32_mount( uint32_t start_addr, fat_fs_t* fs ) {
    fat_boot_sector_t boot_sector = { 0 };

    fat_get_sector( fs, start_addr, sizeof( fs->sector_buffer ), fs->sector_buffer );
    _fat_memcpy( &boot_sector, fs->sector_buffer, sizeof( fs->sector_buffer ) );


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
        printf_( "fat32.c:%d | root entry count value * 32 should be even multiple of bytes per sector\r\n", __LINE__ );
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
        printf_( "fat32.c:%d | invalid bpb media value: actual value: %d\r\n", __LINE__, boot_sector.bpb_media );
#endif
        return 1;
    }
#if defined( MMC_FS_TYPE_FAT32 )
    if ( *( uint16_t * )boot_sector.bpb_fatsz16 != 0 ) {
#if defined( DEBUG_INFO_ENABLE )
        printf_( "fat32.c:%d | bpb fat size value is not 0 for fat32: actual value: %d\r\n", __LINE__, boot_sector.bpb_fatsz16 );
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


    // Since the FAT area is next to the reserved area, its offset and size are:
    fs->fat_start_sector = start_addr + *( uint16_t * )boot_sector.bpb_resvd_sec_cnt;
    if ( *( uint16_t * )boot_sector.bpb_fatsz16 == 0 ) {
        fs->fat_sectors = *( uint32_t * )boot_sector.bpb_fatsz32 * boot_sector.bpb_num_fats;
    } else {
        fs->fat_sectors = *( uint16_t * )boot_sector.bpb_fatsz16 * boot_sector.bpb_num_fats;
    }

    // The offset and size of root directory are:
    fs->root_dir_start_sector = fs->fat_start_sector + fs->fat_sectors;
    fs->root_dir_sectors = (  32 * *( uint16_t * )boot_sector.bpb_root_ent_cnt + *( uint16_t * )boot_sector.bpb_bytes_per_sec - 1  ) / 
                            *( uint16_t * )boot_sector.bpb_bytes_per_sec;
    // NOTE: On the FAT32 volumes, root entry count is always 0 ( there is no root directory ). The data become the rest of the volume

    fs->data_start_sector = fs->root_dir_start_sector + fs->root_dir_sectors;

    if ( *( uint32_t * )boot_sector.bpb_tot_sec32 == 0 ) {
        fs->data_sectors = *( uint16_t * )boot_sector.bpb_tot_sec16 - fs->data_start_sector;
    } else {
        fs->data_sectors = *( uint32_t * )boot_sector.bpb_tot_sec32 - fs->data_start_sector;
    }

    fs->cluster_count = fs->data_sectors / boot_sector.bpb_sec_per_clus;
    if (  fs->cluster_count <= 4085  ) fs->sub_type = FAT_SUB_TYPE_FAT12;
    else if (  fs->cluster_count > 4085 && fs->cluster_count <= 65525  ) fs->sub_type = FAT_SUB_TYPE_FAT16;
    else if (  fs->cluster_count > 65525  ) fs->sub_type = FAT_SUB_TYPE_FAT32;
    // NOTE: This numbers are relative, they are not representing physical memory layout.
    // The reasons are disk partitioning and disk manufacturer reserved areas

    fs->bytes_per_sector = *( uint16_t * )boot_sector.bpb_bytes_per_sec;
    fs->sectors_per_cluster = boot_sector.bpb_sec_per_clus;
    fs->root_clus_num = *( uint32_t * )boot_sector.bpb_root_clus;

    return 0;
}

static int __attribute__( ( __unused__ ) ) _fat_to_big_endian( uint8_t *buf, uint8_t *src, size_t len ) {
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
