#include "../include/fat.h"

cluster_val_t read_fat(cluster_addr_t cluster, fat_data_t* fi) {
    cluster_offset_t fat_offset  = cluster * 4;
    sector_addr_t fat_sector = fi->first_fat_sector + (fat_offset / fi->cluster_size);
    unsigned char* cluster_data = malloc_s(fi->bytes_per_sector * fi->sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s error!");
        return BAD_CLUSTER_32;
    }

    if (!DSK_readoff_sectors(fat_sector, 0, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        free_s(cluster_data);
        return BAD_CLUSTER_32;
    }

    cluster_val_t table_value = cluster_data[fat_offset % fi->cluster_size];
    free_s(cluster_data);
    return table_value;
}

int write_fat(cluster_addr_t cluster, cluster_status_t value, fat_data_t* fi) {
    cluster_offset_t fat_offset  = cluster * 4;
    sector_addr_t fat_sector = fi->first_fat_sector + (fat_offset / fi->cluster_size);
    unsigned char* cluster_data = malloc_s(fi->bytes_per_sector * fi->sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s error!");
        return 0;
    }

    if (!DSK_readoff_sectors(fat_sector, 0, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        free_s(cluster_data);
        return 0;
    }

    cluster_data[fat_offset % fi->cluster_size] = value;
    if (!DSK_writeoff_sectors(fat_sector, 0, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        print_error("Could not write new FAT32 cluster number to sector.");
        free_s(cluster_data);
        return 0;
    }

    free_s(cluster_data);
    return 1;
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
