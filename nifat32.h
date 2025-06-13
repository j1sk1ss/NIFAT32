#ifndef NIFAT32_H_
#define NIFAT32_H_

#include "include/null.h"
#include "include/hamming.h"
#include "include/threading.h"
#include "include/checksum.h"
#include "include/fatname.h"
#include "include/fat.h"
#include "include/fatinfo.h"
#include "include/mm.h"
#include "include/logging.h"
#include "include/cluster.h"
#include "include/disk.h"
#include "include/str.h"
#include "include/entry.h"
#include "include/ctable.h"

/* Bpb taken from http://wiki.osdev.org/FAT */
typedef struct fat_extBS_32 {
    unsigned int   table_size_32;
    unsigned short extended_flags;
    unsigned short fat_version;
    unsigned int   root_cluster;
    unsigned short fat_info;
    unsigned short backup_BS_sector;
    unsigned char  reserved_0[12];
    unsigned char  drive_number;
    unsigned char  reserved_1;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    unsigned char  volume_label[11];
    unsigned char  fat_type_label[8];
    checksum_t     checksum;
} __attribute__((packed)) fat_extBS_32_t;

typedef struct fat_BS {
    unsigned char  bootjmp[3];
    unsigned char  oem_name[8];
    unsigned short bytes_per_sector;
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sector_count;
    unsigned char  table_count;
    unsigned short root_entry_count;
    unsigned short total_sectors_16;
    unsigned char  media_type;
    unsigned short table_size_16;
    unsigned short sectors_per_track;
    unsigned short head_side_count;
    unsigned int   hidden_sector_count;
    unsigned int   total_sectors_32;
    fat_extBS_32_t extended_section;
    checksum_t     checksum;
} __attribute__((packed)) fat_BS_t;

/*
https://en.wikipedia.org/wiki/Golden_ratio
2^32 / φ, where φ = +-1.618
*/
#define HASH_CONST 2654435761U
#define PRIME1     73856093U
#define PRIME2     19349663U
#define PRIME3     83492791U
#define GET_BOOTSECTOR(number, total_sectors) ((((number) * PRIME1 + PRIME2) * PRIME3) % (total_sectors))

/*
Init function. 
Note: This function also init memory manager.
Note 2: For noise-immunity purpuses for initialization of NIFAT32 we
should know end count of sectors.
Params:
- bs_num - Bootsector number.
- ts - Total sectors in filesystem.

Return 1 if init success.
Return 0 if init was interrupted by error.
*/
int NIFAT32_init(int bs_num, unsigned int ts);

/*
Return 1 if content exists.
Return 0 if content not exists.
*/
int NIFAT32_content_exists(const char* path);

// Mode flags (bitmask)
#define RW_MODE    0b00000001 /* Read-write mode */
#define CR_MODE    0b00000010 /* Create everything mode */
#define CR_RW_MODE 0b00000011 /* Create + read/write mode (combination of CR_MODE | RW_MODE) */
#define DIR_MODE   0b00000100 /* Directory mode */
#define FILE_MODE  0b00001000 /* File mode */

// Macro to combine mode and target into a single byte
// Assumes mode uses lower 4 bits and target uses upper 4 bits
#define MODE(mode, target) ((target << 4) | (mode & 0x0F))
#define DF_MODE MODE(0, 0) /* Default mode - do nothing special */

// Macro to extract mode (lower 4 bits)
#define GET_MODE(combined) (combined & 0x0F)

// Macro to extract target (upper 4 bits)
#define GET_MODE_TARGET(combined) ((combined >> 4) & 0x0F)
/*
Open content to content table.
Params:
- path - Path to content (dir or file).
- mode - Content open mode.

Return content index or negative error code.
*/
ci_t NIFAT32_open_content(const char* path, unsigned char mode);

/*
Get summary info about content.
Params:
- ci - Content index.
- info - Pointer to info struct that will be filled by info.

Return 1 if stat success.
Return 0 if something goes wrong.
*/
int NIFAT32_stat_content(const ci_t ci, cinfo_t* info);

/*
Change meta data of content.
Note: This function will change creation date, file and extention.
Note 2: This function can't change filesize and base cluster.
Params:
- ci - Targer content index.
- info - New meta data.
         Note: required fields is file_name, file_extension and type.

Return 1 if change success.
Return 0 if something goes wrong.
*/
int NIFAT32_change_meta(const ci_t ci, const cinfo_t* info);

/*
Read data from content to buffer. 
Note: This function don't check buffer and it's address.
Note 2: If offset larger then content size, function will return buff_size.
Note 3: If ci is ci of directory, will return raw directory info. For working with this info
use directory_entry_t.
Params:
- ci - Target content index.
- offset - Offset in content.
- buffer - Pointer where function should store data.
- buff_size - Buffer size.

Return count of bytes that was readden by function.
*/
int NIFAT32_read_content2buffer(const ci_t ci, cluster_offset_t offset, buffer_t buffer, int buff_size);

/*
Write data from buffer to content. 
Note: This function don't check buffer and it's address.
Note 2: If offset larger then content size, function will return data_size.
Params:
- ci - Target content index.
- offset - Offset in content.
- da - Pointer to source data.
- data_size - Data size.

Return count of bytes that was written by function.
*/
int NIFAT32_write_buffer2content(const ci_t ci, cluster_offset_t offset, const_buffer_t data, int data_size);

/*
Close content from table and release all resources.
Params:
- ci - Content index.

Return 1 if close was success.
Return 0 if something goes wrong.
*/
int NIFAT32_close_content(ci_t ci);

#define PUT_TO_ROOT -1
#define NO_RESERVE  1
/*
Add content to target content index. 
Note: PUT_TO_ROOT will put content into the root directory.
Params:
- ci - Root content index. Should be directory.
       Note: Can be PUT_TO_ROOT.
- info - Pointer to info about new content.
- reserve - Reserved cluster count for content. 
            Note: This option can be NO_RESERVE.
            Note 2: Will reserve cluster chain for defragmentation prevent.

Return 1 if operation was success.
Return 0 if something goes wrong.
*/
int NIFAT32_put_content(const ci_t ci, cinfo_t* info, int reserve);

/*
Delete content by content index.
Note: This function will close this content.
Params:
- ci - Content index.

Return 1 if delete success.
Return 0 if something goes wrong.
*/
int NIFAT32_delete_content(ci_t ci);

#endif