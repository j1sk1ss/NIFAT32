/*
License:
    MIT License

    Copyright (c) 2025 Nikolay 

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Description:
    This file contains main tools for working with File Allocation Table

Dependencies:
    - mm.h - FS memory manager.
    - disk.h - Working with disk sectors.
    - hamming.h - Decoding and encoding FAT values.
    - fatinfo.h - Info structure about current FS setup.
    - logging.h - Logging tools.
    - threading.h - Locks for IO operations (cluster allocation).
*/

#ifndef FAT_H_
#define FAT_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "mm.h"
#include "disk.h"
#include "null.h"
#include "fatmap.h"
#include "hamming.h"
#include "fatinfo.h"
#include "logging.h"
#include "threading.h"

#define FAT_CLUSTER_FREE     0x00000000
#define FAT_CLUSTER_BAD      0x0FFFFFF7
#define FAT_CLUSTER_RESERVED 0x0FFFFFF8
#define FAT_CLUSTER_END      0x0FFFFFFF

#define FAT_MULTIPLIER 340573321U // Another prime, far from above
#define GET_FATSECTOR(n, ts)  (((((n) + 7) * FAT_MULTIPLIER) >> 13) % ts)

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

/*
Check if cluster is reserved.
Return 1 if it is bad.
Return 0 if it is not.
*/
int is_cluster_reserved(cluster_val_t cluster);

#ifdef __cplusplus
}
#endif
#endif