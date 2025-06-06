#include "nifat32.h"

static fat_data_t _fs_data = { 0 };
static content_t* _content_table[CONTENT_TABLE_SIZE] = { NULL };

int NIFAT32_init(int bs_num, unsigned int ts) {
    if (bs_num > 5) {
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

    if (!DSK_read_sector(GET_BOOTSECTOR(bs_num, ts), encoded_bs, sector_size)) {
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
    fat_extBS_32_t* ext_bootstruct = (fat_extBS_32_t*)bootstruct->extended_section;

    checksum_t bcheck = bootstruct->checksum;
    bootstruct->checksum = 0;
    checksum_t exbcheck = ext_bootstruct->checksum;
    ext_bootstruct->checksum = 0;

    ext_bootstruct->checksum = crc32(0, (buffer_t)ext_bootstruct, sizeof(fat_extBS_32_t));
    bootstruct->checksum     = crc32(0, (buffer_t)bootstruct, sizeof(fat_BS_t));
    if (bootstruct->checksum != bcheck || ext_bootstruct->checksum != exbcheck) {
        print_error(
            "Checksum check error! %u != %u or %u != %u. Moving to reserved sector!", 
            bootstruct->checksum, bcheck, ext_bootstruct->checksum, exbcheck
        );
        return NIFAT32_init(bs_num + 1, ts);
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
    _fs_data.first_fat_sector = bootstruct->reserved_sector_count;
    _fs_data.ext_root_cluster = ext_bootstruct->root_cluster;
    _fs_data.cluster_size = _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster;
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) {
        _content_table[i] = NULL;
    }

    print_debug("NIFAT32 init success! Stats from boot sector:");
    print_debug("FAT type:                  %i", _fs_data.fat_type);
    print_debug("Bytes per sector:          %u", _fs_data.bytes_per_sector);
    print_debug("Sectors per cluster:       %u", _fs_data.sectors_per_cluster);
    print_debug("Reserved sectors:          %u", bootstruct->reserved_sector_count);
    print_debug("Number of FATs:            %u", bootstruct->table_count);
    print_debug("FAT size (in sectors):     %u", _fs_data.fat_size);
    print_debug("Total sectors:             %u", _fs_data.total_sectors);
    print_debug("Root entry count:          %u", bootstruct->root_entry_count);
    print_debug("Root dir sectors:          %d", root_dir_sectors);
    print_debug("Data sectors:              %d", data_sectors);
    print_debug("Total clusters:            %u", _fs_data.total_clusters);
    print_debug("First FAT sector:          %u", _fs_data.first_fat_sector);
    print_debug("First data sector:         %u", _fs_data.first_data_sector);
    print_debug("Root cluster (FAT32):      %u", _fs_data.ext_root_cluster);
    print_debug("Cluster size (in bytes):   %u", _fs_data.cluster_size);

    if (bs_num > 0) {
        if (!DSK_write_sector(GET_BOOTSECTOR(0, ts), (const_buffer_t)encoded_bs, sector_size)) {
            print_warn("Attempt for bootsector restore failed!");
        }
    }

    free_s(encoded_bs);
    free_s(decoded_bs);
    return 1;
}

static int _validate_entry(directory_entry_t* entry) {
    checksum_t entry_checksum = entry->checksum;
    entry->checksum = 0;
    if (crc32(0, (buffer_t)entry, sizeof(directory_entry_t)) != entry_checksum) return 0;
    else entry->checksum = entry_checksum;
    return 1;
}

static int _entry_search(const char* name, cluster_addr_t ca, directory_entry_t* meta) {
    print_debug("_entry_search(name=%s, cluster=%u)", name, ca);
    buffer_t cluster_data = (buffer_t)malloc_s(_fs_data.cluster_size);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!read_cluster(ca, cluster_data, _fs_data.cluster_size, &_fs_data)) {
        print_error("read_cluster() encountered an error. Aborting...");
        free_s(cluster_data);
        return -2;
    }

    int decoded_len = _fs_data.cluster_size / sizeof(encoded_t);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!decoded_cluster) {
        print_error("malloc_s() error!");
        free_s(cluster_data);
        return -3;
    }

    unpack_memory((const encoded_t*)cluster_data, decoded_cluster, decoded_len);
    directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
    unsigned int entries_per_cluster = (_fs_data.cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
        if (entry->file_name[0] == ENTRY_END) break;
        if (
            _validate_entry(entry) && 
            entry->file_name[0] != ENTRY_FREE
        ) {
            if (!str_strncmp((char*)entry->file_name, name, 11)) {
                str_memcpy(meta, entry, sizeof(directory_entry_t));
                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, _fs_data.cluster_size, &_fs_data)) {
                    print_error("Error correction of directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -4;
                }
                
                free_s(decoded_cluster);
                free_s(cluster_data);
                return 1;
            }
        }
    }

    cluster_addr_t nca = read_fat(ca, &_fs_data);
    if (is_cluster_end(nca)) {
        free_s(decoded_cluster);
        free_s(cluster_data);
        return -5;
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return _entry_search(name, ca, meta);
}

