#include "../include/cluster.h"

int get_cluster_count(unsigned int size, fat_data_t* fi) {
    return size / (fi->sectors_per_cluster * fi->bytes_per_sector);
}

lock_t _allocater_lock = NULL_LOCK;
static cluster_addr_t last_allocated_cluster = CLUSTER_OFFSET;
cluster_addr_t alloc_cluster(fat_data_t* fi) {
    if (!THR_require_write(&_allocater_lock, get_thread_num())) {
        print_error("Can't write-lock alloc_cluster function!");
        return FAT_CLUSTER_BAD;
    }

    cluster_addr_t cluster = fatmap_find_free(last_allocated_cluster, 1, fi);
    if (!cluster) cluster = last_allocated_cluster;
    else {
        last_allocated_cluster = cluster + 1;
        THR_release_write(&_allocater_lock, get_thread_num());
        return cluster;
    }

    int nfree_skip_step = 0;
    cluster_status_t cluster_status = FAT_CLUSTER_FREE;
    while (cluster < fi->total_clusters) {
        cluster_status = read_fat(cluster, fi);
        if (is_cluster_free(cluster_status)) {
            last_allocated_cluster = cluster + 1;
            THR_release_write(&_allocater_lock, get_thread_num());
            return cluster;
        }
        else if (is_cluster_bad(cluster_status) || is_cluster_reserved(cluster_status)) {
            cluster += nfree_skip_step++;
        }

        cluster++;
    }

    last_allocated_cluster = fi->ext_root_cluster;
    THR_release_write(&_allocater_lock, get_thread_num());
    return FAT_CLUSTER_BAD;
}

int dealloc_cluster(const cluster_addr_t ca, fat_data_t* fi) {
    cluster_status_t cluster_status = read_fat(ca, fi);
    if (is_cluster_free(cluster_status)) return 1;
    if (set_cluster_free(ca, fi)) return 1;
    print_error("Error occurred with write_fat(), aborting operations...");
    return 0;
}

int dealloc_chain(cluster_addr_t ca, fat_data_t* fi) {
    do {
        cluster_addr_t next_cluster = read_fat(ca, fi);
        if (!dealloc_cluster(ca, fi)) {
            print_error("dealloc_cluster() encountered an error. Aborting...");
            return 0;
        }

        ca = next_cluster;
    } while (!is_cluster_end(ca) && !is_cluster_bad(ca) && !is_cluster_free(ca));
    return 1;
}

int readoff_cluster(
    cluster_addr_t ca, cluster_offset_t offset, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi
) {
    print_debug("readoff_cluster(ca=%u, offset=%u, size=%i)", ca, offset, buff_size);
    sector_addr_t start_sect = (ca - fi->ext_root_cluster) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    return DSK_readoff_sectors(start_sect, offset, buffer, buff_size, fi->sectors_per_cluster);
}

int read_cluster(cluster_addr_t ca, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi) {
    return readoff_cluster(ca, 0, buffer, buff_size, fi);
}

int writeoff_cluster(
    cluster_addr_t ca, cluster_offset_t offset, const_buffer_t __restrict data, int data_size, fat_data_t* __restrict fi
) {
    print_debug("writeoff_cluster(ca=%u, offset=%u, size=%i)", ca, offset, data_size);
    sector_addr_t start_sect = (ca - fi->ext_root_cluster) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    return DSK_writeoff_sectors(start_sect, offset, data, data_size, fi->sectors_per_cluster);
}

int write_cluster(cluster_addr_t ca, const_buffer_t __restrict data, int data_size, fat_data_t* __restrict fi) {
    return writeoff_cluster(ca, 0, data, data_size, fi);
}

int copy_cluster(cluster_addr_t src, cluster_addr_t dst, buffer_t __restrict buffer, int buff_size, fat_data_t* __restrict fi) {
    print_debug("copy_cluster(src=%u, dst=%u)", src, dst);
    sector_addr_t start_src = (src - fi->ext_root_cluster) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    sector_addr_t start_dst = (dst - fi->ext_root_cluster) * (unsigned short)fi->sectors_per_cluster + fi->first_data_sector;
    return DSK_copy_sectors(start_src, start_dst, fi->sectors_per_cluster, buffer, buff_size);
}
