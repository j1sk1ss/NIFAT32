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

static const content_t* _content_table[CONTENT_TABLE_SIZE] = { NULL };

int FAT_initialize() {
    return 0;
}

static cluster_addr_t last_allocated_cluster = SECTOR_OFFSET;
static cluster_addr_t _cluster_allocate() {
    cluster_addr_t cluster = last_allocated_cluster;
    cluster_status_t clusterStatus = FREE_CLUSTER_32;
    while (cluster < _fs_data.total_clusters) {
        clusterStatus = read_fat(cluster, &_fs_data);
        if (is_cluster_free(clusterStatus)) {
            if (set_cluster_end(cluster, _fs_data.fat_type)) {
                last_allocated_cluster = cluster;
                return cluster;
            }
            else {
                print_error("Error occurred with write_fat, aborting operations...");
                return -3;
            }
        }
        else if (clusterStatus < 0) {
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
    else if (cluster_status < 0) {
        print_error("Error occurred with read_fat(), aborting operations...");
        return 0;
    }

    if (set_cluster_free(cluster, &_fs_data)) return 1;
    else {
        print_error("Error occurred with write_fat(), aborting operations...");
        return 0;
    }
}

static int _cluster_readoff(cluster_addr_t cluster, cluster_offset_t offset, unsigned char* buffer, int buff_size) {
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)_fs_data.sectors_per_cluster + _fs_data.first_data_sector;
    return DSK_readoff_sectors(start_sect, offset, buffer, buff_size, _fs_data.sectors_per_cluster);
}

static int _cluster_read(cluster_addr_t cluster, unsigned char* buffer, int buff_size) {
    return _cluster_readoff(cluster, 0, buffer, buff_size);
}

static int _cluster_writeoff(cluster_addr_t cluster, cluster_offset_t offset, const unsigned char* data, int data_size) {
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)_fs_data.sectors_per_cluster + _fs_data.first_data_sector;
    return DSK_writeoff_sectors(start_sect, offset, data, data_size, _fs_data.sectors_per_cluster);
}

static int _cluster_write(cluster_addr_t cluster, const unsigned char* data, int data_size) {
    return _cluster_writeoff(cluster, 0, data, data_size);
}

static int _copy_cluster2cluster(unsigned int source, unsigned int destination) {
    /* TODO */
    return 1;
}

static cluster_addr_t _add_cluster_to_content(ci_t ci) {
    directory_entry_t content_meta = FAT_get_content_from_table(ci)->meta;
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
                return -1;
            }

            return allocated_cluster;
        }
        else {
            print_error("Allocated cluster [addr=%i] is <BAD>!", allocated_cluster);
            return -1;
        }
    }

    return -1;
}

static int _directory_search(
    const char* entry_name, 
    const cluster_addr_t cluster, 
    directory_entry_t* file
) {
    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
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
    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
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
            file_to_add->creation_date 			= _current_date();
            file_to_add->creation_time 			= _current_time();
            file_to_add->creation_time_tenths 	= _current_time();
            file_to_add->last_accessed 			= file_to_add->creation_date;
            file_to_add->last_modification_date = file_to_add->creation_date;
            file_to_add->last_modification_time = file_to_add->creation_time;
            
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
    if (!_name_check((char*)old_meta->file_name)) {
        print_error("Invalid file name!");
        return -1;
    }

    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
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

static int _directory_remove(const cluster_addr_t cluster, const char* file_name) {
    if (!_name_check(file_name)) {
        print_error("Invalid file name!");
        return -1;
    }

    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -2;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return -3;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (!str_strcmp((char*)file_metadata->file_name, file_name)) {
            file_metadata->file_name[0] = ENTRY_FREE;
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
                unsigned int next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) {
                    print_error("End of cluster chain reached. file_t not found. Aborting...");
                    return -5;
                }

                return _directory_remove(next_cluster, file_name);
            }
        }
    }

    free_s(cluster_data);
    return -6;
}

static ci_t _add_content2table(const content_t* content) {
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
    int result = FAT_unload_content_system(_content_table[ci]);
    _content_table[ci] = NULL;
    return result;
}

content_t* FAT_get_content_from_table(ci_t ci) {
    return _content_table[ci];
}

int FAT_close_content(ci_t ci) {
    return _remove_content_from_table(ci);
}

