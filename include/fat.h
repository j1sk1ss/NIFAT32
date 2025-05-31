#ifndef FAT_H_
#define FAT_H_

#include "fatinfo.h"

#define END_CLUSTER_32      0x0FFFFFF8
#define BAD_CLUSTER_32      0x0FFFFFF7
#define FREE_CLUSTER_32     0x00000000

int read_fat(unsigned int cluster, fat_data_t* fi);
int write_fat(unsigned int cluster, unsigned int value, fat_data_t* fi);

inline int is_cluster_free(unsigned int cluster) {
    return !cluster;
}

inline int set_cluster_free(unsigned int cluster, fat_data_t* fi) {
    return write_fat(cluster, 0, fi);
}

inline int is_cluster_end(unsigned int cluster) {
    return cluster == END_CLUSTER_32 ? 1 : 0;
}

inline int set_cluster_end(unsigned int cluster, fat_data_t* fi) {
    return write_fat(cluster, END_CLUSTER_32, fi);
}

inline int is_cluster_bad(unsigned int cluster) {
    return cluster == BAD_CLUSTER_32 ? 1 : 0;
}

inline int set_cluster_bad(unsigned int cluster, fat_data_t* fi) {
    return write_fat(cluster, BAD_CLUSTER_32, fi);
}

#endif