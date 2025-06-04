#include "../include/cluster.h"

lock_t _allocater_lock = NULL_LOCK;
static cluster_addr_t last_allocated_cluster = CLUSTER_OFFSET;
cluster_addr_t alloc_cluster(fat_data_t* fi) {
    if (!THR_require_write(&_allocater_lock, get_thread_num())) {
        print_error("Can't write-lock alloc_cluster function!");
        return BAD_CLUSTER_32;
    }

    cluster_addr_t cluster = last_allocated_cluster;
    cluster_status_t cluster_status = FREE_CLUSTER_32;
    while (cluster < fi->total_clusters) {
        cluster_status = read_fat(cluster, fi);
        if (is_cluster_free(cluster_status)) {
            if (set_cluster_end(cluster, fi)) {
                last_allocated_cluster = cluster;
                THR_release_write(&_allocater_lock, get_thread_num());
                return cluster;
            }
            else {
                print_error("Error occurred with write_fat(), aborting operations...");
                THR_release_write(&_allocater_lock, get_thread_num());
                return BAD_CLUSTER_32;
            }
        }
        else if (is_cluster_bad(cluster_status)) {
            print_error("Error occurred with read_fat(), aborting operations...");
            THR_release_write(&_allocater_lock, get_thread_num());
            return BAD_CLUSTER_32;
        }

        cluster++;
    }

    last_allocated_cluster = fi->ext_root_cluster;
    THR_release_write(&_allocater_lock, get_thread_num());
    return BAD_CLUSTER_32;
}

int dealloc_cluster(const cluster_addr_t cluster, fat_data_t* fi) {
    cluster_status_t cluster_status = read_fat(cluster, fi);
    if (is_cluster_free(cluster_status)) return 1;
    else if (is_cluster_bad(cluster_status)) {
        print_error("Error occurred with read_fat(), aborting operations...");
        return 0;
    }

    if (set_cluster_free(cluster, fi)) return 1;
    else {
        print_error("Error occurred with write_fat(), aborting operations...");
        return 0;
    }
}

int readoff_cluster(cluster_addr_t cluster, cluster_offset_t offset, buffer_t buffer, int buff_size, fat_data_t* fi) {
    print_debug("readoff_cluster(cluster=%u, offset=%u, size=%i)", cluster, offset, buff_size);
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    return DSK_readoff_sectors(start_sect, offset, buffer, buff_size, fi->sectors_per_cluster);
}

int read_cluster(cluster_addr_t cluster, buffer_t buffer, int buff_size, fat_data_t* fi) {
    return readoff_cluster(cluster, 0, buffer, buff_size, fi);
}

int writeoff_cluster(cluster_addr_t cluster, cluster_offset_t offset, const_buffer_t data, int data_size, fat_data_t* fi) {
    print_debug("writeoff_cluster(cluster=%u, offset=%u, size=%i)", cluster, offset, data_size);
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    return DSK_writeoff_sectors(start_sect, offset, data, data_size, fi->sectors_per_cluster);
}

int write_cluster(cluster_addr_t cluster, const_buffer_t data, int data_size, fat_data_t* fi) {
    return writeoff_cluster(cluster, 0, data, data_size, fi);
}