int FAT_read_content2buffer(ci_t ci, unsigned int offset, unsigned char* buffer, unsigned int buff_size) {
    content_t* content = FAT_get_content_from_table(ci);
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

int FAT_write_buffer2content(ci_t ci, unsigned int offset, const unsigned char* data, unsigned int data_size) {
    content_t* content = FAT_get_content_from_table(ci);
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
        if (ca < 0) {
            print_error("_add_cluster_to_content() error. Aborting...");
            return -3;
        }

        _cluster_write(ca, data + (cluster_size * clusters++), data_size);
        data_size -= cluster_size;
    }

    return 1;
}

/* TODO from ci to ca */
int FAT_directory_list(ci_t ci) {
    cluster_addr_t cluster = GET_CLUSTER_FROM_ENTRY(FAT_get_content_from_table(ci)->meta, _fs_data.fat_type);
    content_t* content = FAT_create_content();
    if (!content) return -1;

    content->directory = _create_directory();
    if (!content->directory) return -2;

    content->parent_cluster = 0;
    content->content_type = CONTENT_TYPE_DIRECTORY;

    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        FAT_unload_content_system(content);
        return -3;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        FAT_unload_content_system(content);
        free_s(cluster_data);
        return -4;
    }

    unsigned int iterations = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (file_metadata->file_name[0] == ENTRY_END) break;
        else if (!str_strncmp((char*)file_metadata->file_name, "..", 2) || !str_strncmp((char*)file_metadata->file_name, ".", 1)) {
            iterations++;
            file_metadata++;
        }
        else if (
            (file_metadata->file_name[0] == ENTRY_FREE) || 
            ((file_metadata->attributes & FILE_LONG_NAME) == FILE_LONG_NAME)
        ) {	
            if (iterations < _fs_data.cluster_size / sizeof(directory_entry_t) - 1) {
                iterations++;
                file_metadata++;
            }
            else {
                unsigned int next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) break;
                else if (next_cluster < 0) {
                    print_error("read_fat() encountered an error. Aborting...");
                    FAT_unload_content_system(content);
                    free_s(cluster_data);
                    return -5;
                }
                else {
                    FAT_unload_content_system(content);
                    free_s(cluster_data);
                    return FAT_directory_list(next_cluster);
                }
            }
        }
        else {
            if ((file_metadata->attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {			
                file_t* file = _create_file();
                str_strncpy(file->name, (const char*)file_metadata->file_name, 11);
                /* TODO */
            }
            else if ((file_metadata->attributes & FILE_DIRECTORY) == FILE_DIRECTORY) {
                directory_t* parent_dir = _create_directory();
                str_strncpy(parent_dir->name, (char*)file_metadata->file_name, 11);
                /* TODO */
            }

            iterations++;
            file_metadata++;
        }
    }

    ci_t ci = _add_content2table(content);
    if (ci < 0) {
        print_error("An error occurred in _add_content2table(). Aborting...");
        FAT_unload_content_system(content);
        free(cluster_data);
        return -6;
    }

    return ci;
}

int FAT_content_exists(const char* path) {
    unsigned int start = 0;
    char fileNamePart[256] = { 0 };
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;
    for (int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == '\\' || path[iterator] == '\0') {
            str_memset(fileNamePart, 0, 256);
            str_memcpy(fileNamePart, path + start, iterator - start);

            directory_entry_t file_info;
            if (_directory_search(fileNamePart, active_cluster, &file_info) < 0) return 0;
            start = iterator + 1;

            active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
        }
    }

    return 1;
}

