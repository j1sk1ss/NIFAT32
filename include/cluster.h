#ifndef CLUSTER_H_
#define CLUSTER_H_

#include "fat.h"
#include "disk.h"
#include "fatinfo.h"
#include "logging.h"
#include "threading.h"

/* Default offset for new clusters */
#define CLUSTER_OFFSET 128

typedef unsigned char*       buffer_t;
typedef const unsigned char* const_buffer_t;

/*
Calculate count of clusters for provided size of data.
Params:
- size - Data size.
- fi - Pointer to FS data.

Return count of cluster for provided data.
*/
int get_cluster_count(unsigned int size, fat_data_t* fi);

/*
Allocate cluster from free-clusters on disk and mark cluster as "END_CLUSTER32"
[Thread-safe]

Params:
- fi - FS data.

Return cluster address or BAD_CLUSTER if error.
*/
cluster_addr_t alloc_cluster(fat_data_t* fi);

/*
Mark cluster as <FREE>.
Params:
- ca - Cluster address.
- fi - FS data.

Return 1 if dealloc success.
Return 0 if something goes wrong.
*/
int dealloc_cluster(const cluster_addr_t ca, fat_data_t* fi);

/*
Read data from cluster with offset.
Params:
- ca - Cluster address.
- offset - Offset in cluster (Should be lower than spc * sector_size).
- buffer - Pointer where function will store data.
- buff_size - buffer size.
- fi - FS data.

Return count of readden bytes.
*/
int readoff_cluster(cluster_addr_t ca, cluster_offset_t offset, buffer_t buffer, int buff_size, fat_data_t* fi);

/*
Read data from cluster.
Params:
- ca - Cluster address.
- buffer - Pointer where function will store data.
- buff_size - buffer size.
- fi - FS data.

Return count of readden bytes.
*/
int read_cluster(cluster_addr_t ca, buffer_t buffer, int buff_size, fat_data_t* fi);

/*
Write data to cluster with offset.
Params:
- ca - Cluster address.
- offset - Offset in cluster (Should be lower than spc * sector_size).
- buffer - Pointer where function will take data for write.
- buff_size - buffer size.
- fi - FS data.

Return count of written bytes.
*/
int writeoff_cluster(cluster_addr_t ca, cluster_offset_t offset, const_buffer_t data, int data_size, fat_data_t* fi);

/*
Write data to cluster.
Params:
- ca - Cluster address.
- buffer - Pointer where function will take data for write.
- buff_size - buffer size.
- fi - FS data.

Return count of written bytes.
*/
int write_cluster(cluster_addr_t ca, const_buffer_t data, int data_size, fat_data_t* fi);

#endif