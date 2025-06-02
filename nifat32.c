#include "nifat32.h"

static fat_data_t _fs_data = {
    .fat_size            = 0,
    .fat_type            = 0,
    .first_fat_sector    = 0,
    .first_data_sector   = 0,
    .total_sectors       = 0,
    .total_clusters      = 0,
    .bytes_per_sector    = 0,
    .sectors_per_cluster = 0,
    .ext_root_cluster    = 0,
    .cluster_size        = 0
};

static content_t* _content_table[CONTENT_TABLE_SIZE] = { NULL };

int NIFAT32_init() {
    mm_init();
    buffer_t sector_data = (buffer_t)malloc_s(DSK_get_sector_size());
    if (!sector_data) {
        print_error("malloc_s() error!");
        return 0;
    }

    if (!DSK_read_sector(0, sector_data, DSK_get_sector_size())) {
        print_error("DSK_read_sector() error!");
        free_s(sector_data);
        return 0;
    }

    fat_BS_t* bootstruct = (fat_BS_t*)sector_data;
    fat_extBS_32_t* ext_bootstruct = (fat_extBS_32_t*)bootstruct->extended_section;

    checksum_t bcheck = bootstruct->checksum;
    bootstruct->checksum = 0;
    checksum_t exbcheck = ext_bootstruct->checksum;
    ext_bootstruct->checksum = 0;
    
    ext_bootstruct->checksum = crc32(0, (buffer_t)ext_bootstruct, sizeof(fat_extBS_32_t));
    bootstruct->checksum     = crc32(0, (buffer_t)bootstruct, sizeof(fat_BS_t));
    if (bootstruct->checksum != bcheck || ext_bootstruct->checksum != exbcheck) {
        print_error(
            "Checksum check error! %u != %u or %u != %u", 
            bootstruct->checksum, bcheck, ext_bootstruct->checksum, exbcheck
        );
        return 0;
    }
    else {
        bootstruct->checksum     = bcheck;
        ext_bootstruct->checksum = exbcheck;
    }

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

    free_s(sector_data);
    return 1;
}

static cluster_addr_t last_allocated_cluster = SECTOR_OFFSET;
static cluster_addr_t _cluster_allocate() {
    cluster_addr_t cluster = last_allocated_cluster;
    cluster_status_t cluster_status = FREE_CLUSTER_32;
    while (cluster < _fs_data.total_clusters) {
        cluster_status = read_fat(cluster, &_fs_data);
        if (is_cluster_free(cluster_status)) {
            if (set_cluster_end(cluster, &_fs_data)) {
                last_allocated_cluster = cluster;
                return cluster;
            }
            else {
                print_error("Error occurred with write_fat, aborting operations...");
                return -3;
            }
        }
        else if (is_cluster_bad(cluster_status)) {
            print_error("Error occurred with __read_fat, aborting operations...");
            return -2;
        }

        cluster++;
    }

    last_allocated_cluster = 2;
    return -1;
}

static int _cluster_deallocate(const cluster_addr_t cluster) {
    cluster_status_t cluster_status = read_fat(cluster, &_fs_data);
    if (is_cluster_free(cluster_status)) return 1;
    else if (is_cluster_bad(cluster_status)) {
        print_error("Error occurred with read_fat(), aborting operations...");
        return 0;
    }

    if (set_cluster_free(cluster, &_fs_data)) return 1;
    else {
        print_error("Error occurred with write_fat(), aborting operations...");
        return 0;
    }
}

static int _cluster_readoff(cluster_addr_t cluster, cluster_offset_t offset, buffer_t buffer, int buff_size) {
    print_debug("_cluster_readoff(cluster=%u, offset=%u)", cluster, offset);
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)_fs_data.sectors_per_cluster + _fs_data.first_data_sector;
    return DSK_readoff_sectors(start_sect, offset, buffer, buff_size, _fs_data.sectors_per_cluster);
}

static int _cluster_read(cluster_addr_t cluster, buffer_t buffer, int buff_size) {
    return _cluster_readoff(cluster, 0, buffer, buff_size);
}

static int _cluster_writeoff(cluster_addr_t cluster, cluster_offset_t offset, const buffer_t data, int data_size) {
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)_fs_data.sectors_per_cluster + _fs_data.first_data_sector;
    return DSK_writeoff_sectors(start_sect, offset, data, data_size, _fs_data.sectors_per_cluster);
}

static int _cluster_write(cluster_addr_t cluster, const buffer_t data, int data_size) {
    return _cluster_writeoff(cluster, 0, data, data_size);
}