ci_t FAT_open_content(const char* path) {
    content_t* fat_content = FAT_create_content();
    if (!fat_content) return -1;

    char fileNamePart[256] = { 0 };
    unsigned int start = 0;
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;
    
    directory_entry_t content_meta;
    for (unsigned int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == '\\' || path[iterator] == '\0') {
            str_memset(fileNamePart, '\0', 256);
            str_memcpy(fileNamePart, path + start, iterator - start);

            if (_directory_search(fileNamePart, active_cluster, &content_meta) < 0) {
                print_error("_directory_search() not found content!");
                FAT_unload_content_system(fat_content);
                return -2;
            }

            start = iterator + 1;
            active_cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
            if (path[iterator] != '\0') fat_content->parent_cluster = active_cluster;
        }
    }
    
    str_memcpy(&fat_content->meta, &content_meta, sizeof(directory_entry_t));
    if ((content_meta.attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {
        fat_content->file = _create_file();
        if (!fat_content->file) {
            print_error("_create_file() error!");
            FAT_unload_content_system(fat_content);
            return -3;
        }

        int content_size = 0;
        unsigned int* content = NULL;
        fat_content->content_type = CONTENT_TYPE_FILE;        
        fat_content->file->data_head = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);        

        char name[13] = { 0 };
        str_strncpy(fat_content->file->name, (char*)fat_content->meta.file_name, 13);
        /* TODO */
    }
    else {
        fat_content->directory = _create_directory();
        if (!fat_content->directory) {
            FAT_unload_content_system(fat_content);
            return -4;
        }

        fat_content->content_type = CONTENT_TYPE_DIRECTORY;
        str_strncpy(fat_content->directory->name, (char*)content_meta.file_name, 10);
    }

    ci_t ci = _add_content2table(fat_content);
    if (ci < 0) {
        print_error("An error occurred in _add_content2table(). Aborting...");
        FAT_unload_content_system(fat_content);
        return -5;
    }

    return ci;
}

int FAT_change_meta(const char* path, const directory_entry_t* new_meta) {
    unsigned int start = 0;
    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;
    cluster_addr_t prev_active_cluster = 0;

    directory_entry_t file_info;
    for (unsigned int iterator = 0; iterator <= str_strlen(path); iterator++) {
        if (path[iterator] == '\\' || path[iterator] == '\0') {
            prev_active_cluster = active_cluster;

            char part_path[256] = { 0 };
            str_memcpy(part_path, path + start, iterator - start);
            if (_directory_search(part_path, active_cluster, &file_info) < 0) {
                print_error("An error occurred in _directory_search(). Aborting...");
                return -1;
            }

            start = iterator + 1;
            active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
        }
    }

    if (!_directory_edit(prev_active_cluster, &file_info, new_meta)) {
        print_error("_directory_edit() encountered an error. Aborting...");
        return -1;
    }
    
    return 1;
}

int FAT_put_content(const char* path, content_t* content) {
    int parent_ci = FAT_open_content(path);
    if (parent_ci == -1) return -1;

    directory_entry_t file_info = _content_table[parent_ci]->meta;
    unsigned int active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
    _remove_content_from_table(parent_ci);

    char output[13] = { 0 };
    _fatname2name((char*)content->meta.file_name, output);
    int retVal = _directory_search(output, active_cluster, NULL);
    if (retVal < 0 && retVal != -2) {
        print_error("_directory_search() encountered an error. Aborting...");
        return -1;
    }

    if (_directory_add(active_cluster, &content->meta) < 0) {
        print_error("_directory_add() encountered an error. Aborting...");
        return -1;
    }

    return 1;
}

int FAT_delete_content(const char* path) {
    ci_t ci = FAT_open_content(path);
    if (ci < 0) {
        print_error("Content not found!");
        return -1;
    }

    content_t* content = FAT_get_content_from_table(ci);
    if (!content) {
        print_error("Content not found!");
        FAT_close_content(ci);
        return -2;
    }

    cluster_addr_t prev_cluster = 0;
    cluster_addr_t data_cluster = GET_CLUSTER_FROM_ENTRY(content->meta, _fs_data.fat_type);    
    while (!is_cluster_end(data_cluster) && !is_cluster_bad(data_cluster)) {
        prev_cluster = read_fat(data_cluster, &_fs_data);
        if (!_cluster_deallocate(data_cluster)) {
            print_error("_cluster_deallocate() encountered an error. Aborting...");
            FAT_close_content(ci);
            return -3;
        }

        data_cluster = prev_cluster;
    }

    if (!_directory_remove(content->parent_cluster, (char*)content->meta.file_name)) {
        print_error("_directory_remove() encountered an error. Aborting...");
        FAT_close_content(ci);
        return -4;
    }

    FAT_close_content(ci);
    return 1;
}

