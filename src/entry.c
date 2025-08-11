#include "../include/entry.h"

static int _validate_entry(directory_entry_t* entry) {
    checksum_t entry_checksum = entry->checksum;
    entry->checksum = 0;
    if (murmur3_x86_32((buffer_t)entry, sizeof(directory_entry_t), 0) != entry_checksum) return 0;
    else entry->checksum = entry_checksum;
    return 1;
}

static int _read_encoded_cluster(
    cluster_addr_t ca, buffer_t __restrict enc, int enc_size, buffer_t __restrict dec, int dec_size, fat_data_t* __restrict fi
) {
    print_debug("_read_encoded_cluster(ca=%u)", ca);
    if (!enc || !dec || is_cluster_bad(ca)) return 0;
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

int entry_iterate(
    cluster_addr_t ca, int (*handler)(entry_info_t*, directory_entry_t*, void*), void* ctx, fat_data_t* __restrict fi
) {
    print_debug("entry_iterate(cluster=%u)", ca);
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);

    int exit = 0;
    do {
        if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
            break;
        }
        
        directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
        for (unsigned int i = 0; i < entries_per_cluster && !exit; i++, entry++) {
            if (entry->file_name[0] == ENTRY_END) break;
            entry_info_t info = { .ca = ca, .offset = i };
            exit = handler(&info, entry, ctx);
        }

        pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
        if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
            print_error("Error correction of directory entry failed. Aborting...");
            break;
        }
    } while (!is_cluster_end((ca = read_fat(ca, fi))) && !exit);

    free_s(decoded_cluster);
    free_s(cluster_data);
    return exit;
}

static int _index_handler(entry_info_t* info, directory_entry_t* entry, void* ctx) {
    ecache_t** context = (ecache_t**)ctx;
    if (!_validate_entry(entry) || entry->file_name[0] == ENTRY_FREE) {
        return 0;
    }

    checksum_t entry_hash = murmur3_x86_32((const_buffer_t)entry->file_name, sizeof(entry->file_name), 0);
    *context = ecache_insert(*context, entry_hash, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, entry->dca);
    return 0;
}

int entry_index(cluster_addr_t ca, ecache_t** __restrict cache, fat_data_t* __restrict fi) {
    print_debug("entry_index(cluster=%u)", ca);
    return entry_iterate(ca, _index_handler, (void*)cache, fi);
}

typedef struct {
    const char*        name;
    checksum_t         name_hash;
    directory_entry_t* meta;
    ecache_t*          index;
    fat_data_t*        fi; // filesystem info
    int                ji; // journal index
} entry_ctx_t;

static int _search_handler(entry_info_t* info, directory_entry_t* entry, void* ctx) {
    entry_ctx_t* context = (entry_ctx_t*)ctx;
    if (!_validate_entry(entry) || entry->file_name[0] == ENTRY_FREE) return 0;
    if (context->name_hash != entry->name_hash) return 0;
    if (str_strncmp(context->name, (char*)entry->file_name, 11)) return 0;
    if (context->meta) {
        str_memcpy(context->meta, entry, sizeof(directory_entry_t));
        context->meta->rca = info->ca;
    }

    return 1;
}

int entry_search(
    const char* __restrict name, cluster_addr_t ca, ecache_t* __restrict cache, directory_entry_t* __restrict meta, fat_data_t* __restrict fi
) {
    print_debug("entry_search(name=%s, ca=%u, cache=%s)", name, ca, cache != NO_ECACHE ? "YES" : "NO");
    if (cache != NO_ECACHE) {
        checksum_t entry_hash = murmur3_x86_32((const_buffer_t)name, 11, 0);
        ecache_t* cached_entry = ecache_find(cache, entry_hash);
        if (cached_entry) {
            if (meta) create_entry(name, IS_ECACHE_DIR(cached_entry), cached_entry->ca, 0, meta);
            return 1;
        }
    }

    entry_ctx_t ctx = { .meta = meta, .name = name, .name_hash = murmur3_x86_32((const_buffer_t)name, 11, 0) };
    return entry_iterate(ca, _search_handler, (void*)&ctx, fi);
}

#ifndef NIFAT32_RO
static int _edit_handler(entry_info_t* info, directory_entry_t* entry, void* ctx) {
    entry_ctx_t* context = (entry_ctx_t*)ctx;
    if (!_validate_entry(entry) || entry->file_name[0] == ENTRY_FREE) return 0;
    if (context->name_hash != entry->name_hash) return 0;
    if (str_strncmp((char*)entry->file_name, context->name, 11)) return 0;

    context->ji = journal_add_operation(EDIT_OP, info->ca, info->offset, (unsqueezed_entry_t*)context->meta, context->fi);
    if (context->index != NO_ECACHE) {
        checksum_t src, dst;
        src = murmur3_x86_32((const_buffer_t)context->name, 11, 0);
        ecache_delete(context->index, src);
        dst = murmur3_x86_32((const_buffer_t)entry->file_name, sizeof(entry->file_name), 0);
        ecache_insert(context->index, dst, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, entry->dca);
    }

    str_memcpy(entry, context->meta, sizeof(directory_entry_t));
    return 1;
}
#endif

int entry_edit(
    cluster_addr_t ca, ecache_t* __restrict cache, const char* __restrict name, const directory_entry_t* __restrict meta, fat_data_t* __restrict fi
) {
#ifndef NIFAT32_RO
    print_debug("entry_edit(cluster=%u, cache=%s)", ca, cache != NO_ECACHE ? "YES" : "NO");
    entry_ctx_t context = { 
        .meta = (directory_entry_t*)meta, 
        .name = name, 
        .name_hash = murmur3_x86_32((const_buffer_t)name, 11, 0), 
        .index = cache, .fi = fi 
    };

    int result = entry_iterate(ca, _edit_handler, (void*)&context, fi);
    journal_solve_operation(context.ji, fi);
    return result;
#else
    return 1;
#endif
}