static int _entry_add(cluster_addr_t ca, directory_entry_t* meta) {
    print_debug("_entry_add(cluster=%u)", ca);
    buffer_t cluster_data = malloc_s(_fs_data.cluster_size);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!read_cluster(ca, (buffer_t)cluster_data, _fs_data.cluster_size, &_fs_data)) {
        print_error("read_cluster() encountered an error. Aborting...");
        free_s(cluster_data);
        return -2;
    }

    int decoded_len = _fs_data.cluster_size / sizeof(encoded_t);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!decoded_cluster) {
        print_error("malloc_s() error!");
        free_s(cluster_data);
        return -3;
    }

    unpack_memory((const encoded_t*)cluster_data, decoded_cluster, decoded_len);
    directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
    unsigned int entries_per_cluster = (_fs_data.cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
        if (
            !_validate_entry(entry) || 
            entry->file_name[0] == ENTRY_FREE || 
            entry->file_name[0] == ENTRY_END
        ) {
            str_memcpy(entry, meta, sizeof(directory_entry_t));
            if (i + 1 < entries_per_cluster) (entry + 1)->file_name[0] = ENTRY_END;
            pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
            if (!write_cluster(ca, cluster_data, _fs_data.cluster_size, &_fs_data)) {
                print_error("Writing new directory entry failed. Aborting...");
                free_s(decoded_cluster);
                free_s(cluster_data);
                return -5;
            }

            free_s(decoded_cluster);
            free_s(cluster_data);
            return 1;
        }
    }

    cluster_addr_t nca = read_fat(ca, &_fs_data);
    if (is_cluster_end(nca)) {
        if (is_cluster_bad((nca = alloc_cluster(&_fs_data)))) {
            print_error("Allocation of new cluster failed. Aborting...");
            free_s(decoded_cluster);
            free_s(cluster_data);
            return -2;
        }

        if (!set_cluster_end(nca, &_fs_data)) {
            print_error("Can't set new cluster as <END>. Aborting...");
            free_s(decoded_cluster);
            free_s(cluster_data);
            return -3;
        }

        if (!write_fat(ca, nca, &_fs_data)) {
            print_error("Extension of the cluster chain with new cluster failed. Aborting...");
            free_s(decoded_cluster);
            free_s(cluster_data);
            return -4;
        }
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return _entry_add(nca, meta);
}

