#ifndef FATINFO_H_
#define FATINFO_H_

typedef struct {
    unsigned int  fat_size;
    unsigned int  fat_type;
    unsigned char fat_count;
    unsigned int  first_data_sector;
    unsigned int  total_sectors;
    unsigned int  total_clusters;
    unsigned int  bytes_per_sector;
    unsigned int  cluster_size;
    unsigned int  sectors_per_cluster;
    unsigned int  ext_root_cluster;
    unsigned int  sectors_padd;
} fat_data_t;

#endif