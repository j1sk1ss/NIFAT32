#ifndef ECACHE_H_
#define ECACHE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/mm.h>
#include <std/str.h>
#include <std/checksum.h>
#include <src/fat.h>

#define ECACHE_BLACK   0b00000001
#define ECACHE_FILE    0b00000010
#define ECACHE_DIR     0b00000100

#define IS_ECACHE_RED(n)     (((n) != NULL) && (((n)->flags & ECACHE_BLACK) == 0))
#define IS_ECACHE_BLACK(n)   (((n) != NULL) && (((n)->flags & ECACHE_BLACK) != 0))
#define SET_ECACHE_RED(n)    do { if (n) (n)->flags &= ~ECACHE_BLACK; } while (0)
#define SET_ECACHE_BLACK(n)  do { if (n) (n)->flags |= ECACHE_BLACK; } while (0)

#define IS_ECACHE_FILE(n)    (((n) != NULL) && (((n)->flags & ECACHE_FILE) != 0))
#define IS_ECACHE_DIR(n)     (((n) != NULL) && (((n)->flags & ECACHE_DIR) != 0))
#define SET_ECACHE_FILE(n)   do { if (n) (n)->flags |= ECACHE_FILE; } while (0)
#define SET_ECACHE_DIR(n)    do { if (n) (n)->flags |= ECACHE_DIR; } while (0)

#define GET_ECACHE_COLOR(n) (((n) != NULL) ? ((n)->flags & ECACHE_BLACK) : ECACHE_BLACK)
#define SET_ECACHE_COLOR_VAL(n, val) do { \
    if (n) { \
        if (val & ECACHE_BLACK) SET_ECACHE_BLACK(n); \
        else SET_ECACHE_RED(n); \
    } \
} while (0)

typedef struct ecache {
    struct ecache* p;
    struct ecache* l;
    struct ecache* r;
    unsigned char  flags;
    checksum_t     hash;
    cluster_addr_t ca;
} __attribute__((__packed__)) ecache_t;

ecache_t* ecache_insert(ecache_t* root, checksum_t hash, unsigned char is_dir, cluster_addr_t ca);
ecache_t* ecache_delete(ecache_t* root, checksum_t hash);
ecache_t* ecache_find(ecache_t* root, checksum_t hash);
int ecache_free(ecache_t* root);

#ifdef __cplusplus
}
#endif
#endif
