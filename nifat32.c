#include "nifat32.h"

static fat_data_t _fs_data;

int NIFAT32_init(nifat32_params* params) {
    print_log("NIFAT32 init. Reading %i bootsector at sa=%i", params->bs_num, GET_BOOTSECTOR(params->bs_num, params->ts));
    if (params->bs_num >= 5) {
        print_error("Init error! No reserved sectors!");
        return 0;
    }

    mm_init();
    int sector_size = DSK_get_sector_size();
    buffer_t encoded_bs = (buffer_t)malloc_s(sector_size);
    if (!encoded_bs) {
        print_error("malloc_s() error!");
        return 0;
    }

    if (!DSK_read_sector(GET_BOOTSECTOR(params->bs_num, params->ts), encoded_bs, sector_size)) {
        print_error("DSK_read_sector() error!");
        free_s(encoded_bs);
        return 0;
    }

    buffer_t decoded_bs = (buffer_t)malloc_s(sector_size);
    if (!decoded_bs) {
        print_error("malloc_s() error!");
        return 0;
    }

    unpack_memory((encoded_t*)encoded_bs, decoded_bs, sector_size);

    fat_BS_t* bootstruct = (fat_BS_t*)decoded_bs;
    fat_extBS_32_t* ext_bootstruct = &bootstruct->extended_section;

    checksum_t bcheck = bootstruct->checksum;
    bootstruct->checksum = 0;
    checksum_t exbcheck = ext_bootstruct->checksum;
    ext_bootstruct->checksum = 0;

    ext_bootstruct->checksum = crc32(0, (buffer_t)ext_bootstruct, sizeof(fat_extBS_32_t));
    bootstruct->checksum     = crc32(0, (buffer_t)bootstruct, sizeof(fat_BS_t));
    if (bootstruct->checksum != bcheck || ext_bootstruct->checksum != exbcheck) {
        print_error(
            "Checksum check error! [bootstruct=%u != %u] or [ext_bootstruct=%u != %u]. Moving to reserved sector!", 
            bootstruct->checksum, bcheck, ext_bootstruct->checksum, exbcheck
        );

        params->bs_num++;
        return NIFAT32_init(params);
    }
    else {
        bootstruct->checksum     = bcheck;
        ext_bootstruct->checksum = exbcheck;
    }

    _fs_data.fat_count = bootstruct->table_count;
    _fs_data.total_sectors = bootstruct->total_sectors_32;
    _fs_data.fat_size = ext_bootstruct->table_size_32;

    int root_dir_sectors = ((bootstruct->root_entry_count * 32) + (bootstruct->bytes_per_sector - 1)) / bootstruct->bytes_per_sector;
    int data_sectors = _fs_data.total_sectors - (bootstruct->reserved_sector_count + (bootstruct->table_count * _fs_data.fat_size) + root_dir_sectors);
    if (!data_sectors || !bootstruct->sectors_per_cluster) {
        _fs_data.total_clusters = bootstruct->total_sectors_32 / bootstruct->sectors_per_cluster;
    }
    else {
        _fs_data.total_clusters = data_sectors / bootstruct->sectors_per_cluster;
    }

    _fs_data.fat_type = 32;
    _fs_data.first_data_sector = bootstruct->reserved_sector_count + bootstruct->table_count * ext_bootstruct->table_size_32;
    _fs_data.sectors_per_cluster = bootstruct->sectors_per_cluster;
    _fs_data.bytes_per_sector = bootstruct->bytes_per_sector;
    _fs_data.sectors_padd = bootstruct->reserved_sector_count;
    _fs_data.ext_root_cluster = ext_bootstruct->root_cluster;
    _fs_data.cluster_size = _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster;

    print_info("| NIFAT32 init success! Stats from boot sector:");
    print_info("| FAT type:                  %i", _fs_data.fat_type);
    print_info("| Bytes per sector:          %u", _fs_data.bytes_per_sector);
    print_info("| Sectors per cluster:       %u", _fs_data.sectors_per_cluster);
    print_info("| Reserved sectors:          %u", bootstruct->reserved_sector_count);
    print_info("| Number of FATs:            %u", bootstruct->table_count);
    print_info("| FAT size (in sectors):     %u", _fs_data.fat_size);
    print_info("| Total sectors:             %u", _fs_data.total_sectors);
    print_info("| Root entry count:          %u", bootstruct->root_entry_count);
    print_info("| Root dir sectors:          %d", root_dir_sectors);
    print_info("| Data sectors:              %d", data_sectors);
    print_info("| Total clusters:            %u", _fs_data.total_clusters);
    print_info("| First FAT sector:          %u", _fs_data.sectors_padd);
    print_info("| First data sector:         %u", _fs_data.first_data_sector);
    print_info("| Root cluster (FAT32):      %u", _fs_data.ext_root_cluster);
    print_info("| Cluster size (in bytes):   %u", _fs_data.cluster_size);

    if (params->bs_num > 0) {
        print_warn("%i of boot sector records are incorrect. Attempt to fix...", params->bs_num);
        for (int i = 0; i < 5; i++) {
            if (i == params->bs_num) continue;
            if (!DSK_write_sector(GET_BOOTSECTOR(i, params->ts), (const_buffer_t)encoded_bs, sector_size)) {
                print_warn("Attempt for bootsector restore failed!");
            }
        }
    }

    if (params->fat_cache) {
        if (!fat_cache_init(&_fs_data)) {
            print_warn("FAT cache init error!");
        }
    }

    if (!ctable_init()) {
        print_warn("Ctable init error!");
    }

    free_s(encoded_bs);
    free_s(decoded_bs);
    return 1;
}

