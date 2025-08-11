#include "../include/fat.h"

static cluster_val_t* _fat = NULL;
int fat_cache_init(fat_data_t* fi) {
    _fat = (cluster_val_t*)malloc_s(fi->total_clusters * sizeof(cluster_val_t));
    if (!_fat) {
        print_error("malloc_s() error!");
        return 0;
    }

    for (cluster_addr_t ca = 0; ca < fi->total_clusters; ca++) _fat[ca] = FAT_CLUSTER_BAD;
    return 1;
}

int fat_cache_hload(fat_data_t* fi) {
    if (!_fat) return 0;
    for (cluster_addr_t ca = 0; ca < fi->total_clusters; ca++) {
        cluster_val_t cls = read_fat(ca, fi);
        if (cls == FAT_CLUSTER_FREE) fatmap_set(ca);
        else fatmap_unset(ca);
        _fat[ca] = cls;
    }

    return 1;
}

int fat_cache_unload() {
    if (_fat) free_s(_fat);
    else return 0;
    return 1;
}

#ifndef NIFAT32_RO
static int __write_fat__(cluster_addr_t ca, cluster_status_t value, fat_data_t* fi, int fat) {
    cluster_offset_t fat_offset = ca * sizeof(cluster_val_t) * sizeof(encoded_t);
    sector_addr_t fat_sector = fi->sectors_padd + GET_FATSECTOR(fat, fi->total_sectors) + (fat_offset / fi->bytes_per_sector);
    
    decoded_t table_buffer[sizeof(cluster_val_t)] = { 0 };
    pack_memory((const byte_t*)&value, table_buffer, sizeof(cluster_val_t));
    if (
        !DSK_writeoff_sectors(fat_sector, fat_offset % fi->bytes_per_sector, (const unsigned char*)table_buffer, sizeof(table_buffer), 1)
    ) {
        print_error("Could not write new FAT32 cluster number to sector.");
        return 0;
    }

    return 1;    
} 
#endif

int write_fat(cluster_addr_t ca, cluster_status_t value, fat_data_t* fi) {
#ifndef NIFAT32_RO
    print_debug("write_fat(ca=%u, value=%u)", ca, value);
    if (ca < fi->ext_root_cluster || ca > fi->total_clusters) return 0;
    
    if (_fat) _fat[ca] = value;
    if (value == FAT_CLUSTER_FREE) fatmap_set(ca);
    else fatmap_unset(ca);

    for (int i = 0; i < fi->fat_count; i++) __write_fat__(ca, value, fi, i);
    return 1;
#else
    return 1;
#endif
}

static cluster_val_t __read_fat__(cluster_addr_t ca, fat_data_t* fi, int fat) {
    cluster_offset_t fat_offset = ca * sizeof(cluster_val_t) * sizeof(encoded_t);
    sector_addr_t fat_sector = fi->sectors_padd + GET_FATSECTOR(fat, fi->total_sectors) + (fat_offset / fi->bytes_per_sector);
    
    encoded_t table_buffer[sizeof(cluster_val_t)] = { 0 };
    if (
        !DSK_readoff_sectors(fat_sector, fat_offset % fi->bytes_per_sector, (unsigned char*)table_buffer, sizeof(table_buffer), 1
    )) {
        print_error("Could not read sector that contains FAT32 table entry needed.");
        return FAT_CLUSTER_BAD;
    }

    cluster_val_t table_value = 0;
    unpack_memory(table_buffer, (byte_t*)&table_value, sizeof(cluster_val_t));
    return table_value & 0x0FFFFFFF;
}

cluster_val_t read_fat(cluster_addr_t ca, fat_data_t* fi) {
    print_debug("read_fat(ca=%u)", ca);
    if (ca < fi->ext_root_cluster || ca > fi->total_clusters) return FAT_CLUSTER_BAD;
    if (_fat && _fat[ca] != FAT_CLUSTER_BAD) {
        print_debug("cached read_fat(ca=%u) -> %u", ca, _fat[ca]);
        return _fat[ca];
    }

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
    if (table_value == FAT_CLUSTER_FREE) fatmap_set(ca);
    else fatmap_unset(ca);

    if (wrong > 0) {
        print_warn("FAT wrong value at ca=%u. Fixing to val=%u...", ca, table_value);
        write_fat(ca, table_value, fi);
    }

    print_debug("read_fat(ca=%u) -> %u", ca, table_value);
    return table_value;
}