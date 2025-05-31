#include "../include/fat.h"


int read_fat(unsigned int cluster, fat_data_t* fi) {
    unsigned int fat_offset = cluster * 4;
    unsigned char* cluster_data = ATA_read_sectors(fi->first_fat_sector + (fat_offset / fi->cluster_size), fi->sectors_per_cluster);
    if (!cluster_data) {
        // kprintf("[%s %i] Function __read_fat: Could not read sector that contains FAT32 table entry needed.\n", __FILE__, __LINE__);
        return -1;
    }

    unsigned int table_value = *(unsigned int*)&cluster_data[fat_offset % fi->cluster_size];
    // ALC_free(cluster_data, KERNEL);
    return table_value;
}

int write_fat(unsigned int cluster, unsigned int value, fat_data_t* fi) {
    unsigned int fat_offset = cluster * 4;
    unsigned int fat_sector = fi->first_fat_sector + (fat_offset / fi->cluster_size);
    
    unsigned char* cluster_data = ATA_read_sectors(fat_sector, fi->sectors_per_cluster);
    if (!cluster_data) {
        // kprintf("Function __write_fat: Could not read sector that contains FAT32 table entry needed.\n");
        return -1;
    }

    *(unsigned int*)&cluster_data[fat_offset % fi->cluster_size] = value;
    // if (ATA_write_sectors(fat_sector, cluster_data, FAT_data.sectors_per_cluster) != 1) {
    //     kprintf("Function __write_fat: Could not write new FAT32 cluster number to sector.\n");
    //     return -1;
    // }

    // ALC_free(cluster_data, KERNEL);
    return 0;
}
