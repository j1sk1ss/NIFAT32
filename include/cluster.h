#ifndef CLUSTER_H_
#define CLUSTER_H_

#include "fat.h"
#include "disk.h"
#include "fatinfo.h"
#include "logging.h"
#include "threading.h"

#define CLUSTER_OFFSET 128

typedef unsigned char*       buffer_t;
typedef const unsigned char* const_buffer_t;

/*
Allocate cluster from free-clusters on disk and mark cluster as "END_CLUSTER32"
[Thread-safe]

Params:
- fi - FS data.

Return cluster address or BAD_CLUSTER if error.
*/
cluster_addr_t alloc_cluster(fat_data_t* fi);

/*
*/
int dealloc_cluster(const cluster_addr_t cluster, fat_data_t* fi);

/*
*/
int readoff_cluster(cluster_addr_t cluster, cluster_offset_t offset, buffer_t buffer, int buff_size, fat_data_t* fi);

/*
*/
int read_cluster(cluster_addr_t cluster, buffer_t buffer, int buff_size, fat_data_t* fi);

/*
*/
int writeoff_cluster(cluster_addr_t cluster, cluster_offset_t offset, const_buffer_t data, int data_size, fat_data_t* fi);

/*
*/
int write_cluster(cluster_addr_t cluster, const_buffer_t data, int data_size, fat_data_t* fi);

#endif