#ifndef ENTRY_H_
#define ENTRY_H_

#include "mm.h"
#include "logging.h"
#include "hamming.h"
#include "fatinfo.h"
#include "cluster.h"
#include "fatname.h"
#include "checksum.h"

#define FILE_LAST_LONG_ENTRY 0x40
#define ENTRY_FREE           0xE5
#define ENTRY_END            0x00
#define ENTRY_JAPAN          0x05
#define LAST_LONG_ENTRY      0x40

#define FILE_READ_ONLY 0x01
#define FILE_HIDDEN    0x02
#define FILE_SYSTEM    0x04
#define FILE_VOLUME_ID 0x08
#define FILE_DIRECTORY 0x10
#define FILE_ARCHIVE   0x20

/* from http://wiki.osdev.org/FAT */
/* From file_system.h (CordellOS brunch FS_based_on_FAL) */

typedef struct directory_entry {
    unsigned char  file_name[11];
    checksum_t     name_hash;
    unsigned char  attributes;
    cluster_addr_t cluster;
    unsigned int   file_size;
    checksum_t     checksum;
} __attribute__((packed)) directory_entry_t;

int create_entry(
    const char* fullname, char is_dir, cluster_addr_t first_cluster, 
    unsigned int file_size, directory_entry_t* entry, fat_data_t* fi
);

int entry_search(const char* name, cluster_addr_t ca, directory_entry_t* meta, fat_data_t* fi);
int entry_add(cluster_addr_t ca, directory_entry_t* meta, fat_data_t* fi);
int entry_edit(cluster_addr_t ca, const char* name, const directory_entry_t* meta, fat_data_t* fi);
int entry_remove(cluster_addr_t ca, const char* name, fat_data_t* fi);

#endif