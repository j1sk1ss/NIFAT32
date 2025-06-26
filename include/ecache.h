#ifndef ECACHE_H_
#define ECACHE_H_

#include "mm.h"
#include "fat.h"
#include "str.h"
#include "ripemd160.h"

typedef enum { RED, BLACK } node_color_t;

typedef struct ecache {
    struct ecache* p;
    struct ecache* l;
    struct ecache* r;
    node_color_t   color;
    ripemd160_t    hash;
    cluster_addr_t ca;
} ecache_t;

ecache_t* ecache_insert(ecache_t* root, ripemd160_t hash, cluster_addr_t ca);
ecache_t* ecache_find(ecache_t* root, ripemd160_t hash);
int ecache_free(ecache_t* root);

#endif