int entry_add(cluster_addr_t ca, ecache_t* __restrict cache, directory_entry_t* __restrict meta, fat_data_t* __restrict fi) {
#ifndef NIFAT32_RO
    print_debug("entry_add(ca=%u, cache=%s)", ca, cache != NO_ECACHE ? "YES" : "NO");
    int decoded_len = fi->cluster_size / sizeof(encoded_t);
    buffer_t cluster_data, decoded_cluster;
    if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
    
    unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    do {
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
                int ji = journal_add_operation(ADD_OP, ca, i, (unsqueezed_entry_t*)meta, fi);
                str_memcpy(entry, meta, sizeof(directory_entry_t));
                if (i + 1 < entries_per_cluster) (entry + 1)->file_name[0] = ENTRY_END;
                if (cache != NO_ECACHE) {
                    checksum_t entry_hash = murmur3_x86_32((const_buffer_t)meta->file_name, sizeof(meta->file_name), 0);
                    ecache_insert(cache, entry_hash, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, meta->dca);
                }

                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, fi->cluster_size, fi)) {
                    print_error("Writing new directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -6;
                }

                journal_solve_operation(ji, fi);
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
    } while (!is_cluster_end(ca));

    free_s(decoded_cluster);
    free_s(cluster_data);
    return -1;
#else
    return 1;
#endif
}

#ifndef NIFAT32_RO
static int _entry_erase_rec(cluster_addr_t ca, int file, fat_data_t* fi) {
    print_debug("_entry_erase_rec(cluster=%u, file=%i)", ca, file);
    if (file) return dealloc_chain(ca, fi);
    else {
        int decoded_len = fi->cluster_size / sizeof(encoded_t);
        buffer_t cluster_data, decoded_cluster;
        if (!_allocate_buffers(&cluster_data, fi->cluster_size, &decoded_cluster, decoded_len)) return -1;
        cluster_addr_t nca = ca;
        unsigned int entries_per_cluster = (fi->cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
        do {
            if (!_read_encoded_cluster(ca, cluster_data, fi->cluster_size, decoded_cluster, decoded_len, fi)) {
                break;
            }
            
            nca = read_fat(ca, fi);
            directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
            for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
                if (entry->file_name[0] == ENTRY_END) break;
                if (_validate_entry(entry) && entry->file_name[0] != ENTRY_FREE) {
                    if (_entry_erase_rec(entry->dca, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, fi) < 0) {
                        print_warn("Recursive erase error!");
                    }
                }
            }

            if (!dealloc_cluster(ca, fi)) {
                print_warn("dealloc_cluster() encountered an error.");
            }

            ca = nca;
        } while (!is_cluster_end(ca));

        free_s(decoded_cluster);
        free_s(cluster_data);
        return 1;
    }

    return -2;
}

static int _remove_handler(entry_info_t* info, directory_entry_t* entry, void* ctx) {
    entry_ctx_t* context = (entry_ctx_t*)ctx;
    if (!_validate_entry(entry) || entry->file_name[0] == ENTRY_FREE) return 0;
    if (str_strncmp((char*)entry->file_name, context->name, 11)) return 0;

    if (context->index != NO_ECACHE) {
        checksum_t src, dst;
        src = murmur3_x86_32((const_buffer_t)context->name, 11, 0);
        ecache_delete(context->index, src);
        dst = murmur3_x86_32((const_buffer_t)entry->file_name, sizeof(entry->file_name), 0);
        ecache_insert(context->index, dst, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, entry->dca);
    }

    context->ji = journal_add_operation(DEL_OP, info->ca, info->offset, (unsqueezed_entry_t*)entry, context->fi);
    if (_entry_erase_rec(entry->dca, (entry->attributes & FILE_DIRECTORY) != FILE_DIRECTORY, context->fi) < 0) {
        print_error("Cluster chain delete failed. Aborting...");
        return 1;
    }

    entry->file_name[0] = ENTRY_FREE;
    return 1;
}
#endif

int entry_remove(cluster_addr_t ca, const char* __restrict name, ecache_t* __restrict cache, fat_data_t* __restrict fi) {
#ifndef NIFAT32_RO
    print_debug("entry_remove(cluster=%u, cache=%s)", ca, cache != NO_ECACHE ? "YES" : "NO");
    entry_ctx_t ctx = { .name = name, .fi = fi, .index = cache };
    int result = entry_iterate(ca, _remove_handler, (void*)&ctx, fi);
    journal_solve_operation(ctx.ji, fi);
    return result;
#else
    return 1;
#endif
}

int create_entry(
    const char* __restrict fullname, char is_dir, cluster_addr_t first_cluster, 
    unsigned int file_size, directory_entry_t* __restrict entry
) {
    entry->checksum = 0;
    entry->rca = FAT_CLUSTER_BAD;
    entry->dca = first_cluster;

    if (is_dir) {
        entry->file_size  = 1;
        entry->attributes = FILE_DIRECTORY;
    }
    else {
        entry->file_size  = file_size;
        entry->attributes = FILE_ARCHIVE;
    }

    str_memcpy(entry->file_name, fullname, 11);
    entry->name_hash = murmur3_x86_32((const_buffer_t)entry->file_name, sizeof(entry->file_name), 0);
    entry->checksum  = murmur3_x86_32((const_buffer_t)entry, sizeof(directory_entry_t), 0);
    print_debug("_create_entry=%.11s, is_dir=%i, fca=%u", entry->file_name, is_dir, first_cluster);
    return 1; 
}
