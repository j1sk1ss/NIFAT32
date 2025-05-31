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

static int _cluster_deallocate(const unsigned int cluster) {
    unsigned int cluster_status = read_fat(cluster, &_fs_data);
    if (is_cluster_free(cluster_status)) return 0;
    else if (cluster_status < 0) {
        print_error("Error occurred with read_fat(), aborting operations...");
        return -1;
    }

    if (set_cluster_free(cluster, &_fs_data) == 0) return 0;
    else {
        print_error("Error occurred with write_fat(), aborting operations...");
        return -1;
    }
}

static int _cluster_readoff(cluster_addr_t cluster, unsigned int offset, unsigned char* buffer, int buff_size) {
    sector_addr_t start_sect = (cluster - 2) * (unsigned short)_fs_data.sectors_per_cluster + _fs_data.first_data_sector;
    return DSK_readoff_sectors(start_sect, offset, buffer, buff_size, _fs_data.sectors_per_cluster);
}

static int _cluster_read(cluster_addr_t cluster, unsigned char* buffer, int buff_size) {
    return _cluster_readoff(cluster, 0, buffer, buff_size);
}

static int _cluster_writeoff(cluster_addr_t cluster, unsigned int offset, const unsigned char* data, int data_size) {
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

static int _add_cluster_to_content(int ci) {
    directory_entry_t content_meta = FAT_get_content_from_table(ci)->meta;
    unsigned int cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
    while (!is_cluster_end(cluster) && !is_cluster_bad(cluster)) {
        cluster = read_fat(cluster, &_fs_data);
    }

    if (!is_cluster_end(cluster)) print_error("End of cluster chain not found!");
    else {
        cluster_addr_t allocated_cluster = _cluster_allocate();
        if (!is_cluster_bad(allocated_cluster)) {
            return write_fat(cluster, allocated_cluster, &_fs_data);
        }
        else {
            print_error("Allocated cluster is <Bad>!");
            return -1;
        }
    }

    return -1;
}

int FAT_directory_list(int ci, unsigned char attrs, int exclusive) {
    cluster_addr_t cluster = GET_CLUSTER_FROM_ENTRY(FAT_get_content_from_table(ci)->meta, _fs_data.fat_type);
    content_t* content = FAT_create_content();
    if (!content) return -1;

    content->directory = _create_directory();
    if (!content->directory) return -2;

    content->parent_cluster = 0;
    content->content_type = CONTENT_TYPE_DIRECTORY;

    const unsigned char default_hidden_attributes = (FILE_HIDDEN | FILE_SYSTEM);
    unsigned char attributes_to_hide = default_hidden_attributes;
    if (exclusive == 0) attributes_to_hide &= (~attrs);
    else if (exclusive == 1) attributes_to_hide = (~attrs);

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
        else if (!strncmp((char*)file_metadata->file_name, "..", 2) || !strncmp((char*)file_metadata->file_name, ".", 1)) {
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
                    return FAT_directory_list(next_cluster, attrs, exclusive);
                }
            }
        }
        else {
            if ((file_metadata->attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {			
                file_t* file = _create_file();
                strncpy(file->name, (const char*)file_metadata->file_name, 11);
                /* TODO */
            }
            else if ((file_metadata->attributes & FILE_DIRECTORY) == FILE_DIRECTORY) {
                directory_t* parent_dir = _create_directory();
                strncpy(parent_dir->name, (char*)file_metadata->file_name, 11);
                /* TODO */
            }

            iterations++;
            file_metadata++;
        }
    }

    int root_ci = _add_content2table(content);
    if (root_ci < 0) {
        print_error("An error occurred in _add_content2table(). Aborting...");
        FAT_unload_content_system(content);
        free(cluster_data);
        return -6;
    }

    return root_ci;
}

static int _directory_search(
    const char* filepart, 
    const cluster_addr_t cluster, 
    directory_entry_t* file
) {
    char searchName[13] = { 0 };
    strcpy(searchName, filepart);
    if (_name_check(searchName)) {
        _name2fatname(searchName);
    }

    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!cluster_data) {
        print_error("malloc_s() error!");
        return -1;
    }

    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return -1;
    }

    unsigned int iterator = 0;
    directory_entry_t* file_metadata = (directory_entry_t*)cluster_data;
    while (1) {
        if (file_metadata->file_name[0] == ENTRY_END) break;
        else if (strncmp((char*)file_metadata->file_name, searchName, 11) != 0) {
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
                    return -1;
                } 
                else {
                    free_s(cluster_data);
                    return _directory_search(filepart, next_cluster, file);
                }
            }
        }
        else {
            if (file) memcpy(file, file_metadata, sizeof(directory_entry_t));
            free_s(cluster_data);
            return 0;
        }
    }

    free_s(cluster_data);
    return -2;
}