static int _directory_search(
    const char* entry_name, 
    const cluster_addr_t cluster, 
    directory_entry_t* file
) {
    print_debug("_directory_search(entry_name=%s, cluster=%u)", entry_name, cluster);
    buffer_t cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return -2;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        print_spec("file_metadata->file_name=[%s] and [%s]", file_metadata->file_name, entry_name);
        if (file_metadata->file_name[0] == ENTRY_END) break;
        else if (str_strncmp((char*)file_metadata->file_name, entry_name, 11)) {
            if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1) {
                iterator++;
                file_metadata++;
            }
            else {
                int next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) break;
                else if (next_cluster < 0) {
                    print_error("read_fat() encountered an error. Aborting...");
                    free_s(cluster_data);
                    return -3;
                } 
                else {
                    free_s(cluster_data);
                    return _directory_search(entry_name, next_cluster, file);
                }
            }
        }
        else {
            if (file) {
                str_memcpy(file, file_metadata, sizeof(directory_entry_t));
            }

            free_s(cluster_data);
            return 1;
        }
    }

    free_s(cluster_data);
    return -4;
}

static int _directory_add(const cluster_addr_t cluster, directory_entry_t* file_to_add) {
    print_debug("_directory_add(cluster=%u, file=%s)", cluster, file_to_add->file_name);
    buffer_t cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return -1;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (file_metadata->file_name[0] != ENTRY_FREE && file_metadata->file_name[0] != ENTRY_END) {
            if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1) {
                file_metadata++;
                iterator++;
            }
            else {
                cluster_addr_t next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) {
                    next_cluster = _cluster_allocate();
                    if (is_cluster_bad(next_cluster)) {
                        print_error("Allocation of new cluster failed. Aborting...");
                        free_s(cluster_data);
                        return -2;
                    }

                    if (!write_fat(cluster, next_cluster, &_fs_data)) {
                        print_error("Extension of the cluster chain with new cluster failed. Aborting...");
                        free_s(cluster_data);
                        return -3;
                    }
                }

                free_s(cluster_data);
                return _directory_add(next_cluster, file_to_add);
            }
        }
        else {
            file_to_add->low_bits  = GET_ENTRY_LOW_BITS(cluster, _fs_data.fat_type);
            file_to_add->high_bits = GET_ENTRY_HIGH_BITS(cluster, _fs_data.fat_type);
            str_memcpy(file_metadata, file_to_add, sizeof(directory_entry_t));
            if (!_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
                print_error("Writing new directory entry failed. Aborting...");
                free_s(cluster_data);
                return -5;
            }

            free_s(cluster_data);
            return 1;
        }
    }

    free_s(cluster_data);
    return -6;
}

static int _directory_edit(const cluster_addr_t cluster, directory_entry_t* old_meta, const directory_entry_t* new_meta) {
    if (!is_fatname((char*)old_meta->file_name)) {
        print_error("Invalid file name!");
        return -1;
    }

    buffer_t cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -2;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        free_s(cluster_data);
        return -3;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (!str_strcmp((char*)file_metadata->file_name, (char*)old_meta->file_name)) {
            str_memcpy(file_metadata, new_meta, sizeof(directory_entry_t));
            if (!_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
                print_error("Writing updated directory entry failed. Aborting...");
                free_s(cluster_data);
                return -4;
            }

            return 1;
        } 
        else {
            if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1)  {
                iterator++;
                file_metadata++;
            } 
            else {
                free_s(cluster_data);
                cluster_addr_t next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) {
                    print_error("End of cluster chain reached. file_t not found. Aborting...");
                    return -5;
                }

                return _directory_edit(next_cluster, old_meta, new_meta);
            }
        }
    }

    free_s(cluster_data);
    return -6;
}

static int _directory_remove(const cluster_addr_t cluster, const directory_entry_t* file_to_add) {
    buffer_t cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return -2;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (!str_strcmp((char*)file_metadata->file_name, (char*)file_to_add->file_name)) {
            file_metadata->file_name[0] = ENTRY_FREE;
            if (!_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
                print_error("Writing updated directory entry failed. Aborting...");
                free_s(cluster_data);
                return -3;
            }

            return 1;
        } 
        else {
            if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1)  {
                iterator++;
                file_metadata++;
            } 
            else {
                free_s(cluster_data);
                unsigned int next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) {
                    print_error("End of cluster chain reached. file_t not found. Aborting...");
                    return -4;
                }

                return _directory_remove(next_cluster, file_to_add);
            }
        }
    }

    free_s(cluster_data);
    return -5;
}


static unsigned short _current_time() {
    /* TODO */
    return 0;
}

