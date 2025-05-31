#include "../include/fat.h"

cluster_val_t read_fat(cluster_addr_t cluster, fat_data_t* fi) {
    unsigned int fat_offset  = cluster * 4;
    sector_addr_t fat_sector = fi->first_fat_sector + (fat_offset / fi->cluster_size);
    unsigned char* cluster_data = malloc_s(fi->bytes_per_sector * fi->sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s error!");
        return -2;
    }

    if (!DSK_read_sectors(fat_sector, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        free_s(cluster_data);
        return -1;
    }

    cluster_val_t table_value = cluster_data[fat_offset % fi->cluster_size];
    free_s(cluster_data);
    return table_value;
}

int write_fat(cluster_addr_t cluster, cluster_status_t value, fat_data_t* fi) {
    unsigned int fat_offset  = cluster * 4;
    sector_addr_t fat_sector = fi->first_fat_sector + (fat_offset / fi->cluster_size);
    unsigned char* cluster_data = malloc_s(fi->bytes_per_sector * fi->sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s error!");
        return -2;
    }

    if (!DSK_read_sectors(fat_sector, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        free_s(cluster_data);
        return -1;
    }

    cluster_data[fat_offset % fi->cluster_size] = value;
    if (!DSK_write_sectors(fat_sector, cluster_data, fi->bytes_per_sector * fi->sectors_per_cluster, fi->sectors_per_cluster)) {
        kprintf("Function __write_fat: Could not write new FAT32 cluster number to sector.");
        free_s(cluster_data);
        return -1;
    }

    free_s(cluster_data);
    return 0;
}
