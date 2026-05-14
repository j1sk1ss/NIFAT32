/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    File Allocation Table types, cache modes, and cluster chain operations.

Dependencies:
    - std/mm.h - Filesystem memory manager.
    - std/null.h - NULL definition.
    - std/hamming.h - Encoded FAT value helpers.
    - std/logging.h - Logging helpers.
    - std/threading.h - FAT locks.
    - nft32/disk.h - Sector I/O primitives.
    - nft32/fatmap.h - Free cluster bitmap.
    - nft32/errors.h - Error registration.
    - nft32/fatinfo.h - FAT filesystem metadata.
*/

#ifndef FAT_H_
#define FAT_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/mm.h>
#include <std/null.h>
#include <std/hamming.h>
#include <std/logging.h>
#include <std/threading.h>
#include <nft32/disk.h>
#include <nft32/fatmap.h>
#include <nft32/errors.h>
#include <nft32/fatinfo.h>

#define FAT_CLUSTER_FREE     0x00000000
#define FAT_CLUSTER_BAD      0x0FFFFFF7
#define FAT_CLUSTER_RESERVED 0x0FFFFFF8
#define FAT_CLUSTER_END      0x0FFFFFFF

#define FAT_MULTIPLIER 340573321U // Another prime, far from above
#define GET_FATSECTOR(n, ts) (((((n) + 7) * FAT_MULTIPLIER) >> 13) % (ts - 32))

typedef unsigned int cluster_offset_t;
typedef unsigned int cluster_addr_t;
typedef unsigned int cluster_status_t;
typedef unsigned int cluster_val_t;

/*
Initialize cache for FAT.
Note: Will allocate total_cluster * sizeof(uint32_t). 
Params:
- fi - Pointer to FS info.

Return 1 if init success.
Return 0 if something goes wrong.
*/
int fat_cache_init(fat_data_t* fi);

/*
FAT cache hard load. Will load entier FAT table to RAM via fat_read.
WARN: This operation really slow. Invoke it before start and only ones.
Params:
- fi - FS info.

Return 1 if hard load success.
Return 0 if something goes wrong.
*/
int fat_cache_hload(fat_data_t* fi);

/*
Unload allocated fat cache table.
Return 1 if operation success.
Return 0 if cache was NULL.
*/
int fat_cache_unload();

/*
Read 4 bytes from FAT for target cluster.
Note: Will read data from all copies. Return most freq. data.
Note 2: Fix bit-errors if major voting works correct.
Params:
- ca - Target claster address.
- fi - FS info.

Return cluster value or FAT_CLUSTER_BAD on error.
- 
*/
cluster_val_t read_fat(cluster_addr_t ca, fat_data_t* fi);

/*
Write 4 bytes to FAT for target cluster.
Note: Will sync all FAT copies.
Params:
- ca - Target claster address.
- fi - FS info.

Return 1 if write success.
Return 0 if something goes wrong.
*/
int write_fat(cluster_addr_t ca, cluster_status_t value, fat_data_t* fi);

/*
Check if cluster is free.
Return 1 if it is free.
Return 0 if it is not.
*/
static inline int is_cluster_free(cluster_val_t cluster) {
    return !cluster;
}

/*
Set cluster free.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
static inline int set_cluster_free(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, 0, fi);
}

/*
Check if cluster is end.
Return 1 if it is end.
Return 0 if it is not.
*/
static inline int is_cluster_end(cluster_val_t cluster) {
    return cluster == FAT_CLUSTER_END;
}

/*
Set cluster end.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
static inline int set_cluster_end(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, FAT_CLUSTER_END, fi);
}

/*
Check if cluster is bad.
Return 1 if it is bad.
Return 0 if it is not.
*/
static inline int is_cluster_bad(cluster_val_t cluster) {
    return cluster == FAT_CLUSTER_BAD;
}

/*
Set cluster bad.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
static inline int set_cluster_bad(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, FAT_CLUSTER_BAD, fi);
}

/*
Check if cluster is reserved.
Return 1 if it is bad.
Return 0 if it is not.
*/
static inline int is_cluster_reserved(cluster_val_t cluster) {
    return cluster == FAT_CLUSTER_RESERVED;
}

#ifdef __cplusplus
}
#endif
#endif