static int _entry_edit(cluster_addr_t ca, const directory_entry_t* old, const directory_entry_t* new) {
    print_debug("_entry_edit(cluster=%u)", ca);
    buffer_t cluster_data = malloc_s(_fs_data.cluster_size);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!read_cluster(ca, (buffer_t)cluster_data, _fs_data.cluster_size, &_fs_data)) {
        print_error("read_cluster() encountered an error. Aborting...");
        free_s(cluster_data);
        return -2;
    }

    int decoded_len = _fs_data.cluster_size / sizeof(encoded_t);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!decoded_cluster) {
        print_error("malloc_s() error!");
        free_s(cluster_data);
        return -3;
    }

    unpack_memory((const encoded_t*)cluster_data, decoded_cluster, decoded_len);
    directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
    unsigned int entries_per_cluster = (_fs_data.cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
        if (entry->file_name[0] == ENTRY_END) break;
        if (
            _validate_entry(entry) && 
            entry->file_name[0] != ENTRY_FREE
        ) {
            if (!str_strncmp((char*)entry->file_name, (char*)old->file_name, 11)) {
                str_memcpy(entry, new, sizeof(directory_entry_t));
                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, _fs_data.cluster_size, &_fs_data)) {
                    print_error("Writing updated directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -4;
                }

                free_s(decoded_cluster);
                free_s(cluster_data);
                return 1;
            }
        }
    }

    cluster_addr_t nca = read_fat(ca, &_fs_data);
    if (is_cluster_end(nca)) {
        print_error("<END> of cluster chain reached. file_t not found. Aborting...");
        free_s(decoded_cluster);
        free_s(cluster_data);
        return -5;
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return _entry_edit(nca, old, new);
}

static int _entry_remove(cluster_addr_t ca, const directory_entry_t* meta) {
    print_debug("_entry_remove(cluster=%u)", ca);
    buffer_t cluster_data = malloc_s(_fs_data.cluster_size);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!read_cluster(ca, (buffer_t)cluster_data, _fs_data.cluster_size, &_fs_data)) {
        print_error("read_cluster() encountered an error. Aborting...");
        free_s(cluster_data);
        return -2;
    }

    int decoded_len = _fs_data.cluster_size / sizeof(encoded_t);
    buffer_t decoded_cluster = (buffer_t)malloc_s(decoded_len);
    if (!decoded_cluster) {
        print_error("malloc_s() error!");
        free_s(cluster_data);
        return -3;
    }

    unpack_memory((const encoded_t*)cluster_data, decoded_cluster, decoded_len);
    directory_entry_t* entry = (directory_entry_t*)decoded_cluster;
    unsigned int entries_per_cluster = (_fs_data.cluster_size / sizeof(encoded_t)) / sizeof(directory_entry_t);
    for (unsigned int i = 0; i < entries_per_cluster; i++, entry++) {
        if (entry->file_name[0] == ENTRY_END) break;
        if (
            _validate_entry(entry) && 
            entry->file_name[0] != ENTRY_FREE
        ) {
            if (!str_strncmp((char*)entry->file_name, (char*)meta->file_name, 11)) {
                entry->file_name[0] = ENTRY_FREE;
                pack_memory(decoded_cluster, (encoded_t*)cluster_data, decoded_len);
                if (!write_cluster(ca, cluster_data, _fs_data.cluster_size, &_fs_data)) {
                    print_error("Writing updated directory entry failed. Aborting...");
                    free_s(decoded_cluster);
                    free_s(cluster_data);
                    return -3;
                }

                free_s(decoded_cluster);
                free_s(cluster_data);
                return 1;
            }
        }
    }

    cluster_addr_t nca = read_fat(ca, &_fs_data);
    if (is_cluster_end(nca)) {
        print_error("<END> of cluster chain reached. file_t not found. Aborting...");
        free_s(decoded_cluster);
        free_s(cluster_data);
        return -5;
    }

    free_s(decoded_cluster);
    free_s(cluster_data);
    return _entry_remove(nca, meta);
}