static unsigned short _current_date() {
    /* TODO */
    return 0;
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
    
    entry->low_bits  = first_cluster;
    entry->high_bits = first_cluster >> 16;  

    if (is_dir) entry->attributes = FILE_DIRECTORY;
    else {
        entry->file_size  = file_size;
        entry->attributes = FILE_ARCHIVE;
    }

    entry->creation_date = _current_date();
    entry->creation_time = _current_time();
    entry->creation_time_tenths = _current_time();

    entry->checksum = 0;
    entry->checksum = crc32(0, (buffer_t)entry, sizeof(directory_entry_t));

    name_to_fatname(tmp_filename, (char*)entry->file_name);
    print_debug("_create_entry=%s/%u", entry->file_name, entry->attributes);
    return 1; 
}

static content_t* _create_content() {
    content_t* content = (content_t*)malloc_s(sizeof(content_t));
    if (!content) return NULL;
    content->directory = NULL;
    content->file = NULL;
    content->parent_cluster = -1;
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
    else if (content->content_type == CONTENT_TYPE_DIRECTORY) _unload_file_system(content->file);
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

static cluster_addr_t _get_cluster_by_path(const char* path, directory_entry_t* entry) {
    print_debug("_get_cluster_by_path(path=%s)", path);
    unsigned int start = 0;
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;

    directory_entry_t file_info;
    for (unsigned int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == '/' || path[iterator] == '\0') {
            char name_buffer[256] = { 0 };
            str_memcpy(name_buffer, path + start, iterator - start);

            char fatname_buffer[128] = { 0 };
            name_to_fatname(name_buffer, fatname_buffer);
            if (_directory_search(fatname_buffer, active_cluster, &file_info) < 0) return BAD_CLUSTER_32;
            start = iterator + 1;

            active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
        }
    }

    if (entry) {
        str_memcpy(entry, &file_info, sizeof(directory_entry_t));
    }

    return active_cluster;    
}

int NIFAT32_content_exists(const char* path) {
    return _get_cluster_by_path(path, NULL) != BAD_CLUSTER_32;
}