/*
Find cluster active cluster by path.
Return FAT_CLUSTER_BAD if path invalid.
dfca - Default start cluster address. Place where we start search.
*/
static cluster_addr_t _get_cluster_by_path(
    const char* path, directory_entry_t* entry, cluster_addr_t* parent, unsigned char mode, const ci_t rci
) {
    print_debug("_get_cluster_by_path(path=%s, mode=%p, rci=%i)", path, mode, rci);

    cluster_addr_t parent_cluster = _fs_data.ext_root_cluster;
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;
    if (rci != NO_RCI) parent_cluster = active_cluster = get_content_data_ca(rci);

    unsigned int start = 0;
    directory_entry_t current_entry;
    for (unsigned int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == PATH_SPLITTER || !path[iterator]) {
            char name_buffer[32]    = { 0 };
            char fatname_buffer[32] = { 0 };

            str_memcpy(name_buffer, path + start, iterator - start);
            name_to_fatname(name_buffer, fatname_buffer);

            ecache_t* entry_index = get_content_ecache(rci);
            if (entry_search(fatname_buffer, active_cluster, entry_index, &current_entry, &_fs_data) < 0) {
                if (IS_CREATE_MODE(mode)) {
                    cluster_addr_t nca = alloc_cluster(&_fs_data);
                    if (set_cluster_end(nca, &_fs_data)) {
                        create_entry(
                            fatname_buffer, path[iterator] || GET_MODE_TARGET(mode) != FILE_TARGET, 
                            nca, _fs_data.cluster_size, &current_entry, &_fs_data
                        );

                        if (entry_add(active_cluster, entry_index, &current_entry, &_fs_data) < 0) {
                            print_error("Can't add new entry with mode=%p", mode);
                            dealloc_cluster(current_entry.cluster, &_fs_data);
                        }
                    }
                    else {
                        print_error("set_cluster_end() error!");
                        return FAT_CLUSTER_BAD;
                    }
                }
                else {
                    return FAT_CLUSTER_BAD;
                }
            }

            start = iterator + 1;
            parent_cluster = active_cluster;
            active_cluster = current_entry.cluster;
        }
    }

    if (entry)  str_memcpy(entry, &current_entry, sizeof(directory_entry_t));
    if (parent) *parent = parent_cluster;
    return active_cluster;    
}

int NIFAT32_content_exists(const char* path) {
    print_log("NIFAT32_content_exists(path=%s)", path);
    return _get_cluster_by_path(path, NULL, NULL, DF_MODE, NO_RCI) != FAT_CLUSTER_BAD;
}

