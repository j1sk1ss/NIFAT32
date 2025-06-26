#include "../include/entry.h"

static int _validate_entry(directory_entry_t* entry) {
    checksum_t entry_checksum = entry->checksum;
    entry->checksum = 0;
    if (crc32(0, (buffer_t)entry, sizeof(directory_entry_t)) != entry_checksum) return 0;
    else entry->checksum = entry_checksum;
    return 1;
}

static int _read_encoded_cluster(
    cluster_addr_t ca, buffer_t __restrict enc, int enc_size, buffer_t __restrict dec, int dec_size, fat_data_t* __restrict fi
) {
    print_debug("_read_encoded_cluster(ca=%u)", ca);
    if (!enc || !dec) return 0;
    if (!read_cluster(ca, enc, enc_size, fi)) {
        print_error("read_cluster() encountered an error. Aborting...");
        return 0;
    }

    unpack_memory((const encoded_t*)enc, dec, dec_size);
    return 1;
}

static int _allocate_buffers(
    buffer_t* __restrict encoded, unsigned int enc_len, buffer_t* __restrict decoded, unsigned int dec_len
) {
    *encoded = (buffer_t)malloc_s(enc_len);
    *decoded = (buffer_t)malloc_s(dec_len);
    if (!*encoded || !*decoded) {
        print_error("malloc_s() error!");
        if (*encoded) free_s(*encoded);
        if (*decoded) free_s(*decoded);
        return 0;
    }

    return 1;
}

int entry_index(cluster_addr_t ca, ecache_t** __restrict cache, fat_data_t* __restrict fi) {
    print_debug("entry_index(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
        
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
            if (entry->file_name[0] == ENTRY_END) break;
            if (
                _validate_entry(entry) && 
                entry->file_name[0] != ENTRY_FREE
            ) {
                ripemd160_t entry_hash;
                ripemd160((const_buffer_t)entry->file_name, sizeof(entry->file_name), entry_hash);
                *cache = ecache_insert(*cache, entry_hash, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, entry->cluster);
            }
        }

        ca = read_fat(ca, fi);
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -4;
}

int entry_search(
    const char* __restrict name, cluster_addr_t ca, ecache_t* __restrict cache, 
    directory_entry_t* __restrict meta, fat_data_t* __restrict fi
) {
    print_debug("entry_search(name=%s, ca=%u, cache=%s)", name, ca, cache != NO_ECACHE ? "YES" : "NO");
    if (cache != NO_ECACHE) {
        ripemd160_t entry_hash;
        ripemd160((const_buffer_t)name, 11, entry_hash);
        ecache_t* cached_entry = ecache_find(cache, entry_hash);
        if (!cached_entry) {
            create_entry(name, IS_ECACHE_DIR(cached_entry), ca, 0, meta, fi);
            return 1;
        }
    }

    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
        
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
            if (entry->file_name[0] == ENTRY_END) break;
            if (
                _validate_entry(entry) && 
                entry->file_name[0] != ENTRY_FREE
            ) {
                if (entry->name_hash != name_hash) continue;
                if (!str_strncmp((char*)entry->file_name, name, 11)) {
                    str_memcpy(meta, entry, sizeof(directory_entry_t));
                    pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                    if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                        print_error("Error correction of directory entry failed. Aborting...");
                        break;
                    }
                    
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return 1;
                }
            }
        }

        ca = read_fat(ca, fi);
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -4;
}

int entry_add(cluster_addr_t ca, ecache_t* __restrict cache, directory_entry_t* __restrict meta, fat_data_t* __restrict fi) {
    print_debug("entry_add(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    cluster_addr_t root_ca = ca;
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
    
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
            if (
                !_validate_entry(entry) || 
                entry->file_name[0] == ENTRY_FREE || 
                entry->file_name[0] == ENTRY_END
            ) {
                str_memcpy(entry, meta, sizeof(directory_entry_t));
                if (i + 1 < entries_per_cluster) (entry + 1)->file_name[0] = ENTRY_END;
                if (cache != NO_ECACHE) {
                    ripemd160_t entry_hash;
                    ripemd160((const_buffer_t)meta->file_name, sizeof(meta->file_name), entry_hash);
                    ecache_insert(cache, entry_hash, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, meta->cluster);
                }

                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                    print_error("Writing new directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -6;
                }

                free_s(decoded_cluster);
                free_s(cluster_data);
                return 1;
            }
        }

        cluster_addr_t nca = read_fat(ca, fi);
        if (is_cluster_bad(nca)) {
             print_error("<BAD> cluster in the chain. Aborting...");
            break;
        }

        if (is_cluster_end(nca)) {
            if (is_cluster_bad((nca = alloc_cluster(fi)))) {
                print_error("Allocation of new cluster failed. Aborting...");
                break;
            }

            if (!set_cluster_end(nca, fi)) {
                print_error("Can't set new cluster as <END>. Aborting...");
                break;
            }

            if (!write_fat(ca, nca, fi)) {
                print_error("Extension of the cluster chain with new cluster failed. Aborting...");
                break;
            }
        }

        ca = nca;
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -1;
}

