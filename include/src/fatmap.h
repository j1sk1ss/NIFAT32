#ifndef FATMAP_H_
#define FATMAP_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/mm.h>
#include <std/str.h>
#include <std/null.h>
#include <std/logging.h>
#include <std/threading.h>
#include <src/errors.h>
#include <src/fatinfo.h>

#define BITMAP_NFREE 0x00000000
#define BITMAP_IS_FREE(val, pos)    (!(((val) >> (pos)) & 1))
typedef unsigned int bitmap_val_t;

#define BITS_PER_WORD (sizeof(bitmap_val_t) * 8)

int fatmap_init(fat_data_t* fi);
int fatmap_set(unsigned int ca);
int fatmap_unset(unsigned int ca);
unsigned int fatmap_find_free(unsigned int offset, int size, fat_data_t* fi);
int fatmap_unload();

#ifdef __cplusplus
}
#endif
#endif