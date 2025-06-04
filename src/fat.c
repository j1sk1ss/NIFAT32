#include "../include/fat.h"

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
    for (int i = 0; i < fi->fat_count; i++) __write_fat__(ca, value, fi, i);
    return 1;
}

static cluster_val_t __read_fat__(cluster_addr_t ca, fat_data_t* fi, int fat) {
    cluster_offset_t fat_offset = ca * 4;
    sector_addr_t fat_sector  = fi->first_fat_sector + (fi->fat_size * fat) + (fat_offset / fi->cluster_size);
    cluster_val_t table_value = BAD_CLUSTER_32;
    if (!DSK_readoff_sectors(fat_sector, fat_offset % fi->cluster_size, (unsigned char*)&table_value, sizeof(cluster_val_t), 1)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        return BAD_CLUSTER_32;
    }

    return table_value;
} 

cluster_val_t read_fat(cluster_addr_t ca, fat_data_t* fi) {
    int val_freq = 0;
    cluster_val_t table_value = BAD_CLUSTER_32;
    for (int i = 0; i < fi->fat_count; i++) {
        cluster_val_t fat_val = __read_fat__(ca, fi, i);
        if (fat_val != table_value) val_freq--;
        else val_freq++;

        if (val_freq < 0) {
            table_value = fat_val;
            val_freq = 0;
        }
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
    return cluster == END_CLUSTER_32 ? 1 : 0;
}

int set_cluster_end(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, END_CLUSTER_32, fi);
}

int is_cluster_bad(cluster_val_t cluster) {
    return cluster == BAD_CLUSTER_32 ? 1 : 0;
}

int set_cluster_bad(cluster_val_t cluster, fat_data_t* fi) {
    return write_fat(cluster, BAD_CLUSTER_32, fi);
}
