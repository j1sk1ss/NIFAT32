#include "../include/ctable.h"

static content_t* _content_table[CONTENT_TABLE_SIZE];

int ctable_init() {
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) _content_table[i] = NULL;
    return 1;
}

content_t* create_content() {
    content_t* content = (content_t*)malloc_s(sizeof(content_t));
    if (!content) return NULL;
    content->directory = NULL;
    content->file      = NULL;
    content->parent_cluster = FAT_CLUSTER_BAD;
    return content;
}

int unload_content_system(content_t* content) {
    if (!content) return -1;
    if (content->content_type == CONTENT_TYPE_DIRECTORY) free_s(content->directory);
    else if (content->content_type == CONTENT_TYPE_FILE) free_s(content->file);
    free_s(content);
    return 1;
}

ci_t add_content2table(content_t* content) {
    for (int i = 0; i < CONTENT_TABLE_SIZE; i++) {
        if (!_content_table[i]) {
            _content_table[i] = content;
            return i;
        }
    }

    return -1;
}

int remove_content_from_table(ci_t ci) {
    if (!_content_table[ci]) return -1;
    int result = unload_content_system(_content_table[ci]);
    _content_table[ci] = NULL;
    return result;
}

content_t* get_content_from_table(const ci_t ci) {
    return _content_table[ci];
}