static int _directory_add(const cluster_addr_t cluster, directory_entry_t* file_to_add) {
    unsigned char* cluster_data = malloc_s(_fs_data.bytes_per_sector * _fs_data.sectors_per_cluster);
    if (!_cluster_read(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
        print_error("_cluster_read() encountered an error. Aborting...");
        return 0;
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
                unsigned int next_cluster = read_fat(cluster, &_fs_data);
                if (is_cluster_end(next_cluster)) {
                    next_cluster = _cluster_allocate();
                    if (is_cluster_bad(next_cluster)) {
                        print_error("Allocation of new cluster failed. Aborting...");
                        free_s(cluster_data);
                        return -1;
                    }

                    if (!write_fat(cluster, next_cluster, &_fs_data)) {
                        print_error("Extension of the cluster chain with new cluster failed. Aborting...");
                        free_s(cluster_data);
                        return -1;
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

            cluster_addr_t new_cluster = _cluster_allocate();
            if (is_cluster_bad(new_cluster)) {
                print_error("Allocation of new cluster failed. Aborting...\n");
                free_s(cluster_data);
                return -1;
            }
            
            file_to_add->low_bits  = GET_ENTRY_LOW_BITS(new_cluster, _fs_data.fat_type);
            file_to_add->high_bits = GET_ENTRY_HIGH_BITS(new_cluster, _fs_data.fat_type);

            memcpy(file_metadata, file_to_add, sizeof(directory_entry_t));
            if (_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster) != 0) {
                print_error("Writing new directory entry failed. Aborting...");
                free_s(cluster_data);
                return -1;
            }

            free_s(cluster_data);
            return 0;
        }
    }

    free_s(cluster_data);
    return -1;
}

static int _directory_edit(const cluster_addr_t cluster, directory_entry_t* old_meta, directory_entry_t* new_meta) {
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
        if (!strcmp((char*)file_metadata->file_name, (char*)old_meta->file_name)) {
            memcpy(file_metadata, new_meta, sizeof(directory_entry_t));
            if (!_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
                print_error("Writing updated directory entry failed. Aborting...");
                free_s(cluster_data);
                return -4;
            }

            return 1;
        } 
        else if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1)  {
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
        if (!strcmp((char*)file_metadata->file_name, file_name)) {
            file_metadata->file_name[0] = ENTRY_FREE;
            if (!_cluster_write(cluster, cluster_data, _fs_data.bytes_per_sector * _fs_data.sectors_per_cluster)) {
                print_error("Writing updated directory entry failed. Aborting...");
                free_s(cluster_data);
                return -4;
            }

            return 1;
        } 
        else if (iterator < _fs_data.cluster_size / sizeof(directory_entry_t) - 1)  {
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

    free_s(cluster_data);
    return -6;
}

int FAT_content_exists(const char* path) {
    char fileNamePart[256] = { 0 };
    unsigned short start = 0;

    cluster_addr_t active_cluster = _fs_data.ext_root_cluster;
    for (int iterator = 0; iterator <= strlen(path); iterator++) {
        if (path[iterator] == '\\' || path[iterator] == '\0') {
            memset(fileNamePart, '\0', 256);
            memcpy(fileNamePart, path + start, iterator - start);

            directory_entry_t file_info;
            if (_directory_search(fileNamePart, active_cluster, &file_info) < 0) return 0;
            start = iterator + 1;

            active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
        }
    }

    return 1;
}

int FAT_open_content(const char* path) {
    content_t* fat_content = FAT_create_content();
    if (!fat_content) return -1;

    char fileNamePart[256] = { 0 };
    unsigned short start = 0;
    unsigned int active_cluster = _fs_data.ext_root_cluster;
    
    directory_entry_t content_meta;
    for (unsigned int iterator = 0; iterator <= strlen(path); iterator++) {
        if (path[iterator] == '\\' || path[iterator] == '\0') {
            memset(fileNamePart, '\0', 256);
            memcpy(fileNamePart, path + start, iterator - start);

            if (_directory_search(fileNamePart, active_cluster, &content_meta) < 0) {
                FAT_unload_content_system(fat_content);
                return -3;
            }

            start = iterator + 1;
            active_cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
            if (path[iterator] != '\0') fat_content->parent_cluster = active_cluster;
        }
    }
    
    memcpy(&fat_content->meta, &content_meta, sizeof(directory_entry_t));
    if ((content_meta.attributes & FILE_DIRECTORY) != FILE_DIRECTORY) {
        fat_content->file = _create_file();
        if (!fat_content->file) {
            FAT_unload_content_system(fat_content);
            return -5;
        }

        fat_content->content_type = CONTENT_TYPE_FILE;
        unsigned int* content = NULL;
        int content_size = 0;
        
        int cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
        while (cluster < END_CLUSTER_32) {
            unsigned int* new_content = (unsigned int*)ALC_realloc(content, (content_size + 1) * sizeof(unsigned int), KERNEL);
            if (new_content == NULL) {
                ALC_free(content, KERNEL);
                FAT_unload_content_system(fat_content);
                return -6;
            }

            new_content[content_size] = cluster;
            content = new_content;
            content_size++;

            cluster = __read_fat(cluster);
            if (cluster == BAD_CLUSTER_32) {
                print_error("The cluster chain is corrupted with a bad cluster. Aborting...");
                ALC_free(content, KERNEL);
                FAT_unload_content_system(fat_content);
                return -7;
            }
            else if (cluster == -1) {
                print_error("An error occurred in __read_fat. Aborting...");
                ALC_free(content, KERNEL);
                FAT_unload_content_system(fat_content);
                return -8;
            }
        }
        
        fat_content->file->data = (unsigned int*)ALC_malloc(content_size * sizeof(unsigned int), KERNEL);
        if (!fat_content->file->data) {
            ALC_free(content, KERNEL);
            ALC_free(fat_content->file, KERNEL);
            FAT_unload_content_system(fat_content);
            return -9;
        }

        memcpy(fat_content->file->data, content, content_size * sizeof(unsigned int));
        fat_content->file->data_size = content_size;
        ALC_free(content, KERNEL);

        char name[13] = { 0 };
        strcpy(name, (char*)fat_content->meta.file_name);
        strncpy(fat_content->file->name, strtok(name, " "), 8);
        strncpy(fat_content->file->extension, strtok(NULL, " "), 4);
    }
    else {
        fat_content->directory = _create_directory();
        if (!fat_content->directory) {
            FAT_unload_content_system(fat_content);
            return -10;
        }

        fat_content->content_type = CONTENT_TYPE_DIRECTORY;
        strncpy(fat_content->directory->name, (char*)content_meta.file_name, 10);
    }

    int ci = _add_content2table(fat_content);
    if (ci < 0) {
        LOG("Function FAT_open_content: an error occurred in _add_content2table. Aborting...\n");
        if (fat_content->file) ALC_free(fat_content->file, KERNEL);
        else if (fat_content->directory) ALC_free(fat_content->directory, KERNEL);
        FAT_unload_content_system(fat_content);
        return -11;
    }

    return ci;
}

content_t* FAT_get_content_from_table(int ci) {
    return _content_table[ci];
}

int FAT_close_content(int ci) {
    return _remove_content_from_table(ci);
}

// Function for reading part of file
// data - content for reading
// buffer - buffer data storage
// offset - file seek
// size - size of read data
int FAT_read_content2buffer(int ci, unsigned char* buffer, unsigned int offset, unsigned int size) {
    unsigned int data_seek     = offset % (_fs_data.sectors_per_cluster * SECTOR_SIZE);
    unsigned int cluster_seek  = offset / (_fs_data.sectors_per_cluster * SECTOR_SIZE);
    unsigned int data_position = 0;

    content_t* data = FAT_get_content_from_table(ci);
    if (!data) return -1;

    for (int i = cluster_seek; i < data->file->data_size && data_position < size; i++) {
        unsigned int copy_size = min(SECTOR_SIZE * _fs_data.sectors_per_cluster - data_seek, size - data_position);
        unsigned char* content_part = _cluster_readoff(data->file->data[i], data_seek);
        memcpy(buffer + data_position, content_part, copy_size);
        ALC_free(content_part, KERNEL);
        data_position += copy_size;
        data_seek = 0;
    }

    return data_position;
}

// Function for reading part of file
// data - content for reading
// buffer - buffer data storage
// offset - file seek
// size - size of read data
// stop - value that will stop reading
int FAT_read_content2buffer_stop(int ci, unsigned char* buffer, unsigned int offset, unsigned int size, unsigned char* stop) {
    unsigned int data_seek     = offset % (_fs_data.sectors_per_cluster * SECTOR_SIZE);
    unsigned int cluster_seek  = offset / (_fs_data.sectors_per_cluster * SECTOR_SIZE);
    unsigned int data_position = 0;

    content_t* data = FAT_get_content_from_table(ci);
    if (data == NULL) return -1;
    
    for (int i = cluster_seek; i < data->file->data_size && data_position < size; i++) {
        unsigned int copy_size = min(SECTOR_SIZE * _fs_data.sectors_per_cluster - data_seek, size - data_position);
        unsigned char* content_part = _cluster_readoff_stop(data->file->data[i], data_seek, stop);

        memcpy(buffer + data_position, content_part, copy_size);
        ALC_free(content_part, KERNEL);
        
        data_position += copy_size;
        data_seek = 0;

        if (stop[0] == STOP_SYMBOL) break;
    }

    return data_position;
}

int FAT_ELF_execute_content(int ci, int argc, char* argv[], int type) {
    ELF32_program* program = ELF_read(ci, type);
    int (*programEntry)(int, char* argv[]) = (int (*)(int, char* argv[]))(program->entry_point);
    if (!programEntry) return -255;

    int result_code = programEntry(argc, argv);
    ELF_free_program(program, type);

    return result_code;
}

int FAT_write_buffer2content(int ci, const unsigned char* buffer, unsigned int offset, unsigned int size) {
    content_t* data = FAT_get_content_from_table(ci);
    if (data == NULL) return -1;
    if (data->file == NULL) return -2;

    unsigned int cluster_seek = offset / (_fs_data.sectors_per_cluster * SECTOR_SIZE);
    unsigned int data_position = 0;
    unsigned int cluster_position = 0;
    unsigned int prev_offset = offset;

    // Write to presented clusters
    for (cluster_position = cluster_seek; cluster_position < data->file->data_size && data_position < size; cluster_position++) {
        unsigned int write_size = min(size - data_position, _fs_data.sectors_per_cluster * SECTOR_SIZE);
        _cluster_writeoff(buffer + data_position, data->file->data[cluster_position], offset, write_size);

        offset = 0;
        data_position += write_size;
    }

    // Allocate cluster and write
    if (data_position < size) {
        // Calculate new variables
        unsigned int new_offset = prev_offset + data_position;
        unsigned int new_size   = size - data_position;
        const unsigned char* new_buffer = buffer + data_position;

        // Allocate cluster
        _add_cluster_to_content(ci);
        FAT_write_buffer2content(ci, new_buffer, new_offset, new_size);
    }

    return 1;
}

int FAT_change_meta(const char* path, const char* new_name) {
    char fileNamePart[256] = { 0 };
    unsigned short start = 0;
    unsigned int active_cluster = 0;
    unsigned int prev_active_cluster = 0;

    if (_fs_data.fat_type == 32) active_cluster = _fs_data.ext_root_cluster;
    else {
        printf("Function FAT_change_meta: FAT16 and FAT12 are not supported!\n");
        return -1;
    }

    directory_entry_t file_info; //holds found directory info
    if (strlen(path) == 0) { // Create main dir if it not created (root dir)
        if (_fs_data.fat_type == 32) {
            active_cluster 		 = _fs_data.ext_root_cluster;
            file_info.attributes = FILE_DIRECTORY | FILE_VOLUME_ID;
            file_info.file_size  = 0;
            file_info.high_bits  = GET_ENTRY_HIGH_BITS(active_cluster, _fs_data.fat_type);
            file_info.low_bits 	 = GET_ENTRY_LOW_BITS(active_cluster, _fs_data.fat_type);
        }
        else {
            printf("Function FAT_change_meta: FAT16 and FAT12 are not supported!\n");
            return -1;
        }
    }
    else {
        for (unsigned int iterator = 0; iterator <= strlen(path); iterator++) 
            if (path[iterator] == '\\' || path[iterator] == '\0') {
                prev_active_cluster = active_cluster;

                memset(fileNamePart, '\0', 256);
                memcpy(fileNamePart, path + start, iterator - start);

                int retVal = _directory_search(fileNamePart, active_cluster, &file_info, NULL);
                switch (retVal) {
                    case -2:
                        printf("Function FAT_change_meta: No matching directory found. Aborting...\n");
                    return -2;

                    case -1:
                        printf("Function FAT_change_meta: An error occurred in _directory_search. Aborting...\n");
                    return retVal;
                }

                start = iterator + 1;
                active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type); //prep for next search
            }
    }

    if (_directory_edit(prev_active_cluster, &file_info, new_name) != 0) {
        printf("Function FAT_change_meta: _directory_edit encountered an error. Aborting...\n");
        return -1;
    }
    
    return 0; // directory or file successfully deleted
}

int FAT_put_content(const char* path, content_t* content) {
    int parent_ci = FAT_open_content(path);
    if (parent_ci == -1) return -1;

    directory_entry_t file_info = _content_table[parent_ci]->meta;
    unsigned int active_cluster = GET_CLUSTER_FROM_ENTRY(file_info, _fs_data.fat_type);
    _remove_content_from_table(parent_ci);

    char output[13] = { 0 };
    _fatname2name((char*)content->meta.file_name, output);
    int retVal = _directory_search(output, active_cluster, NULL, NULL);
    if (retVal == -1) {
        printf("Function putFile: directorySearch encountered an error. Aborting...\n");
        return -1;
    }
    else if (retVal != -2) {
        printf("Function putFile: a file matching the name given already exists. Aborting...\n");
        return -3;
    }

    if (_directory_add(active_cluster, &content->meta) != 0) {
        printf("Function FAT_put_content: _directory_add encountered an error. Aborting...\n");
        return -1;
    }

    return 0; // file successfully written
}

int FAT_delete_content(const char* path) {
    int ci = FAT_open_content(path);
    content_t* fat_content = FAT_get_content_from_table(ci);
    if (fat_content == NULL) {
        printf("Function FAT_delete_content: FAT_open_content encountered an error. Aborting...\n");
        return -1;
    }

    unsigned int data_cluster = GET_CLUSTER_FROM_ENTRY(fat_content->meta, _fs_data.fat_type);
    unsigned int prev_cluster = 0;
    
    while (data_cluster < END_CLUSTER_32) {
        prev_cluster = __read_fat(data_cluster);
        if (_cluster_deallocate(data_cluster) != 0) {
            printf("[%s %i] _cluster_deallocate encountered an error. Aborting...\n", __FILE__, __LINE__);
            _remove_content_from_table(ci);
            return -1;
        }

        data_cluster = prev_cluster;
    }

    if (_directory_remove(fat_content->parent_cluster, (char*)fat_content->meta.file_name) != 0) {
        printf("[%s %i] _directory_remove encountered an error. Aborting...\n", __FILE__, __LINE__);
        _remove_content_from_table(ci);
        return -1;
    }

    _remove_content_from_table(ci);
    return 0; // directory or file successfully deleted
}

void FAT_copy_content(char* source, char* destination) {
    int ci_source = FAT_open_content(source);

    content_t* fat_content = FAT_get_content_from_table(ci_source);
    content_t* dst_content = NULL;

    directory_entry_t content_meta;
    memcpy(&content_meta, &fat_content->meta, sizeof(directory_entry_t));

    if (fat_content->directory != NULL) dst_content = FAT_create_object(fat_content->directory->name, 1, NULL);
    else if (fat_content->file != NULL) dst_content = FAT_create_object(fat_content->file->name, 0, fat_content->file->extension);

    directory_entry_t dst_meta;		
    memcpy(&dst_meta, &dst_content->meta, sizeof(directory_entry_t));

    int ci_destination = FAT_put_content(destination, dst_content);
    unsigned int data_cluster = GET_CLUSTER_FROM_ENTRY(content_meta, _fs_data.fat_type);
    unsigned int dst_cluster  = GET_CLUSTER_FROM_ENTRY(dst_meta, _fs_data.fat_type);

    while (data_cluster < END_CLUSTER_32) {
        _add_cluster_to_content(ci_destination);
        dst_cluster = __read_fat(dst_cluster);
        _copy_cluster2cluster(data_cluster, dst_cluster);
        data_cluster = __read_fat(data_cluster);
    }

    _remove_content_from_table(ci_destination);
    _remove_content_from_table(ci_source);
}

int FAT_stat_content(int ci, CInfo_t* info) {
    content_t* content = FAT_get_content_from_table(ci);
    if (!content) {
        info->type = NOT_PRESENT;
        return -1;
    }

    if (content->content_type == CONTENT_TYPE_DIRECTORY) {
        info->size = 0;
        strcpy((char*)info->full_name, (char*)content->directory->name);
        info->type = STAT_DIR;
    }
    else if (content->content_type == CONTENT_TYPE_FILE) {
        info->size = content->file->data_size * _fs_data.sectors_per_cluster * SECTOR_SIZE;
        strcpy((char*)info->full_name, (char*)content->meta.file_name);
        strcpy(info->file_name, content->file->name);
        strcpy(info->file_extension, content->file->extension);
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

int _add_content2table(content_t* content) {
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) {
        if (!_content_table[i]) {
            _content_table[i] = content;
            return i;
        }
    }

    return -1;
}

int _remove_content_from_table(int index) {
    if (!_content_table[index]) return -1;
    int result = FAT_unload_content_system(_content_table[index]);
    _content_table[index] = NULL;
    return result;
}

void _fatname2name(char* input, char* output) {
    if (input[0] == '.') {
        if (input[1] == '.') {
            strcpy(output, "..");
            return;
        }

        strcpy (output, ".");
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

    strcpy(input, searchName);
    return input;
}

unsigned short _current_time() {
    _datetime_read_rtc();
    return (DTM_datetime.hour << 11) | (DTM_datetime.minute << 5) | (DTM_datetime.second / 2);
}

unsigned short _current_date() {
    _datetime_read_rtc();

    unsigned short reversed_data = 0;
    reversed_data |= DTM_datetime.day & 0x1F;
    reversed_data |= (DTM_datetime.month & 0xF) << 5;
    reversed_data |= ((DTM_datetime.year - 1980) & 0x7F) << 9;

    return reversed_data;
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

static directory_entry_t* _create_entry(const char* name, const char* ext, int isDir, unsigned int firstCluster, unsigned int filesize) {
    directory_entry_t* data = (directory_entry_t*)ALC_malloc(sizeof(directory_entry_t), KERNEL);
    if (!data) {
        return NULL;
    }

    data->reserved0 			 = 0; 
    data->creation_time_tenths 	 = 0;
    data->creation_time 		 = 0;
    data->creation_date 		 = 0;
    data->last_modification_date = 0;

    char* file_name = (char*)ALC_malloc(25, KERNEL);
    if (!file_name) {
        ALC_free(data, KERNEL);
        return NULL;
    }
    
    strcpy(file_name, name);
    if (ext) {
        strcat(file_name, ".");
        strcat(file_name, ext);
    }
    
    data->low_bits 	= firstCluster;
    data->high_bits = firstCluster >> 16;  

    if (isDir == 1) {
        data->file_size  = 0;
        data->attributes = FILE_DIRECTORY;
    } 
    else {
        data->file_size  = filesize;
        data->attributes = FILE_ARCHIVE;
    }

    data->creation_date = _current_date();
    data->creation_time = _current_time();
    data->creation_time_tenths = _current_time();

    if (_name_check(file_name) != 0) _name2fatname(file_name);
    strncpy((char*)data->file_name, file_name, min(11, strlen(file_name)));
    ALC_free(file_name, KERNEL);

    return data; 
}

content_t* FAT_create_object(char* name, int is_directory, char* extension) {
    content_t* content = FAT_create_content();
    if (strlen(name) > 11 || strlen(extension) > 4) {
        printf("Uncorrect name or ext lenght.\n");
        FAT_unload_content_system(content);
        return NULL;
    }
    
    if (is_directory) {
        content->content_type = CONTENT_TYPE_DIRECTORY;
        content->directory = _create_directory();
        strncpy(content->directory->name, name, 12);

        directory_entry_t* meta = _create_entry(name, NULL, 1, _cluster_allocate(), 0);
        if (meta) memcpy(&content->meta, meta, sizeof(directory_entry_t));
    }
    else {
        content->content_type = CONTENT_TYPE_FILE;
        content->file = _create_file();
        strncpy(content->file->name, name, 8);
        strncpy(content->file->extension, extension, 4);
        
        directory_entry_t* meta = _create_entry(name, extension, 0, _cluster_allocate(), 1);
        if (meta) memcpy(&content->meta, meta, sizeof(directory_entry_t));
    }

    return content;
}

content_t* FAT_create_content() {
    content_t* content = (content_t*)malloc(sizeof(content_t));
    if (!content) return NULL;
    content->directory      = NULL;
    content->file           = NULL;
    content->parent_cluster = -1;
    return content;
}

directory_t* _create_directory() {
    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
    if (!directory) return NULL;
    return directory;
}

file_t* _create_file() {
    file_t* file = (file_t*)malloc(sizeof(file_t));
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