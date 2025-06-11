#include "../include/ctable.h"

static content_t _content_table[CONTENT_TABLE_SIZE];

int ctable_init() {
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) _content_table[i].content_type = CONTENT_TYPE_EMPTY;
    return 1;
}

lock_t _content_lock = NULL_LOCK;

static int _init_content(ci_t ci) {
    _content_table[ci].content_type   = CONTENT_TYPE_UNKNOWN;
    _content_table[ci].parent_cluster = FAT_CLUSTER_BAD;
    return 1;
}

ci_t alloc_ci() {
    ci_t empty = -1;
    if (THR_require_write(&_content_lock, get_thread_num())) {
        for (int i = 0; i < CONTENT_TABLE_SIZE; i++) {
            if (_content_table[i].content_type == CONTENT_TYPE_EMPTY) {
                _init_content(i);
                empty = i;
                break;
            }
        }

        THR_release_write(&_content_lock, get_thread_num());
    }

    return empty;
}

int setup_content(
    ci_t ci, int is_dir, const char* name83, cluster_addr_t root, cluster_addr_t data, directory_entry_t* meta
) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    _content_table[ci].content_type = is_dir ? CONTENT_TYPE_DIRECTORY : CONTENT_TYPE_FILE;
    if (!is_dir) {
        char name[12] = { 0 };
        char ext[6] = { 0 };
        unpack_83_name(name83, name, ext);
        str_strcpy(_content_table[ci].file.name, name);
        str_strcpy(_content_table[ci].file.extension, ext);
    }
    else {
        str_strcpy(_content_table[ci].directory.name, name83);
    }

    str_memcpy(&_content_table[ci].meta, meta, sizeof(directory_entry_t));
    _content_table[ci].parent_cluster = root;
    _content_table[ci].data_cluster   = data;
    return 1;
}

cluster_addr_t get_content_data_ca(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return FAT_CLUSTER_BAD;
    return _content_table[ci].data_cluster;
}

unsigned int get_content_size(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    return _content_table[ci].meta.file_size;
}

const char* get_content_name(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return NULL;
    return (const char*)_content_table[ci].meta.file_name;
}

cluster_addr_t get_content_root_ca(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return FAT_CLUSTER_BAD;
    return _content_table[ci].parent_cluster;
}

int stat_content(const ci_t ci, cinfo_t* info) {
    if (_content_table[ci].content_type == CONTENT_TYPE_DIRECTORY) {
        info->size = 0;
        str_memcpy(info->full_name, _content_table[ci].directory.name, 11);
        info->type = STAT_DIR;
    }
    else if (_content_table[ci].content_type == CONTENT_TYPE_FILE) {
        str_memcpy(info->full_name, _content_table[ci].meta.file_name, 11);
        str_strncpy(info->file_name, _content_table[ci].file.name, 8);
        str_strncpy(info->file_extension, _content_table[ci].file.extension, 3);
        info->type = STAT_FILE;
    }
    else {
        return 0;
    }

    return 1;
}

int destroy_content(ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    if (_content_table[ci].content_type == CONTENT_TYPE_EMPTY) return 0;
    _content_table[ci].content_type = CONTENT_TYPE_EMPTY;
    return 1;
}