ci_t NIFAT32_open_content(const ci_t rci, const char* path, unsigned char mode) {
    print_log("NIFAT32_open_content(path=%s, mode=%p)", path, mode);
    ci_t ci;
    if ((ci = alloc_ci()) < 0) {
        print_error("Ctable is full!");
        return -1;
    }

    cluster_addr_t rca;
    directory_entry_t meta;
    cluster_addr_t ca = _get_cluster_by_path(path, &meta, &rca, mode, rci);
    if (is_cluster_bad(ca)) {
        print_error("Entry path=%s, not found!", path);
        destroy_content(ci);
        return -2;
    }
    else {
        print_debug("NIFAT32_open_content: Content cluster is: %u", ca);
    }
    
    setup_content(ci, (meta.attributes & FILE_DIRECTORY) == FILE_DIRECTORY, (const char*)meta.file_name, rca, ca, &meta, mode);
    return ci;
}

int NIFAT32_index_content(const ci_t ci) {
    print_log("NIFAT32_index_content(ci=%u)", ci);
    if (get_content_type(ci) != CONTENT_TYPE_DIRECTORY) {
        print_error("Can't index content ci=%i. This content is not a directory! Type: [%i]", ci, get_content_type(ci));
        return 0;
    }

    return index_content(ci, &_fs_data);
}

int NIFAT32_close_content(ci_t ci) {
    print_log("NIFAT32_close_content(ci=%i)", ci);
    return destroy_content(ci);
}

int NIFAT32_read_content2buffer(const ci_t ci, cluster_offset_t offset, buffer_t buffer, int buff_size) {
    print_log("NIFAT32_read_content2buffer(ci=%i, offset=%u, readsize=%i)", ci, offset, buff_size);
    if (!IS_READ_MODE(get_content_mode(ci))) {
        print_error("Can't open content ci=%i. No access to read!", ci);
        return 0;
    }

    int total_readden = 0;
    cluster_addr_t ca = get_content_data_ca(ci);
    do {
        if (offset > _fs_data.cluster_size) offset -= _fs_data.cluster_size;
        else {
            int readeble = (buff_size > (int)(_fs_data.cluster_size - offset)) ? (int)(_fs_data.cluster_size - offset) : buff_size;
            if (!readoff_cluster(ca, offset, buffer + total_readden, readeble, &_fs_data)) {
                print_error("readoff_cluster() error. Aborting...");
                return 0;
            }

            offset = 0;
            buff_size -= readeble;
            total_readden += readeble;
        }

        ca = read_fat(ca, &_fs_data);
    } while (!is_cluster_end(ca) && !is_cluster_bad(ca) && buff_size > 0);

    return total_readden;
}

static cluster_addr_t _add_cluster_to_chain(cluster_addr_t hca) {
    print_debug("_add_cluster_to_chain(hca=%i)", hca);
    cluster_addr_t allocated_cluster = alloc_cluster(&_fs_data);
    if (!is_cluster_bad(allocated_cluster)) {
        if (!set_cluster_end(allocated_cluster, &_fs_data)) {
            print_error("Can't set cluster to <END> state!");
            dealloc_cluster(allocated_cluster, &_fs_data);
            return FAT_CLUSTER_BAD;
        }

        if (!write_fat(hca, allocated_cluster, &_fs_data)) {
            print_error("Allocated cluster can't be added to content!");
            dealloc_cluster(allocated_cluster, &_fs_data);
            return FAT_CLUSTER_BAD;
        }

        return allocated_cluster;
    }
    else {
        print_error("Allocated cluster [addr=%i] is <BAD>!", allocated_cluster);
        dealloc_cluster(allocated_cluster, &_fs_data);
        return FAT_CLUSTER_BAD;
    }

    return FAT_CLUSTER_BAD;
}

/*
Params:
- ci - Content that needs new cluster.
- lca - Last cluster of content. Can be FAT_CLUSTER_BAD if we don't now yet last cluster.

Return cluster_addr_t to new cluster. Also new cluster marked as FAT_CLUSTER_END.
Return FAT_CLUSTER_BAD if comething goes wrong.
*/
static cluster_addr_t _add_cluster_to_content(const ci_t ci, cluster_addr_t lca) {
    print_debug("_add_cluster_to_content(ci=%i, lca=%i)", ci, lca);
    if (lca == FAT_CLUSTER_BAD) {
        int max_iterations = _fs_data.total_clusters;
        cluster_addr_t cluster = get_content_data_ca(ci);
        while (!is_cluster_end(cluster) && !is_cluster_bad(cluster) && max_iterations-- > 0) {
            lca = cluster;
            cluster = read_fat(cluster, &_fs_data);
        }

        if (max_iterations <= 0) {
            print_error("Can't allocate cluster!");
            return FAT_CLUSTER_BAD;
        }
    }

    return _add_cluster_to_chain(lca);
}

