#ifndef FAT_H_
#define FAT_H_

#include "mm.h"
#include "disk.h"
#include "fatinfo.h"
#include "logging.h"

#define END_CLUSTER_32  0x0FFFFFF8
#define BAD_CLUSTER_32  0x0FFFFFF7
#define FREE_CLUSTER_32 0x0

typedef unsigned int cluster_offset_t;
typedef unsigned int cluster_addr_t;
typedef unsigned int cluster_status_t;
typedef unsigned int cluster_val_t;

cluster_val_t read_fat(cluster_addr_t cluster, fat_data_t* fi);
int write_fat(cluster_addr_t cluster, cluster_status_t value, fat_data_t* fi);

int is_cluster_free(cluster_val_t cluster);
int set_cluster_free(cluster_val_t cluster, fat_data_t* fi);
int is_cluster_end(cluster_val_t cluster);
int set_cluster_end(cluster_val_t cluster, fat_data_t* fi);
int is_cluster_bad(cluster_val_t cluster);
int set_cluster_bad(cluster_val_t cluster, fat_data_t* fi);

#endif