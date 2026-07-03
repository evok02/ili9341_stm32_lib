#ifndef INC_FAT32_H
#define INC_FAT32_H

#include "common.h"

#include "mmc.h"

typedef struct {
    uint8_t     bs_jmp_boot[3];             //  Jump instruction to the bootstrap code ( x86 instruction ) used by OS boot process.
    uint8_t     bs_oem_name[8];             //  Just a name. This string is recommended, because it is considered to minimize compatibility problems.
    uint8_t     bpb_bytes_per_sec[2];       //  Sector size in unit of byte.

    uint8_t     bpb_sec_per_clus;           //  Number of sectors per allocation unit.
                                            //  NOTE: Any value whose cluster size excees 32KB should not be used

    uint8_t     bpb_resvd_sec_cnt[2];       //  Number of sectors in reserved area.
    uint8_t     bpb_num_fats;               //  Number of FAT copies. 
                                            //  NOTE: The value of this field should always be 2.

    uint8_t     bpb_root_ent_cnt[2];        //  For the FAT12/16 volumes, this field defines number of directory entries in the root dir.
    uint8_t     bpb_tot_sec16[2];           //  Volume size, the total number of sectors of this volume in old 16-bit field.
                                            //  NOTE: For FAT32 this field should always be 0.

    uint8_t     bpb_media;                  //  Media descriptor byte.
    uint8_t     bpb_fatsz16[2];             //  Size of FAt, the number of sectors occupies by a FAT.
                                            //  NOTE: On the FAT32 volumes, it must be an invalid value 0 and bpb_fatsz32 is used instead.

    uint8_t     bpb_sec_per_trk[2];         // Number of sectors per track. This field is relevant only for media that have geometry and used for only disk BIOS of IBM PC.
    uint8_t     bpb_num_heads[2];           // Number of heads.
    uint8_t     bpb_hidd_sec[4];            // Number of hidden physical sectors preceding this FAT volume.
                                            // NOTE: This field should always be 0 if volume is located at the beginning of the storage.

    uint8_t    bpb_tot_sec32[4];            // Volume size, total number of sectors of the FAT volume in new 32-bit fields.

#if defined( MMC_FS_TYPE_FAT12 )
    uint8_t     bs_drv_num;                 // Drive number used by disk BIOS of IBM PC. This field is used in MS-DOS bootstrap
    uint8_t     bs_reserved;                // Reserved in DOS and Windows 9X. It should be set 0 when create the volume.
    uint8_t     bs_boot_sig;                // Extended boot sign ( 0x29 ). This is a signature byte indicating that the following three fields are present.
    uint8_t    bs_vol_id[4];                // Volume serial number used with bs_vol_lab to track a volume on the removable storage.
                                            // NOTE: This value is typically generated with current time and date on foramtting.

    uint8_t     bs_vol_lab[11];             // This field is the 11-byte volume valbel and it matches volume label recorded in the root directory.
                                            // FAT driver should update this field when the volume lavel in the root directory is changed.
                                            // NOTE: "NO NAME " is set if label is not present.

    uint8_t   bs_fil_sys_type[8];           // Usually "FAT12   ", "FAT16   " or "FAT     ".
                                            // NOTE: DONT USE THIS FIELD TO DETERMINE FILE SYSTME TYPE.

    uint8_t     bs_boot_code[448];          // Bootstrap program. It is platform dependent and filled with zero when not used.
    uint8_t    bs_sign[2];                  // 0xAA55. A boot signature indicating that this is a valid boot sector.

#elif defined( MMC_FS_TYPE_FAT16 )
    uint8_t     bs_drv_num;                 // Drive number used by disk BIOS of IBM PC. This field is used in MS-DOS bootstrap
    uint8_t     bs_reserved;                // Reserved in DOS and Windows 9X. It should be set 0 when create the volume.
    uint8_t     bs_boot_sig;                // Extended boot sign ( 0x29 ). This is a signature byte indicating that the following three fields are present.
    uint8_t    bs_vol_id[4];                // Volume serial number used with bs_vol_lab to track a volume on the removable storage.
                                            // NOTE: This value is typically generated with current time and date on foramtting.

    uint8_t     bs_vol_lab[11];             // This field is the 11-byte volume valbel and it matches volume label recorded in the root directory.
                                            // FAT driver should update this field when the volume lavel in the root directory is changed.
                                            // NOTE: "NO NAME " is set if label is not present.

    uint8_t   bs_fil_sys_type[8];           // Usually "FAT12   ", "FAT16   " or "FAT     ".
                                            // NOTE: DONT USE THIS FIELD TO DETERMINE FILE SYSTME TYPE.

    uint8_t     bs_boot_code[448];          // Bootstrap program. It is platform dependent and filled with zero when not used.
    uint8_t    bs_sign[2];                  // 0xAA55. A boot signature indicating that this is a valid boot sector.

#elif defined( MMC_FS_TYPE_FAT32 )
    uint8_t     bpb_fatsz32[4];             // Size of FAT in unit of sector.
    uint8_t     bpb_ext_flags[2];           /* Bit 3-0: Active FAT starting from 0. Valid when bit7 is 1.
                                               Bit 6-4: Reserved ( 0 ). 
                                               Bit 7: 0 means that each FAT are active and mirrored. 1 means that only one FAT indicated by bit 3-0 is active.
                                               Bit 15-8-4: Reserved ( 0 ). */
    uint8_t     bpb_fs_ver[2];              // FAT32 version. Upper byte is major version number and lower byte is minor version number.
    uint8_t     bpb_root_clus[4];           // First cluster number of the root dir. It is usually set to 2, the first cluster of the volume.
    uint8_t     bpb_fs_info[2];             // Sector of FSInfo structure in offset from top of the FAT32 volume. Usually set to 1 next to the boot sector.
    uint8_t     bpb_bk_boot_sec[2];         // Sector of backup boot sector in offset from top of the FAT32 volume. Usually set to 6, next to the boot sector.
    uint8_t     bpb_reserved[12];           // Reserved ( 0 );
    uint8_t     bs_drv_num;                 // Drive number used by disk BIOS of IBM PC. This field is used in MS-DOS bootstrap
    uint8_t     bs_reserved;                // Reserved in DOS and Windows 9X. It should be set 0 when create the volume.
    uint8_t     bs_boot_sig;                // Extended boot sign ( 0x29 ). This is a signature byte indicating that the following three fields are present.
    uint8_t     bs_vol_id[4];               // Volume serial number used with bs_vol_lab to track a volume on the removable storage.
                                            // NOTE: This value is typically generated with current time and date on foramtting.

    uint8_t     bs_vol_lab[11];             // This field is the 11-byte volume valbel and it matches volume label recorded in the root directory.
                                            // FAT driver should update this field when the volume lavel in the root directory is changed.
                                            // NOTE: "NO NAME " is set if label is not present.

    uint8_t     bs_fil_sys_type[8];         // Usually "FAT12   ", "FAT16   " or "FAT     ".
                                            // NOTE: DONT USE THIS FIELD TO DETERMINE FILE SYSTME TYPE.

    uint8_t     bs_boot_code32[420];        // Bootstrap program. It is platform dependent and filled with zero when not used.
    uint8_t     bs_sign[2];                 // 0xAA55. A boot signature indicating that this is a valid boot sector.
#endif

} __attribute__( (  __packed__  ) ) fat_boot_sector_t;

