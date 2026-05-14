/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    Cluster-level read, write, allocation, and chain utilities above disk sectors.

Dependencies:
    - nft32/fat.h - FAT cluster types and table operations.
    - nft32/disk.h - Sector I/O primitives.
    - nft32/errors.h - Error registration.
    - nft32/fatinfo.h - FAT filesystem metadata.
    - std/null.h - NULL definition.
    - std/logging.h - Logging helpers.
    - std/threading.h - I/O locks.
*/

#ifndef CLUSTER_H_
#define CLUSTER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <nft32/fat.h>
#include <nft32/disk.h>
#include <nft32/errors.h>
#include <nft32/fatinfo.h>
#include <std/null.h>
#include <std/logging.h>
#include <std/threading.h>

/* Default offset for new clusters */
#define CLUSTER_OFFSET 128

typedef unsigned char        stack_buffer_t;
typedef unsigned char*       buffer_t;
typedef const unsigned char* const_buffer_t;

/*
Calculate count of clusters for provided size of data.
Params:
- `size` - Data size.
- `fi` - Pointer to FS data.

Return count of cluster for provided data.
*/
int get_cluster_count(unsigned int size, fat_data_t* fi);

/*
Allocate cluster from free-clusters on disk and DON'T mark cluster as "END_CLUSTER32"
[Thread-safe]

Params:
- `fi` - FS data.

Return cluster address or BAD_CLUSTER if error.
*/
cluster_addr_t alloc_cluster(fat_data_t* fi);

/*
Mark cluster as <FREE>.
Params:
- `ca` - Cluster address.
- `fi` - FS data.

Return 1 if dealloc success.
Return 0 if something goes wrong.
*/
int dealloc_cluster(const cluster_addr_t ca, fat_data_t* fi);

/*
Deallocate entier cluster chain from start to <END>.
Params:
- `ca` - Start cluster in chain.
- `fi` - FS data.

Return 1 if chain deallocated.
Return 0 if chain is broken (<BAD> or no <END>).
*/
int dealloc_chain(cluster_addr_t ca, fat_data_t* fi);

/*
Read data from cluster with offset.
Params:
- `offset` - Offset in cluster (Should be lower than spc * sector_size).
- `ca` - Cluster address.
- `buffer` - Pointer where function will store data.
- `buff_size` - buffer size.
- `fi` - FS data.

Return count of readden bytes.
*/
int readoff_cluster(
    cluster_addr_t ca, cluster_offset_t offset, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi
);

/*
Read data from cluster.
Params:
- `ca` - Cluster address.
- `buffer` - Pointer where function will store data.
- `buff_size` - buffer size.
- `fi` - FS data.

Return count of readden bytes.
*/
int read_cluster(cluster_addr_t ca, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi);

/*
Write data to cluster with offset.
Params:
- `ca` - Cluster address.
- `offset` - Offset in cluster (Should be lower than spc * sector_size).
- `buffer` - Pointer where function will take data for write.
- `buff_size` - buffer size.
- `fi` - FS data.

Return count of written bytes.
*/
int writeoff_cluster(
    cluster_addr_t ca, cluster_offset_t offset, const_buffer_t __restrict data, int data_size, fat_data_t* __restrict fi
);

/*
Write data to cluster.
Params:
- `ca` - Cluster address.
- `buffer` - Pointer where function will take data for write.
- `buff_size` - buffer size.
- `fi` - FS data.

Return count of written bytes.
*/
int write_cluster(cluster_addr_t ca, const_buffer_t __restrict data, int data_size, fat_data_t* __restrict fi);

/*
Copy source cluster content to destination cluster.
Note: copy buffer should be greater or equals to sector size.
Params:
- `src` - Source cluster.
- `dst` - Destination cluster.
- `buffer` - Copy buffer.
- `buff_size` - Copy buffer size.
- `fi` - FS data.

Return count of readden and written bytes.
*/
int copy_cluster(cluster_addr_t src, cluster_addr_t dst, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi);

#ifdef __cplusplus
}
#endif
#endif
