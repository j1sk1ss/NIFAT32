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
#include "include/ecache.h"
#include "include/entry.h"
#include "include/ctable.h"

/* Bpb taken from http://wiki.osdev.org/FAT */
typedef struct fat_extBS_32 {
    unsigned int   table_size_32;
    unsigned short extended_flags;
    unsigned short fat_version;
    unsigned int   root_cluster;
    unsigned char  drive_number;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    unsigned char  volume_label[11];
    unsigned char  fat_type_label[8];
    checksum_t     checksum;
} __attribute__((packed)) nifat32_ext32_bootsector_t;

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
    nifat32_ext32_bootsector_t extended_section;
    checksum_t     checksum;
} __attribute__((packed)) nifat32_bootsector_t;

#define NO_CACHE   0b00000000
#define CACHE      0b00000001
#define HARD_CACHE 0b00000010
typedef struct {
    char fat_cache;
    int bs_num;
    unsigned int ts;
} nifat32_params;

#define BOOT_MULTIPLIER 2654435761U   // Knuth's multiplier (2^32 / φ)
#define GET_BOOTSECTOR(n, ts) (((((n) + 1) * BOOT_MULTIPLIER) >> 11) % ts)

/*
Init function. 
Note: This function also init memory manager.
Note 2: For noise-immunity purpuses for initialization of NIFAT32 we
should know end count of sectors.
Params:
- params - NIFAT32 setup params.

Return 1 if init success.
Return 0 if init was interrupted by error.
*/
int NIFAT32_init(nifat32_params* params);

/*
Unload sequence. Perform all cleanup tasks.
Return 1.
*/
int NIFAT32_unload();

/*
Return 1 if content exists.
Return 0 if content not exists.
*/
int NIFAT32_content_exists(const char* path);

/* Open mode flags */
#define R_MODE     0b0001  // Read mode
#define W_MODE     0b0010  // Write mode
#define CR_MODE    0b0100  // Create mode

/* Create target flags */
#define NO_TARGET   0b0000
#define FILE_TARGET 0b0001
#define DIR_TARGET  0b0010

/* Pack mode */
#define MODE(mode, target) (((target & 0b1111) << 4) | (mode & 0b1111))

/* Default mode (RW) */
#define DF_MODE MODE((R_MODE | W_MODE), NO_TARGET)

/* Unpack macro */
#define GET_MODE(byte)        ((byte) & 0b1111)
#define GET_MODE_TARGET(byte) (((byte) >> 4) & 0b1111)
#define IS_READ_MODE(byte)    (GET_MODE(byte) & R_MODE)
#define IS_WRITE_MODE(byte)   (GET_MODE(byte) & W_MODE)
#define IS_CREATE_MODE(byte)  (GET_MODE(byte) & CR_MODE)

#define NO_RCI -1
/*
Open content to content table.
Params:
- rci - Root content index. If we don't want to search in entire file system.
        Note: By default use NO_RCI
- path - Path to content (dir or file).
- mode - Content open mode.
         Note: If mode is CR_MODE, function will create all directories in path.
         For last entry in path will use DIR_ or FILE_ MODE. 

Return content index or negative error code.
*/
ci_t NIFAT32_open_content(const ci_t rci, const char* path, unsigned char mode);

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
Trancate content will change occupied size of content.
Note: Will save data in result clusters.
Params:
- ci - Target content index.
- offset - Trancate offset in bytes.
- size - Result size of file in bytes.

Return 1 if operation success.
Return 0 if simething goes wrong.
*/
int NIFAT32_truncate_content(const ci_t ci, cluster_offset_t offset, int size);

/*
Index content directory for improving search speed.
- ci - Content index.

Return 1 if index was success.
Return 0 if something goes wrong.
*/
int NIFAT32_index_content(const ci_t ci);

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

#define DEEP_COPY    0x01
#define SHALLOW_COPY 0x02
/*
Copy content data to destination place.
Note: deep parametr can set copy type: 
- DEEP_COPY copy all data from source with new cluster creation in destination.
- SHALLOW_COPY create link in dst to src data.
Note 2: NIFAT32_copy_content will deallocate all previous data in dst.
Params:
- src - Source content index.
- dst - Destination content index.
- deep - Copy type.

Return 1 if copy success.
Return 0 if something goes wrong.
*/
int NIFAT32_copy_content(const ci_t src, const ci_t dst, char deep);

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