#include <src/ctable.h>

static content_t _content_table[CONTENT_TABLE_SIZE];

int ctable_init() {
    for (ci_t i = 0; i < CONTENT_TABLE_SIZE; i++) {
        _content_table[i].content_type = CONTENT_TYPE_EMPTY;
        _content_table[i].index.root   = NO_ECACHE;
    }
    
    return 1;
}

lock_t _content_lock = NULL_LOCK;

static int _init_content(ci_t ci) {
    _content_table[ci].content_type   = CONTENT_TYPE_UNKNOWN;
    _content_table[ci].parent_cluster = FAT_CLUSTER_BAD;
    _content_table[ci].index.root     = NO_ECACHE;
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
    ci_t ci, int is_dir, const char* __restrict name83, directory_entry_t* __restrict meta, unsigned char mode
) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    _content_table[ci].content_type = is_dir ? CONTENT_TYPE_DIRECTORY : CONTENT_TYPE_FILE;
    if (is_dir) str_strncpy(_content_table[ci].directory.name, name83, 11);
    else {
        char name[12] = { 0 };
        char ext[6]   = { 0 };
        unpack_83_name(name83, name, ext);
        str_strncpy(_content_table[ci].file.name, name, 8);
        str_strncpy(_content_table[ci].file.extension, ext, 3);
    }

    if (meta) {
        str_memcpy(&_content_table[ci].meta, meta, sizeof(directory_entry_t));
        _content_table[ci].parent_cluster = meta->rca;
        _content_table[ci].data_cluster   = meta->dca;
    }

    _content_table[ci].mode = mode;
    return 1;
}

cluster_addr_t get_content_data_ca(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return FAT_CLUSTER_BAD;
    return _content_table[ci].data_cluster;
}

int set_content_data_ca(const ci_t ci, cluster_addr_t ca) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return FAT_CLUSTER_BAD;
    _content_table[ci].data_cluster = ca;
    return 1;
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

unsigned char get_content_mode(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    return _content_table[ci].mode;
}

content_type_t get_content_type(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return CONTENT_TYPE_UNKNOWN;
    return _content_table[ci].content_type; 
}

ecache_t* get_content_ecache(const ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return NO_ECACHE;
    return _content_table[ci].index.root;
}

int stat_content(const ci_t ci, cinfo_t* info) {
    switch (_content_table[ci].content_type) {
        case CONTENT_TYPE_DIRECTORY: {
            info->size = 0;
            str_memcpy(info->full_name, _content_table[ci].directory.name, 11);
            info->type = STAT_DIR;
            break;
        }
        case CONTENT_TYPE_FILE: {
            str_memcpy(info->full_name, _content_table[ci].meta.file_name, 11);
            str_strncpy(info->file_name, _content_table[ci].file.name, 8);
            str_strncpy(info->file_extension, _content_table[ci].file.extension, 3);
            info->type = STAT_FILE;
            break;
        }
        default: return 0;
    }

    return 1;
}

int index_content(const ci_t ci, fat_data_t* fi) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    if (_content_table[ci].index.root) ecache_free(_content_table[ci].index.root);
    entry_index(_content_table[ci].data_cluster, &_content_table[ci].index.root, fi);
    return 1;
}

int destroy_content(ci_t ci) {
    if (ci > CONTENT_TABLE_SIZE || ci < 0) return 0;
    if (_content_table[ci].content_type == CONTENT_TYPE_EMPTY) return 0;
    if (_content_table[ci].index.root) ecache_free(_content_table[ci].index.root);
    _content_table[ci].content_type = CONTENT_TYPE_EMPTY;
    _content_table[ci].index.root   = NO_ECACHE;
    return 1;
}