int NIFAT32_write_buffer2content(const ci_t ci, cluster_offset_t offset, const_buffer_t data, int data_size) {
    print_log("NIFAT32_write_buffer2content(ci=%i, offset=%u, writesize=%i)", ci, offset, data_size);
    if (!IS_WRITE_MODE(get_content_mode(ci))) {
        print_error("Can't open content ci=%i. No access to write!", ci);
        return 0;
    }

    int total_written = 0;
    unsigned int total_size = 0;
    cluster_addr_t ca  = get_content_data_ca(ci);
    cluster_addr_t lca = ca;
    do {
        if (offset > _fs_data.cluster_size) {
            total_size += _fs_data.cluster_size;
            offset -= _fs_data.cluster_size;
        }
        else {
            int writable = (data_size > (int)(_fs_data.cluster_size - offset)) ? (int)(_fs_data.cluster_size - offset) : data_size;
            if (!writeoff_cluster(ca, offset, data + total_written, writable, &_fs_data)) {
                print_error("readoff_cluster() error. Aborting...");
                return 0;
            }

            offset = 0;
            data_size -= writable;
            total_written += writable;
        }

        lca = ca;
        ca  = read_fat(ca, &_fs_data);
    } while (!is_cluster_end(ca) && !is_cluster_bad(ca) && data_size > 0);

    ca = lca;
    while (data_size > 0 && !is_cluster_bad(ca = _add_cluster_to_content(ci, ca))) {
        if (offset > _fs_data.cluster_size) {
            total_size += _fs_data.cluster_size;
            offset -= _fs_data.cluster_size;
        }
        else {
            int writable = (data_size > (int)(_fs_data.cluster_size - offset)) ? (int)(_fs_data.cluster_size - offset) : data_size;
            writeoff_cluster(ca, offset, data + total_written, writable, &_fs_data);

            offset = 0;
            data_size -= writable;
            total_written += writable;
        }
    }

    directory_entry_t entry;
    // create_entry(get_content_name(ci), 0, get_content_data_ca(ci), total_size + total_written, &entry, &_fs_data);
    // entry_edit(get_content_root_ca(ci), get_content_name(ci), &entry, &_fs_data);
    return total_written;
}

int NIFAT32_change_meta(const ci_t ci, const cinfo_t* info) {
    print_log("NIFAT32_change_meta(ci=%i, info=%s/%s/%s)", ci, info->full_name, info->file_name, info->file_extension);
    directory_entry_t meta;
    create_entry(
        info->full_name, info->type == STAT_DIR, get_content_data_ca(ci), info->size, &meta, &_fs_data
    );

    if (!entry_edit(get_content_root_ca(ci), get_content_name(ci), &meta, &_fs_data)) {
        print_error("entry_edit() encountered an error. Aborting...");
        return 0;
    }
    
    return 1;
}

int NIFAT32_truncate_content(const ci_t ci, cluster_offset_t offset, int size) {
    print_log("NIFAT32_truncate_content(ci=%i, offset=%u, size=%i)", ci, offset, size);
    if (!IS_WRITE_MODE(get_content_mode(ci))) {
        print_error("Can't open content ci=%i. No access to write!", ci);
        return 0;
    }

    unsigned int end_size = size;
    cluster_addr_t ca = get_content_data_ca(ci);
    cluster_addr_t start_ca = FAT_CLUSTER_BAD, end_ca = FAT_CLUSTER_BAD;
    do {
        if (end_ca != FAT_CLUSTER_BAD) dealloc_cluster(ca, &_fs_data);
        else {
            if (offset > _fs_data.cluster_size) {
                offset -= _fs_data.cluster_size;
                dealloc_cluster(ca, &_fs_data);
            }
            else {
                if (start_ca == FAT_CLUSTER_BAD) start_ca = ca;
                if ((size -= _fs_data.cluster_size) < 0 && end_ca == FAT_CLUSTER_BAD) {
                    set_cluster_end(ca, &_fs_data);
                    end_ca = ca;
                }
            }
        }

        ca = read_fat(ca, &_fs_data);
    } while (!is_cluster_end(ca) && !is_cluster_bad(ca));

    directory_entry_t entry;
    create_entry(get_content_name(ci), 0, start_ca, end_size, &entry, &_fs_data);
    entry_edit(get_content_root_ca(ci), get_content_name(ci), &entry, &_fs_data);
    return 1;
}