#if defined( MMC_FS_TYPE_FAT32 )
typedef struct {
    uint8_t     fsi_lead_sig[4];             // 0x41615252. This is a lead signature used to validate that this is in fact and FSInfo sector.
    uint8_t     fsi_reserved[480];
    uint8_t     fsi_struc_sig[4];            // 0x61417272. Another signature that is more localized in the sector to the location of the fields that are used.
    uint8_t     fsi_free_count[4];           // This field indicated the last know free cluster count on the volume.
                                             // NOTE: if the value is 0xFFFFFFFF, it is actually unknown.
    uint8_t     fsi_next_free[4];            // The cluster number at which the driver should start looking for free clusters.
    uint8_t     fsi_reserved2[12];           // Reserved.
    uint8_t     fsi_trail_sig[4];            // 0xAA550000. This trail signature is used to validate taht this is in fact an FSInfo sector.
} __attribute__( ( __packed__ ) ) fat32_fs_info_t;
#endif

typedef enum {
    FAT_SUB_TYPE_FAT12,
    FAT_SUB_TYPE_FAT16,
    FAT_SUB_TYPE_FAT32,
} fat_sub_type_e;

typedef struct {
    uint32_t        fat_start_sector;           // Index of the first FAT sector
    uint32_t        fat_sectors;                // Number of sectors in FAT
    uint32_t        root_dir_start_sector;      // Index of the first root directory sector
                                                // NOTE: For FAT32 first root sector index equal to fist data sector index
    uint32_t        root_dir_sectors;           // Number of sectors in root directory
    uint32_t        data_start_sector;          // Index of the first data sectrion sector
    uint32_t        data_sectors;               // Number of data sectors 
    uint32_t        cluster_count;              // Total number of allocatiion units
    fat_sub_type_e  sub_type;                   // Sub type of FAT file system
    uint32_t        last_fat_read;              // Index of the last fat entry that was read
    uint32_t        last_sector_read;           // Index of the last fat entry that was read
    uint32_t        last_sector_wrote;          // Index of the last fat entry that was read
    uint32_t        root_clus_num;              
    uint32_t        fs_info_sector;             // Sector number of fs_info
    uint8_t         sector_buffer[512];         // Sector buffer

    uint16_t        bytes_per_sector;           // Amount of bytes per sector
    uint8_t         sectors_per_cluster;        // Amount of sectors per allocation unit

} __attribute__( ( __aligned__( 8 ) ) ) fat_fs_t;

