#include <nft32/fatmap.h>

static bitmap_val_t* _bitmap = NULL;
int fatmap_init(fat_data_t* fi) {
#ifndef NO_FAT_MAP
    _bitmap = (bitmap_val_t*)nft32_malloc_s((fi->total_clusters / BITS_PER_WORD) * sizeof(bitmap_val_t));
    if (!_bitmap) {
        print_error("nft32_malloc_s() error!");
        errors_register_error(MALLOC_ERROR, fi);
        return 0;
    }

    nft32_str_memset(_bitmap, 0x00, (fi->total_clusters / BITS_PER_WORD) * sizeof(bitmap_val_t));
    return 1;
#endif
    print_warn("fatmap_init() is not implemented! Don't provide the 'NO_FAT_MAP'!");
    return 1;
}

int fatmap_set(unsigned int ca) {
#ifndef NO_FAT_MAP
    if (!_bitmap) return 0;
    _bitmap[ca / BITS_PER_WORD] |= (1U << (ca % BITS_PER_WORD));
    return 1;
#endif
    print_warn("fatmap_set() is not implemented! Don't provide the 'NO_FAT_MAP'!");
    return 1;
}

int fatmap_unset(unsigned int ca) {
#ifndef NO_FAT_MAP
    if (!_bitmap) return 0;
    _bitmap[ca / BITS_PER_WORD] &= ~(1U << (ca % BITS_PER_WORD));
    return 1;
#endif
    print_warn("fatmap_unset() is not implemented! Don't provide the 'NO_FAT_MAP'!");
    return 1;
}

unsigned int fatmap_find_free(unsigned int offset, int size, fat_data_t* fi) {
#ifndef NO_FAT_MAP
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
#endif
    print_warn("fatmap_find_free() is not implemented! Don't provide the 'NO_FAT_MAP'!");
    return 0;
}

int fatmap_unload() {
#ifndef NO_FAT_MAP
    if (_bitmap) nft32_free_s(_bitmap);
    return 1;
#endif
    print_warn("fatmap_unload() is not implemented! Don't provide the 'NO_FAT_MAP'!");
    return 1;
}
