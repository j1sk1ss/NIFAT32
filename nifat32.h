#ifndef NIFAT32_H_
#define NIFAT32_H_

#include <stddef.h>
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

#define FILE_READ_ONLY 0x01
#define FILE_HIDDEN    0x02
#define FILE_SYSTEM    0x04
#define FILE_VOLUME_ID 0x08
#define FILE_DIRECTORY 0x10
#define FILE_ARCHIVE   0x20

#define FILE_LAST_LONG_ENTRY 0x40
#define ENTRY_FREE           0xE5
#define ENTRY_END            0x00
#define ENTRY_JAPAN          0x05
#define LAST_LONG_ENTRY      0x40

#define CONTENT_TABLE_SIZE	50

/* Content Index - ci */
typedef int ci_t;
typedef unsigned int checksum_t;

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
	checksum_t checksum;
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
	unsigned char  extended_section[sizeof(fat_extBS_32_t)];
	checksum_t checksum;
} __attribute__((packed)) fat_BS_t;

/* from http://wiki.osdev.org/FAT */
/* From file_system.h (CordellOS brunch FS_based_on_FAL) */

typedef struct directory_entry {
	unsigned char  file_name[11];
	unsigned char  attributes;
	unsigned char  reserved0;
	cluster_addr_t cluster;
	unsigned int   file_size;
	checksum_t checksum;
} __attribute__((packed)) directory_entry_t;

typedef struct fat_file {
	char name[8];
	char extension[4];
	cluster_addr_t data_head;
	struct FATFile* next;
} file_t;

typedef struct fat_directory {
	char name[11];
	unsigned int next_cluster;
	unsigned int files_cluster;
	unsigned int subdir_cluster;

	struct FATDirectory* next;
	struct FATFile*      files;
	struct FATDirectory* sub_directory;
} directory_t;

typedef enum {
	CONTENT_TYPE_FILE,
	CONTENT_TYPE_DIRECTORY
} content_type_t;

typedef struct {
	union {
		directory_t* directory;
		file_t*      file;
	};
	
	cluster_addr_t parent_cluster;
	directory_entry_t meta;
	content_type_t content_type;
	lock_t lock;
} content_t;

#define NOT_PRESENT	0x00
#define STAT_FILE	0x01
#define STAT_DIR	0x02

typedef struct {
	char full_name[12];
	char file_name[8];
	char file_extension[4];
	int  type;
	int  size;
	unsigned short creation_time;
	unsigned short creation_date;
	unsigned short last_accessed;
	unsigned short last_modification_time;
	unsigned short last_modification_date;
} cinfo_t;

/*
Init function. 
Note: This function also init memory manager.
Params:
- bs - Bootsector.

Return 1 if init success.
Return 0 if init was interrupted by error.
*/
#define DEFAULT_BS  0
#define RESERVE_BS1 6
#define RESERVE_BS2 12
#define NO_RESERVE	999
int NIFAT32_init(sector_addr_t bs);

/*
Return 1 if content exists.
Return 0 if content not exists.
*/
int NIFAT32_content_exists(const char* path);

/*
Open content to content table.
Params:
- path - Path to content (dir or file).

Return content index or negative error code.
*/
ci_t NIFAT32_open_content(const char* path);

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
*/
int NIFAT32_read_content2buffer(const ci_t ci, unsigned int offset, buffer_t buffer, int buff_size);

/*
*/
int NIFAT32_write_buffer2content(const ci_t ci, unsigned int offset, const_buffer_t data, int data_size);

/*
*/
int NIFAT32_close_content(ci_t ci);

/*
*/
#define PUT_TO_ROOT -1
int NIFAT32_put_content(const ci_t ci, cinfo_t* info);

/*
*/
int NIFAT32_delete_content(ci_t ci);

#endif