#ifndef FAT_H_
#define FAT_H_

#include "mm.h"
#include "disk.h"
#include "fatinfo.h"
#include "logging.h"
#include "threading.h"

#define FAT_CLUSTER_FREE     0x00000000
#define FAT_CLUSTER_BAD      0x0FFFFFF7
#define FAT_CLUSTER_RESERVED 0x0FFFFFF8
#define FAT_CLUSTER_END      0x0FFFFFFF

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
int cache_fat_init(fat_data_t* fi);

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
int is_cluster_free(cluster_val_t cluster);

/*
Set cluster free.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
int set_cluster_free(cluster_val_t cluster, fat_data_t* fi);

/*
Check if cluster is end.
Return 1 if it is end.
Return 0 if it is not.
*/
int is_cluster_end(cluster_val_t cluster);

/*
Set cluster end.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
int set_cluster_end(cluster_val_t cluster, fat_data_t* fi);

/*
Check if cluster is bad.
Return 1 if it is bad.
Return 0 if it is not.
*/
int is_cluster_bad(cluster_val_t cluster);

/*
Set cluster bad.
Return 1 if write to FAT success.
Return 0 if something goes wrong.
*/
int set_cluster_bad(cluster_val_t cluster, fat_data_t* fi);

#endif