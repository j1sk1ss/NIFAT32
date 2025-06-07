#include "../include/fat.h"

static unsigned int* _fat = NULL;
int cache_fat_init(fat_data_t* fi) {
    _fat = (unsigned int*)malloc_s(fi->total_clusters * sizeof(unsigned int));
    if (!_fat) {
        print_error("malloc_s() error!");
        return 0;
    }

    for (cluster_addr_t ca = 0; ca < fi->total_clusters; ca++) _fat[ca] = FAT_CLUSTER_BAD;
    return 1;
}

static int __write_fat__(cluster_addr_t ca, cluster_status_t value, fat_data_t* fi, int fat) {
    cluster_offset_t fat_offset  = ca * 4;
    sector_addr_t fat_sector = fi->first_fat_sector + (fi->fat_size * fat) + (fat_offset / fi->cluster_size);
    if (!DSK_writeoff_sectors(fat_sector, fat_offset % fi->cluster_size, (unsigned char*)&value, sizeof(cluster_status_t), 1)) {
        print_error("Could not write new FAT32 cluster number to sector.");
        return 0;
    }

    return 1;    
} 

int write_fat(cluster_addr_t ca, cluster_status_t value, fat_data_t* fi) {
    if (_fat) _fat[ca] = value;
    for (int i = 0; i < fi->fat_count; i++) __write_fat__(ca, value, fi, i);
    return 1;
}

static cluster_val_t __read_fat__(cluster_addr_t ca, fat_data_t* fi, int fat) {
    cluster_offset_t fat_offset = ca * 4;
    sector_addr_t fat_sector  = fi->first_fat_sector + (fi->fat_size * fat) + (fat_offset / fi->cluster_size);
    cluster_val_t table_value = FAT_CLUSTER_BAD;
    if (!DSK_readoff_sectors(fat_sector, fat_offset % fi->cluster_size, (unsigned char*)&table_value, sizeof(cluster_val_t), 1)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        return FAT_CLUSTER_BAD;
    }

    return table_value;
} 

cluster_val_t read_fat(cluster_addr_t ca, fat_data_t* fi) {
    if (_fat && _fat[ca] != FAT_CLUSTER_BAD) return _fat[ca];

    int wrong = -1;
    int val_freq = 0;
    cluster_val_t table_value = FAT_CLUSTER_BAD;
    for (int i = 0; i < fi->fat_count; i++) {
        cluster_val_t fat_val = __read_fat__(ca, fi, i);
        if (fat_val == table_value) val_freq++;
        else {
            val_freq--;
            wrong++;
        }

        if (val_freq < 0) {
            table_value = fat_val;
            val_freq = 0;
        }
    }

    _fat[ca] = table_value;
    if (wrong > 0) {
        write_fat(ca, table_value, fi);
    }

    return table_value;
}

int is_cluster_free(cluster_val_t cluster) {
    return !cluster;
}

int set_cluster_free(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, 0, fi);
}

int is_cluster_end(cluster_val_t cluster) {
    return cluster == FAT_CLUSTER_END ? 1 : 0;
}

int set_cluster_end(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, FAT_CLUSTER_END, fi);
}

int is_cluster_bad(cluster_val_t cluster) {
    return cluster == FAT_CLUSTER_BAD ? 1 : 0;
}

int set_cluster_bad(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, FAT_CLUSTER_BAD, fi);
}
