#ifndef FORMATTER_H_
#define FORMATTER_H_

#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "include/options.h"

#define GET_BIT(b, i) ((b >> i) & 1)
#define SET_BIT(n, i, v) (v ? (n | (1 << i)) : (n & ~(1 << i)))
#define TOGGLE_BIT(b, i) (b ^ (1 << i))

/*
https://en.wikipedia.org/wiki/Golden_ratio
2^32 / φ, where φ = +-1.618
*/
#define BS_BACKUPS 5

#define HASH_CONST 2654435761U
#define PRIME1     73856093U
#define PRIME2     19349663U
#define PRIME3     83492791U
#define GET_BOOTSECTOR(number, total_sectors) ((((number) * PRIME1 + PRIME2) * PRIME3) % (total_sectors))

#define BYTES_PER_SECTOR 	512
#define SECTORS_PER_CLUSTER 8
#define CLUSTER_SIZE 		(BYTES_PER_SECTOR * SECTORS_PER_CLUSTER)
#define RESERVED_SECTORS 	32
#define FAT_COUNT 			3
#define ROOT_DIR_CLUSTER 	2
#define DEFAULT_VOLUME_SIZE 64
#define FAT_ENTRY_FREE 		0x00000000
#define FAT_ENTRY_END 		0x0FFFFFFF
#define FAT_ENTRY_BAD 		0x0FFFFFF7
#define FAT_ENTRY_RESERVED 	0x0FFFFFF8
#define DIR_ENTRY_SIZE 		32

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
	unsigned int   checksum;
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
	unsigned int   checksum;
} __attribute__((packed)) fat_BS_t;

/* from http://wiki.osdev.org/FAT */
/* From file_system.h (CordellOS brunch FS_based_on_FAL) */

typedef struct directory_entry {
	unsigned char  file_name[11];
	unsigned char  attributes;
	unsigned int   cluster;
	unsigned int   file_size;
	unsigned int   checksum;
} __attribute__((packed)) directory_entry_t;

typedef unsigned char byte_t;
typedef unsigned short encoded_t;
typedef unsigned short decoded_t;
typedef unsigned int* fat_table_t;

#endif