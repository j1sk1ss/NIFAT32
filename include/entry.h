#ifndef ENTRY_H_
#define ENTRY_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "mm.h"
#include "ecache.h"
#include "logging.h"
#include "hamming.h"
#include "fatinfo.h"
#include "cluster.h"
#include "fatname.h"
#include "journal.h"
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

typedef struct {
    cluster_addr_t ca;
    int            offset;
} entry_info_t;

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

/*
Create new empty entry.
Params:
- fullname - 8.3 name for entry.
- is_dir - Is this entry for directory.
- dca - Data cluster address. 
        Note: This cluster should be marked as <FREE>.
- file_size - Size of file. If is_dir==1 not used.
- entry - Place where data will be saved.
- fi - FS data.

Return 1 if entry data generation complete.
Return 0 if something goes wrong.
*/
int create_entry(
    const char* __restrict fullname, char is_dir, cluster_addr_t dca, 
    unsigned int file_size, directory_entry_t* __restrict entry, fat_data_t* __restrict fi
);

/*
Base iterate function in cluster.
Params:
- ca - Cluster address.
- handler - Directory entry handler with context.
- ctx - Context for function.
- fi - FS data.

Return 1 if iterate success.
Return 0 if something goes wrong.
*/
int entry_iterate(
    cluster_addr_t ca, int (*handler)(entry_info_t*, directory_entry_t*, void*), void* ctx, fat_data_t* __restrict fi
);

/*
Index entry by provided cluster. This function will index all data to balanced binary tree.
Note: Balancing based on Red&Black mechanism and will took a while. Will give more benefits on big directories.
Params:
- ca - Entry ca. Should be a directory ca.
- cache - Pointer to pointer for cache tree.
- fi - FS data.

Return 1 if binary tree creation complete.
Return 0 if something goes wrong.
*/
int entry_index(cluster_addr_t ca, ecache_t** __restrict cache, fat_data_t* __restrict fi);

#define NO_ECACHE NULL
/*
Search entry in cluster by name. 
Params:
- name - Entry name.
- ca - Cluster address where we should search.
- cache - Cache of entry. Can be NO_ECACHE or NULL.
- meta - Storage for entry data.
- fi - FS data.

Return 1 if entry was found.
Return -1 if something goes wrong.
Return -4 if wntry wasn't found.
*/
int entry_search(
    const char* __restrict name, cluster_addr_t ca, ecache_t* __restrict cache,
    directory_entry_t* __restrict meta, fat_data_t* __restrict fi
);

/*
Add entry to cluster.
Params:
- ca - Cluster where entry will be saved.
- cache - If cache!=NO_ECACHE will insert new cache entry.
- meta - Entry data for saving.
- fi - FS data.

Return 1 if entry has been written.
Return < 0 if something goes wrong.
*/
int entry_add(cluster_addr_t ca, ecache_t* __restrict cache, directory_entry_t* __restrict meta, fat_data_t* __restrict fi);

/*
Edit entry in cluster.
Params:
- ca - Clyster where entry is placed.
- name - Name entry for edit.
- meta - New meta for entry.
- fi - FS data.

Return 1 if edit success.
Return 0 if something goes wrong.
*/
int entry_edit(
    cluster_addr_t ca, ecache_t* __restrict cache, const char* __restrict name, 
    const directory_entry_t* __restrict meta, fat_data_t* __restrict fi
);

/*
Entry remove just mark entry as free without erasing data.
- ca - Clyster where entry is placed.
- name - Name entry for edit.
- cache - Ecache for directory.
- fi - FS data.

Return 1 if delete success.
Return 0 if something goes wrong.
*/
int entry_remove(cluster_addr_t ca, const char* __restrict name, ecache_t* __restrict cache, fat_data_t* __restrict fi);

#ifdef __cplusplus
}
#endif
#endif