#if defined( MMC_FS_TYPE_FAT32 )
typedef struct {
    uint32_t    fsi_lead_sig;               // This is a lead signature used to validate that this is in fact an FSInfo sector.
    uint8_t     fsi_reserved1[480];         // This field should be always initialized to 0.
    uint32_t    fsi_struc_sig;              // Another signature that is more localized in the sector to the location of the fields that are used.
    uint32_t    fsi_free_count;             // This field indicates the last known free cluster count on the volume.
                                            // NOTE: If the value is 0xFFFFFFFF it is actually unknown.

    uint32_t    fsi_nxt_free;               // This field gives a hint for the driver.
                                            // NOTE: In best case this value represent where driver should start looking at for the empty space.
                                            // NOTE: If the value is 0xFFFFFFFF it is actually unknown, driver should start looking at cluster 2.

    uint8_t     fsi_reserved2[12];          // This field should always be initialized to 0.
    uint32_t    fsi_trail_sig;              // This trail signature is used to validate that this is in fact an FSInfo sector.
} __attribute__( (  __packed__  ) ) fat_fsinfo_t;
#endif

typedef enum {
    DIR_ATTR_READ_ONLY      = 0x01U,         // Read only file
    DIR_ATTR_HIDDEN         = 0x02U,         // Normal directory listing should not show this file
    DIR_ATTR_SYSTEM         = 0x04U,         // Indicates this is a system-dependent file
    DIR_ATTR_VOLUME_ID      = 0x08u,         // An entry with this attribute has the volume label of the volume.
                                            // NOTE: Only one entry can exist in the root directory.
                                            // NOTE: dir_fst_clus_hi, dir_fst_clus_lo and dir_file_size must be always zero. 

    DIR_ATTR_DIRECTORY      = 0x10U,         // Indicates this a container of a directory
    DIR_ATTR_ARCHIVE        = 0x20U,         // This is for a backup utilities.
    DIR_ATT_LONG_FILE_NAME  = 0x0FU,         // This combination of attributes indicates the entry is a part of long file name.
} __attribute__( (  __packed__  ) ) dir_attr_e;