int FAT_copy_content(char* source, char* destination) {
    ci_t src_ci = FAT_open_content(source);
    if (src_ci < 0) {
        print_error("Content not found!");
        return -1;
    }

    content_t* src_content = FAT_get_content_from_table(src_ci);
    if (!src_content) {
        print_error("Content not found!");
        FAT_close_content(src_ci);
        return -2;
    }

    directory_entry_t src_meta;
    str_memcpy(&src_meta, &src_content->meta, sizeof(directory_entry_t));

    content_t* dst_content = NULL;
    if (src_content->content_type == CONTENT_TYPE_DIRECTORY) dst_content = FAT_create_object(src_content->directory->name, 1, NULL);
    else if (src_content->content_type == CONTENT_TYPE_FILE) dst_content = FAT_create_object(src_content->file->name, 0, src_content->file->extension);

    directory_entry_t dst_meta;		
    str_memcpy(&dst_meta, &dst_content->meta, sizeof(directory_entry_t));

    ci_t dst_ci = FAT_put_content(destination, dst_content);
    if (src_ci < 0) {
        print_error("Content not found!");
        FAT_close_content(src_ci);
        _unload_content_system(dst_content);
        return -3;
    }

    unsigned int src_cluster = GET_CLUSTER_FROM_ENTRY(src_meta, _fs_data.fat_type);
    unsigned int dst_cluster = GET_CLUSTER_FROM_ENTRY(dst_meta, _fs_data.fat_type);
    while (!is_cluster_end(src_cluster) && !is_cluster_bad(src_cluster)) {
        cluster_addr_t dst_ca = _add_cluster_to_content(dst_ci);
        _copy_cluster2cluster(src_cluster, dst_ca);
        src_cluster = read_fat(src_cluster, &_fs_data);
    }

    FAT_close_content(dst_ci);
    FAT_close_content(src_ci);
    return 1;
}

