#include "../include/entry.h"

static int _validate_entry(directory_entry_t* entry) {
    checksum_t entry_checksum = entry->checksum;
    entry->checksum = 0;
    if (crc32(0, (buffer_t)entry, sizeof(directory_entry_t)) != entry_checksum) return 0;
    else entry->checksum = entry_checksum;
    return 1;
}

static int __read_encoded_cluster__(
    cluster_addr_t ca, buffer_t __restrict enc, int enc_size, buffer_t __restrict dec, int dec_size, fat_data_t* __restrict fi
) {
    print_debug("__read_encoded_cluster__(ca=%u)", ca);
    if (!enc || !dec) return 0;
    if (!read_cluster(ca, enc, enc_size, fi)) {
        print_error("read_cluster() encountered an error. Aborting...");
        return 0;
    }

    unpack_memory((const encoded_t*)enc, dec, dec_size);
    return 1;
}

typedef struct {
    cluster_addr_t root_ca;
    directory_entry_t entry;
    short used;
    char free;
} entry_cache_t;

lock_t _cache_lock = NULL_LOCK;
static int _cache_size = 0;
static entry_cache_t* _entry_cache = NULL;

int entry_cache_init(int cache_size) {
    _entry_cache = (entry_cache_t*)malloc_s(sizeof(directory_entry_t*) * cache_size);
    if (!_entry_cache) {
        print_error("malloc_s() error!");
        return 0;
    }

    _cache_size = cache_size;
    for (int i = 0; i < cache_size; i++) {
        _entry_cache[i].free = 1;
    }

    return 1;
}

int entry_cache_unload() {
    if (!_entry_cache) return 0;
    free_s(_entry_cache);
    return 0;
}

static int _cache_entry(cluster_addr_t ca, directory_entry_t* entry) {
    print_debug("_cache_entry(ca=%u, entry=%.11s)", ca, entry->file_name);
    if (!_entry_cache) return 0;
    if (THR_require_write(&_cache_lock, get_thread_num())) {
        int cached = 0;
        for (int i = 0; i < _cache_size; i++) {
            if (_entry_cache[i].free || _entry_cache[i].used-- < 0) {
                str_memcpy(&_entry_cache[i].entry, entry, sizeof(directory_entry_t));
                _entry_cache[i].used = 16;
                _entry_cache[i].free = 0;
                cached = 1;
                break;
            }
        }

        THR_release_write(&_cache_lock, get_thread_num());
        return cached;
    }

    return 0;
}

int _get_cached_entry(const char* name, checksum_t name_hash, cluster_addr_t ca, directory_entry_t* entry) {
    print_debug("_get_cached_entry(name=%s, ca=%u)", name, ca);
    if (!_entry_cache) return 0;
    if (THR_require_read(&_cache_lock)) {
        int loaded = 0;
        for (int i = 0; i < _cache_size; i++) {
            if (_entry_cache[i].free) continue;
            if (_entry_cache[i].root_ca != ca) continue;
            if (_entry_cache[i].entry.name_hash != name_hash) continue;
            if (!str_strncmp((char*)_entry_cache[i].entry.file_name, name, 11)) {
                _entry_cache[i].used++;
                str_memcpy(entry, &_entry_cache[i].entry, sizeof(directory_entry_t));
                loaded = 1;
                break;
            }
        }

        THR_release_read(&_cache_lock);
        return loaded;
    }

    return 0;
}

int entry_search(const char* __restrict name, cluster_addr_t ca, directory_entry_t* __restrict meta, fat_data_t* __restrict fi) {
    print_debug("entry_search(name=%s, cluster=%u)", name, ca);
    cluster_addr_t root_ca = ca;
    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    if (_get_cached_entry(name, name_hash, ca, meta)) return 1;
    
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data)    free_s(cluster_data);
        if (decoded_cluster) free_s(decoded_cluster);
        return -1;
    }

    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
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
                    
                    _cache_entry(root_ca, entry);
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

/* TODO: cache */
int entry_add(cluster_addr_t ca, directory_entry_t* __restrict meta, fat_data_t* __restrict fi) {
    print_debug("entry_add(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data)    free_s(cluster_data);
        if (decoded_cluster) free_s(decoded_cluster);
        return -1;
    }

    cluster_addr_t root_ca = ca;
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
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
                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                    print_error("Writing new directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -6;
                }

                _cache_entry(root_ca, entry);
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
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data) free_s(cluster_data);
        if (decoded_cluster) free_s(decoded_cluster);
        return -1;
    }

    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
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
        buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
        buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
        if (!cluster_data || !decoded_cluster) {
            print_error("malloc_s() error!");
            if (cluster_data)    free_s(cluster_data);
            if (decoded_cluster) free_s(decoded_cluster);
            return -1;
        }

        cluster_addr_t nca = ca;
        unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
        while (!is_cluster_end(ca)) {
            if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
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
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data) free_s(cluster_data);
        if (decoded_cluster) free_s(decoded_cluster);
        return -1;
    }

    checksum_t name_hash = crc32(0, (const_buffer_t)name, 11);
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    while (!is_cluster_end(ca)) {
        if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
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

    if (is_dir) entry->attributes = FILE_DIRECTORY;
    else {
        entry->file_size  = file_size;
        entry->attributes = FILE_ARCHIVE;
    }

    str_memcpy(entry->file_name, fullname, 11);
    entry->name_hash = crc32(0, (const_buffer_t)entry->file_name, 11);
    entry->checksum  = crc32(0, (const_buffer_t)entry, sizeof(directory_entry_t));
    print_debug("_create_entry=%.11s, is_dir=%i", entry->file_name, is_dir);
    return 1; 
}