typedef struct {
    char        dir_name[11];           // Short file name of the object.
    dir_attr_e  dir_attr;               // File attribute in combination of following flags.
                                        // NOTE: Upper 2 bits are reserved and must be zero.

    uint8_t     dir_ntres;              // Optional flags that indicates case information of the SFN.
                                        // NOTE: 0x08 - every alphabet in the body is low-case
                                        // NOTE: 0x10 - every alpahbet in the extension is low-case

    uint8_t     dir_crt_time_tenth;     // Optional sub-second inforamtion corresponds to dir_crt_time
    uint16_t    dir_crt_time;           // Optional file creation time.
    uint16_t    dir_crt_date;           // Optional file cretation date.
    uint16_t    dir_lst_acc_date;       // Optional last access date.
    uint16_t    dir_fst_clus_hi;        // Upper part of cluster number. 
                                        // NOTE: always zero on FAT12/16.

    uint16_t    dir_wrt_time;           // Last time when any chnage is made to the file.
    uint16_t    dir_wrt_date;           // Last time when any chnage is made to the file.
    uint16_t    dir_fst_clus_lo;        // Lower part of cluster number. When the file size is zero, no cluster is assigned and this item must be zero.
                                        // NOTE: Always a valid value if it is a directory.

    uint32_t    dir_file_size;          // Size of file in unit of byte, Not used when it is a directory and the value must be always zero.
} __attribute__( (  __packed__  ) ) fat_sdir_entry_t;

typedef struct {
    uint8_t     ldir_ord;               // Sequence number ( 1 - 20 ) to identirfy where this entry is in the sequence of LFN entries to compose an LFN.  
    uint8_t     ldir_name1[10];         // Part of LFN from 1st character to 5th character.
    dir_attr_e  ldir_attr;              // LFN attribute. Always ATTR_LONG_NAME and it indicates this is an LFN entry.
    uint8_t     ldir_type;              // Must be zero.
    uint8_t     ldir_chksum;            // Checksum of the SFN entry associated with this entry.
    uint8_t     ldir_name2[12];         // Part of LFN from 6th character to 11th character.
    uint16_t    ldir_fst_clus_lo;       // Must be zero to avoid any wrong repair by old disk utility.
    uint32_t    ldir_name3;             // Part of LFN from 12th character to 13th character.
} fat_ldir_entry_t;

#define MAX_FILE_PATH_LENGTH ( 256U )
#define MAX_FILE_NAME_LENGTH ( 64U )

typedef enum {
    O_NONE              = 0x00U,
    O_OPEN              = 0x01U,
    O_RDONLY            = 0x02U,
    O_APPEND            = 0x04U,
    O_WRONLY            = 0x08U,
    O_RDWR              = 0x10U,
    O_CREAT             = 0x20U,
    O_TRUNC             = 0x40U,
} __attribute__( ( __packed__ ) ) fat_fflag_t;

typedef struct {
    fat_sdir_entry_t    dir_entry;
    size_t              dir_sector;
    char                path[MAX_FILE_PATH_LENGTH];
    uint8_t             file_buf[512];
    uint16_t            _file_buf_idx;
    size_t              sect_off;
    size_t              char_off;
    size_t              curr_clus;
    fat_fflag_t         mode;
} fat_file_t;

typedef enum {
    FAT_ERR_NONE                = 0,
    FAT_ERR_EOF                 = 1,
    FAT_ERR_BUF_OVERFLOW        = 2,
    FAT_ERR_MODE_CONFILICT      = 4,
    FAT_ERR_BAD_FILE_SIZE       = 8,
    FAT_ERR_NULL_POINTER        = 16,
    FAT_ERR_IO                  = 32,
    FAT_ERR_UNDEFINED           = 64,
    FAT_ERR_FILE_NAME           = 128,
} fat_err_e;

typedef enum {
    SEEK_CUR,
    SEEK_SET,
} fat_whence_e;

int fat32_mount( uint32_t start_addr );
fat_file_t *fat32_fopen( const char *path, uint8_t mode );
int fat32_fclose( fat_file_t *file );
size_t fat32_fread( void* buffer, size_t size, fat_file_t *file, fat_err_e *err );
size_t fat32_fwrite( const void* buffer, size_t size, fat_file_t *file, fat_err_e *err );
size_t fat32_lseek( size_t offset, fat_file_t *file, fat_whence_e whence, fat_err_e *err );

#endif