int FAT_stat_content(int ci, cinfo_t* info) {
    content_t* content = FAT_get_content_from_table(ci);
    if (!content) {
        info->type = NOT_PRESENT;
        return -1;
    }

    if (content->content_type == CONTENT_TYPE_DIRECTORY) {
        info->size = 0;
        str_strcpy((char*)info->full_name, (char*)content->directory->name);
        info->type = STAT_DIR;
    }
    else if (content->content_type == CONTENT_TYPE_FILE) {
        str_strcpy((char*)info->full_name, (char*)content->meta.file_name);
        str_strcpy(info->file_name, content->file->name);
        str_strcpy(info->file_extension, content->file->extension);
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

void _fatname2name(char* input, char* output) {
    if (input[0] == '.') {
        if (input[1] == '.') {
            str_strcpy(output, "..");
            return;
        }

        str_strcpy (output, ".");
        return;
    }

    unsigned short counter = 0;
    for (counter = 0; counter < 8; counter++) {
        if (input[counter] == 0x20) {
            output[counter] = '.';
            break;
        }

        output[counter] = input[counter];
    }

    if (counter == 8) output[counter] = '.';
    unsigned short counter2 = 8;
    for (counter2 = 8; counter2 < 11; counter2++) {
        ++counter;
        if (input[counter2] == 0x20 || input[counter2] == 0x20) {
            if (counter2 == 8)
                counter -= 2;

            break;
        }
        
        output[counter] = input[counter2];		
    }

    ++counter;
    while (counter < 12) {
        output[counter] = ' ';
        ++counter;
    }

    output[12] = '\0';
    return;
}

char* _name2fatname(char* input) {
    str2uppercase(input);

    int haveExt = 0;
    char searchName[13] = { '\0' };
    unsigned short dotPos = 0;
    unsigned int counter = 0;

    while (counter <= 8) {
        if (input[counter] == '.' || input[counter] == '\0') {
            if (input[counter] == '.') haveExt = 1;
            dotPos = counter;
            counter++;
            break;
        }
        else {
            searchName[counter] = input[counter];
            counter++;
        }
    }

    if (counter > 9) {
        counter = 8;
        dotPos = 8;
    }
    
    unsigned short extCount = 8;
    while (extCount < 11) {
        if (input[counter] != '\0' && haveExt == 1) searchName[extCount] = input[counter];
        else searchName[extCount] = ' ';

        counter++;
        extCount++;
    }

    counter = dotPos;
    while (counter < 8) {
        searchName[counter] = ' ';
        counter++;
    }

    str_strcpy(input, searchName);
    return input;
}

unsigned short _current_time() {
    /* TODO */
    return 0;
}

unsigned short _current_date() {
    /* TODO */
    return 0;
}

int _name_check(const char* input) {
    short retVal = 0;
    unsigned short iterator = 0;
    for (iterator = 0; iterator < 11; iterator++) {
        if (input[iterator] < 0x20 && input[iterator] != 0x05) {
            retVal = retVal | BAD_CHARACTER;
        }
        
        switch (input[iterator]) {
            case 0x2E: {
                if ((retVal & NOT_CONVERTED_YET) == NOT_CONVERTED_YET) //a previous dot has already triggered this case
                    retVal |= TOO_MANY_DOTS;

                retVal ^= NOT_CONVERTED_YET; //remove NOT_CONVERTED_YET flag if already set
                break;
            }

            case 0x22:
            case 0x2A:
            case 0x2B:
            case 0x2C:
            case 0x2F:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x7C:
                retVal = retVal | BAD_CHARACTER;
        }

        if (input[iterator] >= 'a' && input[iterator] <= 'z') 
            retVal = retVal | LOWERCASE_ISSUE;
    }

    return retVal;
}

static directory_entry_t* _create_entry(
    const char* name, 
    const char* ext, 
    int is_dir, 
    unsigned int first_cluster, 
    unsigned int file_size
) {
    directory_entry_t* data = (directory_entry_t*)malloc_s(sizeof(directory_entry_t));
    if (!data) return NULL;
    
    data->reserved0 			 = 0; 
    data->creation_time_tenths 	 = 0;
    data->creation_time 		 = 0;
    data->creation_date 		 = 0;
    data->last_modification_date = 0;

    char* file_name = (char*)malloc_s(25);
    if (!file_name) {
        free_s(data);
        return NULL;
    }
    
    str_strcpy(file_name, name);
    if (ext) {
        strcat(file_name, ".");
        strcat(file_name, ext);
    }
    
    data->low_bits 	= first_cluster;
    data->high_bits = first_cluster >> 16;  

    if (is_dir) {
        data->file_size  = 0;
        data->attributes = FILE_DIRECTORY;
    } 
    else {
        data->file_size  = file_size;
        data->attributes = FILE_ARCHIVE;
    }

    data->creation_date = _current_date();
    data->creation_time = _current_time();
    data->creation_time_tenths = _current_time();

    if (_name_check(file_name) != 0) _name2fatname(file_name);
    str_strncpy((char*)data->file_name, file_name, min(11, str_strlen(file_name)));
    free_s(file_name);

    return data; 
}

content_t* FAT_create_object(char* name, int is_directory, char* extension) {
    if (str_strlen(name) > 11 || str_strlen(extension) > 4) return NULL;
    
    content_t* content = FAT_create_content();
    if (is_directory) {
        content->content_type = CONTENT_TYPE_DIRECTORY;
        content->directory = _create_directory();
        str_strncpy(content->directory->name, name, 12);

        directory_entry_t* meta = _create_entry(name, NULL, 1, _cluster_allocate(), 0);
        if (meta) {
            str_memcpy(&content->meta, meta, sizeof(directory_entry_t));
            free_s(meta);
        }
    }
    else {
        content->content_type = CONTENT_TYPE_FILE;
        content->file = _create_file();
        str_strncpy(content->file->name, name, 8);
        str_strncpy(content->file->extension, extension, 4);
        
        directory_entry_t* meta = _create_entry(name, extension, 0, _cluster_allocate(), 1);
        if (meta) {
            str_memcpy(&content->meta, meta, sizeof(directory_entry_t));
            free_s(meta);
        }
    }

    return content;
}

content_t* FAT_create_content() {
    content_t* content = (content_t*)malloc_s(sizeof(content_t));
    if (!content) return NULL;
    content->directory = NULL;
    content->file = NULL;
    content->parent_cluster = -1;
    return content;
}

directory_t* _create_directory() {
    directory_t* directory = (directory_t*)malloc_s(sizeof(directory_t));
    if (!directory) return NULL;
    return directory;
}

file_t* _create_file() {
    file_t* file = (file_t*)malloc_s(sizeof(file_t));
    if (!file) return NULL;
    return file;
}

static int _unload_file_system(file_t* file) {
    if (!file) return -1;
    free(file);
    return 1;
}

static int _unload_directory_system(directory_t* directory) {
    if (!directory) return -1;
    free(directory);
    return 1;
}

static int _unload_content_system(content_t* content) {
    if (!content) return -1;
    if (content->content_type == CONTENT_TYPE_DIRECTORY) _unload_directory_system(content->directory);
    else if (content->content_type == CONTENT_TYPE_DIRECTORY) _unload_file_system(content->file);
    free(content);
    return 1;
}