int entry_edit(
    cluster_addr_t ca, const char* __restrict name, const directory_entry_t* __restrict meta, fat_data_t* __restrict fi
) {
    print_debug("entry_edit(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
        
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
            if (entry->file_name[0] == ENTRY_END) break;
            if (
                _validate_entry(entry) && 
                entry->file_name[0] != ENTRY_FREE
            ) {
                if (entry->name_hash != name_hash) continue;
                if (!str_strncmp((char*)entry->file_name, name, 11)) {
                    str_memcpy(entry, meta, sizeof(directory_entry_t));
                    pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                    if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                        print_error("Writing updated directory entry failed. Aborting...");
                        break;
                    }

                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return 1;
                }
            }
        }

        ca = read_fat(ca, fi);
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -2;
}

static int _entry_erase_rec(cluster_addr_t ca, int file, fat_data_t* fi) {
    print_debug("_entry_erase_rec(cluster=%u, file=%i)", ca, file);
    if (file) return dealloc_chain(ca, fi);
    else {
        int decoded_len = fi->cluster_size / sizeof(encoded_t);
        buffer_t cluster_data, decoded_cluster;
        if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
        cluster_addr_t nca = ca;
        unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
        while (!is_cluster_end(ca)) {
            if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
                break;
            }
            
            nca = read_fat(ca, fi);
            directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
            for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
                if (entry->file_name[0] == ENTRY_END) break;
                if (_validate_entry(entry) && entry->file_name[0] != ENTRY_FREE) {
                    if (_entry_erase_rec(entry->cluster, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, fi) < 0) {
                        print_warn("Recursive erase error!");
                    }
                }
            }

            if (!dealloc_cluster(ca, fi)) {
                print_warn("dealloc_cluster() encountered an error.");
            }

            ca = nca;
        }

        free_s(decoded_cluster);
        free_s(cluster_data);
        return 1;
    }

    return -2;
}

int entry_remove(cluster_addr_t ca, const char* __restrict name, fat_data_t* __restrict fi) {
    print_debug("entry_remove(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
        
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
            if (entry->file_name[0] == ENTRY_END) break;
            if (
                _validate_entry(entry) && 
                entry->file_name[0] != ENTRY_FREE
            ) {
                if (entry->name_hash != name_hash) continue;
                if (!str_strncmp((char*)entry->file_name, name, 11)) {
                    if (_entry_erase_rec(entry->cluster, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, fi) < 0) {
                        print_error("Cluster chain delete failed. Aborting...");
                        break;
                    }

                    entry->file_name[0] = ENTRY_FREE;
                    pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                    if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                        print_error("Writing updated directory entry failed. Aborting...");
                        break;
                    }

                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return 1;
                }
            }
        }

        ca = read_fat(ca, fi);
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -2;
}

int create_entry(
    const char* __restrict fullname, char is_dir, cluster_addr_t first_cluster, 
    unsigned int file_size, directory_entry_t* __restrict entry, fat_data_t* __restrict fi
) {
    entry->checksum = 0;
    entry->cluster = first_cluster;

    if (is_dir) entry->attributes |= FILE_DIRECTORY;
    else {
        entry->file_size  = file_size;
        entry->attributes |= FILE_ARCHIVE;
    }

    str_memcpy(entry->file_name, fullname, 11);
    entry->name_hash = crc32(0, (const_buffer_t)entry->file_name, 11);
    entry->checksum  = crc32(0, (const_buffer_t)entry, sizeof(directory_entry_t));
    print_debug("_create_entry=%.11s, is_dir=%i", entry->file_name, is_dir);
    return 1; 
}
