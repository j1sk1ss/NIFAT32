#include "../include/fatmap.h"

static bitmap_val_t* _bitmap = NULL;
int fatmap_init(fat_data_t* fi) {
    _bitmap = (bitmap_val_t*)malloc_s((fi->total_clusters / BITS_PER_WORD) * sizeof(bitmap_val_t));
    if (!_bitmap) {
        print_error("malloc_s() error!");
        return 0;
    }

    str_memset(_bitmap, 0x00, (fi->total_clusters / BITS_PER_WORD) * sizeof(bitmap_val_t));
    return 1;
}

int fatmap_set(unsigned int ca) {
    if (!_bitmap) return 0;
    _bitmap[ca / BITS_PER_WORD] |= (1U << (ca % BITS_PER_WORD));
    return 1;
}

int fatmap_unset(unsigned int ca) {
    if (!_bitmap) return 0;
    _bitmap[ca / BITS_PER_WORD] &= ~(1U << (ca % BITS_PER_WORD));
    return 1;
}

unsigned int fatmap_find_free(unsigned int offset, int size, fat_data_t* fi) {
    if (!_bitmap) return 0;
    if (size <= 0 || offset >= fi->total_clusters) return 0;
    unsigned int limit = fi->total_clusters - offset - size;
    for (unsigned int i = offset; i <= limit; ++i) {
        int found = 1;
        for (int j = 0; j < size; ++j) {
            unsigned int bit_index = i + j;
            if ((_bitmap[bit_index / BITS_PER_WORD] >> (bit_index % BITS_PER_WORD)) & 1U) {
                found = 0;
                i = bit_index;
                break;
            }
        }

        if (found) return i;
    }

    return 0;
}

int fatmap_unload() {
    if (_bitmap) free_s(_bitmap);
    return 1;
}