static int _create_entry(
    const char* name, const char* ext, char is_dir, cluster_addr_t first_cluster, unsigned int file_size, directory_entry_t* entry
) {
    char tmp_filename[25] = { 0 };
    str_strcpy(tmp_filename, name);
    if (!is_dir) {
        str_strcat(tmp_filename, ".");
        str_strcat(tmp_filename, ext);
    }
    
    entry->cluster = first_cluster;
    if (!set_cluster_end(first_cluster, &_fs_data)) {
        print_error("Can't set allocated cluster as <END> for entry!");
        return 0;
    }

    if (is_dir) entry->attributes = FILE_DIRECTORY;
    else {
        entry->file_size  = file_size;
        entry->attributes = FILE_ARCHIVE;
    }

    name_to_fatname(tmp_filename, (char*)entry->file_name);

    entry->checksum = 0;
    entry->checksum = crc32(0, (buffer_t)entry, sizeof(directory_entry_t));
    print_debug("_create_entry=%s/%u", entry->file_name, entry->attributes);
    return 1; 
}

static content_t* _create_content() {
    content_t* content = (content_t*)malloc_s(sizeof(content_t));
    if (!content) return NULL;
    content->directory = NULL;
    content->file = NULL;
    content->parent_cluster = FAT_CLUSTER_BAD;
    return content;
}

static directory_t* _create_directory() {
    directory_t* directory = (directory_t*)malloc_s(sizeof(directory_t));
    if (!directory) return NULL;
    return directory;
}

static file_t* _create_file() {
    file_t* file = (file_t*)malloc_s(sizeof(file_t));
    if (!file) return NULL;
    return file;
}

static int _unload_file_system(file_t* file) {
    if (!file) return -1;
    free_s(file);
    return 1;
}

static int _unload_directory_system(directory_t* directory) {
    if (!directory) return -1;
    free_s(directory);
    return 1;
}

static int _unload_content_system(content_t* content) {
    if (!content) return -1;
    if (content->content_type == CONTENT_TYPE_DIRECTORY) _unload_directory_system(content->directory);
    else if (content->content_type == CONTENT_TYPE_FILE) _unload_file_system(content->file);
    free_s(content);
    return 1;
}

static ci_t _add_content2table(content_t* content) {
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) {
        if (!_content_table[i]) {
            _content_table[i] = content;
            return i;
        }
    }

    return -1;
}

static int _remove_content_from_table(ci_t ci) {
    if (!_content_table[ci]) return -1;
    int result = _unload_content_system(_content_table[ci]);
    _content_table[ci] = NULL;
    return result;
}

static content_t* _get_content_from_table(const ci_t ci) {
    return _content_table[ci];
}

static cluster_addr_t _get_cluster_by_path(const char* path, directory_entry_t* entry, cluster_addr_t* parent) {
    print_debug("_get_cluster_by_path(path=%s)", path);

    unsigned int start = 0;
    cluster_addr_t parent_cluster = _fs_data.ext_root_cluster;
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;

    directory_entry_t file_info;
    for (unsigned int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == '/' || path[iterator] == '\0') {
            char name_buffer[256] = { 0 };
            str_memcpy(name_buffer, path + start, iterator - start);

            char fatname_buffer[128] = { 0 };
            name_to_fatname(name_buffer, fatname_buffer);
            if (_entry_search(fatname_buffer, active_cluster, &file_info) < 0) return FAT_CLUSTER_BAD;
            start = iterator + 1;

            parent_cluster = active_cluster;
            active_cluster = file_info.cluster;
        }
    }

    if (entry) str_memcpy(entry, &file_info, sizeof(directory_entry_t));
    if (parent) *parent = parent_cluster;

    return active_cluster;    
}

int NIFAT32_content_exists(const char* path) {
    return _get_cluster_by_path(path, NULL, NULL) != FAT_CLUSTER_BAD;
}

