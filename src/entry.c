#include "../include/entry.h"

static int _validate_entry(directory_entry_t* entry) {
    checksum_t entry_checksum = entry->checksum;
    entry->checksum = 0;
    if (crc32(0, (buffer_t)entry, sizeof(directory_entry_t)) != entry_checksum) return 0;
    else entry->checksum = entry_checksum;
    return 1;
}

static int __read_encoded_cluster__(
    cluster_addr_t ca, buffer_t enc, int enc_size, buffer_t dec, int dec_size, fat_data_t* fi
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

int entry_search(const char* name, cluster_addr_t ca, directory_entry_t* meta, fat_data_t* fi) {
    print_debug("entry_search(name=%s, cluster=%u)", name, ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data)    free_s(cluster_data);
        if (decoded_cluster) free_s(decoded_cluster);
        return -1;
    }

    checksum_t name_hash = crc32(0, (const_buffer_t)name, str_strlen(name));
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

int entry_add(cluster_addr_t ca, directory_entry_t* meta, fat_data_t* fi) {
    print_debug("entry_add(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data    = (buffer_t)malloc_s(fi->cluster_size);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!cluster_data || !decoded_cluster) {
        print_error("malloc_s() error!");
        if (cluster_data) free_s(cluster_data);
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

int entry_edit(cluster_addr_t ca, const directory_entry_t* old, const directory_entry_t* new, fat_data_t* fi) {
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

    checksum_t name_hash = crc32(0, (const_buffer_t)old->file_name, str_strlen((char*)old->file_name));
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
                if (!str_strncmp((char*)entry->file_name, (char*)old->file_name, 11)) {
                    str_memcpy(entry, new, sizeof(directory_entry_t));
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
    print_debug("entry_erase_rec(cluster=%u, file=%i)", ca, file);
    if (file) {
        cluster_addr_t prev_cluster = 0;
        while (!is_cluster_end(ca) && !is_cluster_bad(ca) && !is_cluster_free(ca)) {
            cluster_addr_t next_cluster = read_fat(ca, fi);
            if (!dealloc_cluster(ca, fi)) {
                print_error("dealloc_cluster() encountered an error. Aborting...");
                return 0;
            }

            ca = next_cluster;
        }

        return 1;
    }
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

        unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
        while (!is_cluster_end(ca)) {
            if (!__read_encoded_cluster__(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
                break;
            }
            
            directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
            for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
                if (entry->file_name[0] == ENTRY_END) break;
                if (_validate_entry(entry) && entry->file_name[0] != ENTRY_FREE) {
                    if (entry_erase_rec(entry->cluster, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, fi) < 0) {
                        print_warn("Recursive erase error!");
                    }
                }
            }

            ca = read_fat(ca, fi);
        }

        if (!dealloc_cluster(ca, fi)) {
            print_warn("dealloc_cluster() encountered an error.");
        }

        free_s(decoded_cluster);
        free_s(cluster_data);
        return 1;
    }

    return -2;
}

int entry_remove(cluster_addr_t ca, const directory_entry_t* meta, fat_data_t* fi) {
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

    checksum_t name_hash = crc32(0, (const_buffer_t)meta->file_name, str_strlen((char*)meta->file_name));
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
                if (!str_strncmp((char*)entry->file_name, (char*)meta->file_name, 11)) {
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
    const char* name, const char* ext, char is_dir, cluster_addr_t first_cluster, 
    unsigned int file_size, directory_entry_t* entry, fat_data_t* fi
) {
    char tmp_filename[25] = { 0 };
    str_strcpy(tmp_filename, name);
    if (!is_dir) {
        str_strcat(tmp_filename, ".");
        str_strcat(tmp_filename, ext);
    }
    
    entry->cluster = first_cluster;
    if (!set_cluster_end(first_cluster, fi)) {
        print_error("Can't set allocated cluster as <END> for entry!");
        return 0;
    }

    if (is_dir) entry->attributes = FILE_DIRECTORY;
    else {
        entry->file_size  = file_size;
        entry->attributes = FILE_ARCHIVE;
    }

    name_to_fatname(tmp_filename, (char*)entry->file_name);
    entry->name_hash = crc32(0, (const_buffer_t)entry->file_name, str_strlen((char*)entry->file_name));

    entry->checksum = 0;
    entry->checksum = crc32(0, (const_buffer_t)entry, sizeof(directory_entry_t));
    print_debug("_create_entry=%s/%u", entry->file_name, entry->attributes);
    return 1; 
}