int NIFAT32_put_content(const ci_t ci, cinfo_t* info, int reserve) {
    print_log("NIFAT32_put_content(ci=%i, info=%s, reserve=%i)", ci, info->full_name, reserve);
    cluster_addr_t target = _fs_data.ext_root_cluster;
    if (ci != PUT_TO_ROOT) target = get_content_data_ca(ci);
    ecache_t* entry_cache = get_content_ecache(target);
    int is_found = entry_search((char*)info->full_name, target, entry_cache, NULL, &_fs_data);
    if (is_found < 0 && is_found != -4) {
        print_error("entry_search() encountered an error [%i]. Aborting...", is_found);
        return 0;
    }

    directory_entry_t entry;
    cluster_addr_t entry_ca = alloc_cluster(&_fs_data);
    if (!set_cluster_end(entry_ca, &_fs_data)) {
        print_error("set_cluster_end() error!");
        return 0;
    }

    create_entry(
        info->full_name, info->type == STAT_DIR, entry_ca, reserve * _fs_data.cluster_size, &entry, &_fs_data
    );
    
    int is_add = entry_add(target, entry_cache, &entry, &_fs_data);
    if (is_add < 0) {
        print_error("entry_add() encountered an error [%i]. Aborting...", is_add);
        dealloc_cluster(entry.cluster, &_fs_data);
        return 0;
    }

    if (reserve > NO_RESERVE) {
        cluster_addr_t lca = entry.cluster;
        for (; reserve > NO_RESERVE; reserve--) {
            lca = _add_cluster_to_chain(lca);
        }
    }

    return 1;
}

int NIFAT32_copy_content(const ci_t src, const ci_t dst, char deep) {
    print_log("NIFAT32_copy_content(src=%i, dst=%i, deep=%i)", src, dst, deep);
    dealloc_chain(get_content_data_ca(dst), &_fs_data);
    switch (deep) {
        case DEEP_COPY: {
            cluster_addr_t dst_ca = alloc_cluster(&_fs_data);
            if (!set_cluster_end(dst_ca, &_fs_data)) {
                print_error("set_cluster_end() error!");
                return 0;
            }

            set_content_data_ca(dst, dst_ca);
            cluster_addr_t src_ca = get_content_data_ca(src);
            buffer_t copy_buffer  = (buffer_t)malloc_s(_fs_data.bytes_per_sector);
            if (!copy_buffer) {
                print_error("malloc_s() error!");
                return 0;
            }

            do {
                if (!copy_cluster(src_ca, dst_ca, copy_buffer, _fs_data.bytes_per_sector, &_fs_data)) {
                    print_error("copy_cluster() error. Aborting...");
                    free_s(copy_buffer);
                    return 0;
                }

                src_ca = read_fat(src_ca, &_fs_data);
                dst_ca = _add_cluster_to_chain(dst_ca);
            } while (!is_cluster_end(src_ca) && !is_cluster_bad(src_ca) && !is_cluster_bad(dst_ca));

            free_s(copy_buffer);
            break;
        }
        case SHALLOW_COPY: {
            set_content_data_ca(dst, get_content_data_ca(src));
            break;
        }
        default: {
            print_error("Unknown deep=%i type!", deep);
            return 0;
        }
    }

    return 1;
}

int NIFAT32_delete_content(ci_t ci) {
    print_log("NIFAT32_delete_content(ci=%i)", ci);
    if (!entry_remove(get_content_root_ca(ci), get_content_name(ci), &_fs_data)) {
        print_error("entry_remove() encountered an error. Aborting...");
        return 0;
    }

    NIFAT32_close_content(ci);
    return 1;
}

int NIFAT32_stat_content(const ci_t ci, cinfo_t* info) {
    print_log("NIFAT32_stat_content(ci=%i)", ci);
    return stat_content(ci, info);
}

int NIFAT32_unload() {
    fat_cache_unload();
    return 1;
}