ci_t NIFAT32_open_content(const char* path) {
    print_debug("NIFAT32_open_content(path=%s)", path);
    content_t* fat_content = _create_content();
    if (!fat_content) {
        print_error("_create_content() error!");
        return -1;
    }
    
    cluster_addr_t cluster = _get_cluster_by_path(path, &fat_content->meta, &fat_content->parent_cluster);
    if (is_cluster_bad(cluster)) {
        print_error("Entry not found!");
        _unload_content_system(fat_content);
        return -2;
    }
    else {
        print_debug("NIFAT32_open_content: Content cluster is: %u", cluster);
    }
    
    fat_content->data_cluster = cluster;
    if ((fat_content->meta.attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {
        fat_content->file = _create_file();
        if (!fat_content->file) {
            print_error("_create_file() error!");
            _unload_content_system(fat_content);
            return -3;
        }

        fat_content->content_type = CONTENT_TYPE_FILE;             
        str_memcpy(fat_content->file->name, fat_content->meta.file_name, 11);
    }
    else {
        fat_content->directory = _create_directory();
        if (!fat_content->directory) {
            _unload_content_system(fat_content);
            return -4;
        }

        fat_content->content_type = CONTENT_TYPE_DIRECTORY;
        str_memcpy(fat_content->directory->name, fat_content->meta.file_name, 11);
    }

    ci_t ci = _add_content2table(fat_content);
    if (ci < 0) {
        print_error("An error occurred in _add_content2table(). Aborting...");
        _unload_content_system(fat_content);
        return -5;
    }

    return ci;
}

int NIFAT32_close_content(ci_t ci) {
    return _remove_content_from_table(ci);
}

int NIFAT32_read_content2buffer(const ci_t ci, cluster_offset_t offset, buffer_t buffer, int buff_size) {
    print_debug("NIFAT32_read_content2buffer(ci=%i, offset=%u, readsize=%i)", ci, offset, buff_size);
    content_t* content = _get_content_from_table(ci);
    if (!content || content->content_type != CONTENT_TYPE_FILE) {
        print_warn("Content or file not found!");
        return 0;
    }

    int total_readden = 0;
    cluster_addr_t ca = content->data_cluster;
    while (!is_cluster_end(ca) && !is_cluster_bad(ca) && buff_size > 0) {
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
    }

    return total_readden;
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
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        print_error("Content not found by ci=%i", ci);
        return FAT_CLUSTER_BAD;
    }

    int max_iterations = _fs_data.total_clusters;
    cluster_addr_t cluster = content->data_cluster;
    if (lca == FAT_CLUSTER_BAD) {
        while (!is_cluster_end(cluster) && !is_cluster_bad(cluster) && max_iterations-- > 0) {
            lca = cluster;
            cluster = read_fat(cluster, &_fs_data);
        }
    }

    if (max_iterations <= 0) {
        print_error("Can't allocate cluster!");
        return FAT_CLUSTER_BAD;
    }

    cluster_addr_t allocated_cluster = alloc_cluster(&_fs_data);
    if (!is_cluster_bad(allocated_cluster)) {
        if (!set_cluster_end(allocated_cluster, &_fs_data)) {
            print_error("Can't set cluster to <END> state!");
            dealloc_cluster(allocated_cluster, &_fs_data);
            return FAT_CLUSTER_BAD;
        }

        if (!write_fat(lca, allocated_cluster, &_fs_data)) {
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

int NIFAT32_write_buffer2content(const ci_t ci, cluster_offset_t offset, const_buffer_t data, int data_size) {
    print_debug("NIFAT32_write_buffer2content(ci=%i, offset=%u, writesize=%i)", ci, offset, data_size);
    content_t* content = _get_content_from_table(ci);
    if (!content || content->content_type != CONTENT_TYPE_FILE) {
        print_warn("Content or file not found!");
        return 0;
    }

    int total_written = 0;
    cluster_addr_t ca = content->data_cluster;
    cluster_addr_t lca = ca;
    while (!is_cluster_end(ca) && !is_cluster_bad(ca) && data_size > 0) {
        if (offset > _fs_data.cluster_size) offset -= _fs_data.cluster_size;
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
    }

    ca = lca;
    while (data_size > 0 && !is_cluster_bad(ca = _add_cluster_to_content(ci, ca))) {
        int writable = (data_size > (int)_fs_data.cluster_size) ? (int)_fs_data.cluster_size : data_size;
        write_cluster(ca, data + total_written, writable, &_fs_data);

        data_size -= writable;
        total_written += writable;
    }

    return total_written;
}

int NIFAT32_change_meta(const ci_t ci, const cinfo_t* info) {
    print_debug("NIFAT32_change_meta(ci=%i, info=%s/%s/%s)", ci, info->full_name, info->file_name, info->file_extension);
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        print_error("Content not found!");
        return 0;
    }

    directory_entry_t new_meta;
    _create_entry(
        info->file_name, info->file_extension, 
        info->type == STAT_DIR, content->meta.cluster, 
        content->meta.file_size, &new_meta
    );

    if (!_entry_edit(content->parent_cluster, &content->meta, &new_meta)) {
        print_error("_entry_edit() encountered an error. Aborting...");
        return 0;
    }
    
    return 1;
}

int NIFAT32_put_content(const ci_t ci, cinfo_t* info) {
    print_debug("NIFAT32_put_content(ci=%i, info=%s/%s/%s)", ci, info->full_name, info->file_name, info->file_extension);
    cluster_addr_t target = _fs_data.ext_root_cluster;
    if (ci != PUT_TO_ROOT) {
        content_t* content = _get_content_from_table(ci);
        if (!content) {
            print_error("Content not found!");
            return 0;
        }

        target = content->meta.cluster;
    }

    int is_found = _entry_search((char*)info->full_name, target, NULL);
    if (is_found < 0 && is_found != -5) {
        print_error("_entry_search() encountered an error [%i]. Aborting...", is_found);
        return 0;
    }

    directory_entry_t new_meta = { 0 };
    _create_entry(info->file_name, info->file_extension, info->type == STAT_DIR, alloc_cluster(&_fs_data), 1, &new_meta);
    int is_add = _entry_add(target, &new_meta);
    if (is_add < 0) {
        print_error("_entry_add() encountered an error [%i]. Aborting...", is_add);
        return 0;
    }

    return 1;
}

/* TODO: recursive delete for dirs */
int NIFAT32_delete_content(ci_t ci) {
    print_debug("NIFAT32_delete_content(ci=%i)", ci);
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        print_error("Content not found!");
        NIFAT32_close_content(ci);
        return 0;
    }

    cluster_addr_t prev_cluster = 0;
    cluster_addr_t data_cluster = content->data_cluster;
    while (!is_cluster_end(data_cluster) && !is_cluster_bad(data_cluster) && !is_cluster_free(data_cluster)) {
        cluster_addr_t next_cluster = read_fat(data_cluster, &_fs_data);
        if (!dealloc_cluster(data_cluster, &_fs_data)) {
            print_error("dealloc_cluster() encountered an error. Aborting...");
            NIFAT32_close_content(ci);
            return 0;
        }

        data_cluster = next_cluster;
    }
   
    if (!_entry_remove(content->parent_cluster, &content->meta)) {
        print_error("_entry_remove() encountered an error. Aborting...");
        NIFAT32_close_content(ci);
        return 0;
    }

    NIFAT32_close_content(ci);
    return 1;
}

int NIFAT32_stat_content(const ci_t ci, cinfo_t* info) {
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        print_error("Content ci=%i not found!", ci);
        info->type = NOT_PRESENT;
        return 0;
    }

    if (content->content_type == CONTENT_TYPE_DIRECTORY) {
        info->size = 0;
        str_memcpy(info->full_name, content->directory->name, 11);
        info->type = STAT_DIR;
    }
    else if (content->content_type == CONTENT_TYPE_FILE) {
        str_memcpy(info->full_name, content->meta.file_name, 11);
        str_memcpy(info->file_name, content->meta.file_name, 8);
        info->file_name[7] = 0;
        str_memcpy(info->file_extension, content->meta.file_name + 8, 3);
        info->type = STAT_FILE;
    }
    else {
        print_error("Unknown content_type! content_type=%i", content->content_type);
        return 0;
    }

    return 1;
}