ci_t NIFAT32_open_content(const char* path) {
    print_debug("NIFAT32_open_content(path=%s)", path);
    content_t* fat_content = _create_content();
    if (!fat_content) {
        print_error("_create_content() error!");
        return -1;
    }
    
    cluster_addr_t cluster = _get_cluster_by_path(path, &fat_content->meta);
    if (is_cluster_bad(cluster)) {
        print_error("Entry not found!");
        _unload_content_system(fat_content);
        return -2;
    }
    else {
        print_debug("NIFAT32_open_content: Content cluster is: %u", cluster);
    }
    
    if ((fat_content->meta.attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {
        fat_content->file = _create_file();
        if (!fat_content->file) {
            print_error("_create_file() error!");
            _unload_content_system(fat_content);
            return -3;
        }

        fat_content->content_type = CONTENT_TYPE_FILE;        
        fat_content->file->data_head = cluster;        
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

int NIFAT32_read_content2buffer(const ci_t ci, unsigned int offset, buffer_t buffer, int buff_size) {
    print_debug("NIFAT32_read_content2buffer(ci=%i, offset=%u)", ci, offset);
    content_t* content = _get_content_from_table(ci);
    if (!content || content->content_type != CONTENT_TYPE_FILE) {
        print_warn("Content or file not found!");
        return -1;
    }

    unsigned int clusters = 0;
    cluster_addr_t ca = content->file->data_head;
    unsigned int cluster_size = _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster;
    while (!is_cluster_end(ca) && !is_cluster_bad(ca) && buff_size > 0) {
        if (offset > cluster_size) offset -= cluster_size;
        else {
            if (!_cluster_readoff(ca, offset, buffer + (cluster_size * clusters++), buff_size)) {
                print_error("_cluster_readoff() error. Aborting...");
                return -2;
            }

            buff_size -= cluster_size - offset;
            offset = 0;
        }

        ca = read_fat(ca, &_fs_data);
    }

    return 1;
}

static cluster_addr_t _add_cluster_to_content(const ci_t ci) {
    directory_entry_t content_meta = _get_content_from_table(ci)->meta;
    cluster_addr_t cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
    while (!is_cluster_end(cluster) && !is_cluster_bad(cluster)) {
        cluster = read_fat(cluster, &_fs_data);
    }

    if (!is_cluster_end(cluster)) print_error("End of cluster chain not found!");
    else {
        cluster_addr_t allocated_cluster = _cluster_allocate();
        if (!is_cluster_bad(allocated_cluster)) {
            if (!write_fat(cluster, allocated_cluster, &_fs_data)) {
                print_error("Allocated cluster can't be added to content!");
                return BAD_CLUSTER_32;
            }

            return allocated_cluster;
        }
        else {
            print_error("Allocated cluster [addr=%i] is <BAD>!", allocated_cluster);
            return BAD_CLUSTER_32;
        }
    }

    return BAD_CLUSTER_32;
}

int NIFAT32_write_buffer2content(const ci_t ci, unsigned int offset, const buffer_t data, int data_size) {
    content_t* content = _get_content_from_table(ci);
    if (!content || content->content_type != CONTENT_TYPE_FILE) {
        print_warn("Content or file not found!");
        return -1;
    }

    unsigned int clusters = 0;
    cluster_addr_t ca = content->file->data_head;
    unsigned int cluster_size = _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster;
    while (!is_cluster_end(ca) && !is_cluster_bad(ca) && data_size > 0) {
        if (offset > cluster_size) offset -= cluster_size;
        else {
            if (!_cluster_writeoff(ca, offset, data + (cluster_size * clusters++), data_size)) {
                print_error("_cluster_readoff() error. Aborting...");
                return -2;
            }

            data_size -= cluster_size - offset;
            offset = 0;
        }

        ca = read_fat(ca, &_fs_data);
    }

    while (data_size > 0) {
        cluster_addr_t ca = _add_cluster_to_content(ci);
        if (is_cluster_bad(ca)) {
            print_error("_add_cluster_to_content() error. Aborting...");
            return -3;
        }

        _cluster_write(ca, data + (cluster_size * clusters++), data_size);
        data_size -= cluster_size;
    }

    return 1;
}

int NIFAT32_change_meta(const char* path, const directory_entry_t* new_meta) {
    directory_entry_t file_info;
    cluster_addr_t cluster = _get_cluster_by_path(path, &file_info);
    if (is_cluster_bad(cluster)) {
        print_error("Entry not found!");
        return -1;
    }

    if (!_directory_edit(cluster, &file_info, new_meta)) {
        print_error("_directory_edit() encountered an error. Aborting...");
        return -2;
    }
    
    return 1;
}

int NIFAT32_put_content(const ci_t ci, cinfo_t* info) {
    print_debug("NIFAT32_put_content(ci=%i, info=%s/%s/%s)", ci, info->full_name, info->file_name, info->file_extension);
    directory_entry_t file_info = _get_content_from_table(ci)->meta;
    unsigned int active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);

    int is_found = _directory_search((char*)info->full_name, active_cluster, NULL);
    if (is_found < 0 && is_found != -4) {
        print_error("_directory_search() encountered an error. Aborting...");
        return -1;
    }

    directory_entry_t new_meta = { 0 };
    _create_entry(info->file_name, info->file_extension, info->type == STAT_DIR, _cluster_allocate(), 1, &new_meta);
    if (_directory_add(active_cluster, &new_meta) < 0) {
        print_error("_directory_add() encountered an error. Aborting...");
        return -1;
    }

    return 1;
}

int NIFAT32_delete_content(ci_t ci) {
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        print_error("Content not found!");
        NIFAT32_close_content(ci);
        return -2;
    }

    cluster_addr_t prev_cluster = 0;
    cluster_addr_t data_cluster = GET_CLUSTER_FROM_ENTRY(content->meta, _fs_data.fat_type);    
    while (!is_cluster_end(data_cluster) && !is_cluster_bad(data_cluster)) {
        prev_cluster = read_fat(data_cluster, &_fs_data);
        if (!_cluster_deallocate(data_cluster)) {
            print_error("_cluster_deallocate() encountered an error. Aborting...");
            NIFAT32_close_content(ci);
            return -3;
        }

        data_cluster = prev_cluster;
    }

    if (!_directory_remove(content->parent_cluster, &content->meta)) {
        print_error("_directory_remove() encountered an error. Aborting...");
        NIFAT32_close_content(ci);
        return -4;
    }

    NIFAT32_close_content(ci);
    return 1;
}

int NIFAT32_stat_content(int ci, cinfo_t* info) {
    content_t* content = _get_content_from_table(ci);
    if (!content) {
        info->type = NOT_PRESENT;
        return -1;
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
        return -2;
    }

    info->creation_date = content->meta.creation_date;
    info->creation_time = content->meta.creation_time;
    info->last_accessed = content->meta.last_accessed;
    info->last_modification_date = content->meta.last_modification_date;
    info->last_modification_time = content->meta.last_modification_time;

    return 1;
}
