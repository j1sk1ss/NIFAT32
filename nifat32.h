#ifndef NIFAT32_H_
#define NIFAT32_H_

#include "include/fat.h"
#include "include/fatinfo.h"
#include "include/mm.h"
#include "include/logging.h"
#include "include/disk.h"

#include <stddef.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


#define SECTOR_OFFSET 23000

#define CLEAN_EXIT_BMASK_16 0x8000
#define HARD_ERR_BMASK_16   0x4000
#define CLEAN_EXIT_BMASK_32 0x08000000
#define HARD_ERR_BMASK_32   0x04000000

#define FILE_LONG_NAME 	    (FILE_READ_ONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_VOLUME_ID)
#define FILE_LONG_NAME_MASK (FILE_READ_ONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_VOLUME_ID | FILE_DIRECTORY | FILE_ARCHIVE)

#define FILE_LAST_LONG_ENTRY 0x40
#define ENTRY_FREE           0xE5
#define ENTRY_END            0x00
#define ENTRY_JAPAN          0x05
#define LAST_LONG_ENTRY      0x40

#define LOWERCASE_ISSUE	  0x01
#define BAD_CHARACTER	  0x02
#define BAD_TERMINATION   0x04
#define NOT_CONVERTED_YET 0x08
#define TOO_MANY_DOTS     0x10

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

#define LOWERCASE_ISSUE	  0x01
#define BAD_CHARACTER	  0x02
#define BAD_TERMINATION   0x04
#define NOT_CONVERTED_YET 0x08
#define TOO_MANY_DOTS     0x10

#define GET_CLUSTER_FROM_ENTRY(x, fat_type)  (x.low_bits | (x.high_bits << (fat_type / 2)))
#define GET_CLUSTER_FROM_PENTRY(x, fat_type) (x->low_bits | (x->high_bits << (fat_type / 2)))

#define GET_ENTRY_LOW_BITS(x, fat_type)           ((x) & ((1 << (fat_type / 2)) - 1))
#define GET_ENTRY_HIGH_BITS(x, fat_type)          ((x) >> (fat_type / 2))
#define CONCAT_ENTRY_HL_BITS(high, low, fat_type) ((high << (fat_type / 2)) | low)

#define CONTENT_TABLE_SIZE	50

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
	unsigned char  extended_section[54];
} __attribute__((packed)) fat_BS_t;

/* from http://wiki.osdev.org/FAT */
/* From file_system.h (CordellOS brunch FS_based_on_FAL) */

typedef struct directory_entry {
	unsigned char  file_name[11];
	unsigned char  attributes;
	unsigned char  reserved0;
	unsigned char  creation_time_tenths;
	unsigned short creation_time;
	unsigned short creation_date;
	unsigned short last_accessed;
	unsigned short high_bits;
	unsigned short last_modification_time;
	unsigned short last_modification_date;
	unsigned short low_bits;
	unsigned int   file_size;
} __attribute__((packed)) directory_entry_t;

typedef struct fat_file {
	char name[8];
	char extension[4];
	unsigned int data_head;
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
	
	unsigned int parent_cluster;
	directory_entry_t meta;
	content_type_t content_type;
} content_t;

#endif