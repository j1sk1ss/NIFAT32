#include "../include/ecache.h"

static int _rotate_left(ecache_t** root, ecache_t* x) {
    ecache_t* y = x->r;
    x->r = y->l;
    if (y->l) y->l->p = x;
    y->p = x->p;

    if (!x->p) *root = y;
    else if (x == x->p->l) x->p->l = y;
    else x->p->r = y;

    y->l = x;
    x->p = y;
    return 1;
}

static int _rotate_right(ecache_t** root, ecache_t* x) {
    ecache_t* y = x->l;
    x->l = y->r;
    if (y->r) y->r->p = x;
    y->p = x->p;

    if (!x->p) *root = y;
    else if (x == x->p->r) x->p->r = y;
    else x->p->l = y;

    y->r = x;
    x->p = y;
    return 1;
}

static int _fix_insert(ecache_t** root, ecache_t* z) {
    while (z->p && IS_ECACHE_RED(z->p)) {
        ecache_t* gp = z->p->p;
        if (z->p == gp->l) {
            ecache_t* y = gp->r;
            if (y && IS_ECACHE_RED(y)) {
                SET_ECACHE_BLACK(z->p);
                SET_ECACHE_BLACK(y);
                SET_ECACHE_RED(gp);
                z = gp;
            } 
            else {
                if (z == z->p->r) {
                    z = z->p;
                    _rotate_left(root, z);
                }

                SET_ECACHE_BLACK(z->p);
                SET_ECACHE_RED(gp);
                _rotate_right(root, gp);
            }
        } else {
            ecache_t* y = gp->l;
            if (y && IS_ECACHE_RED(y)) {
                SET_ECACHE_BLACK(z->p);
                SET_ECACHE_BLACK(y);
                SET_ECACHE_RED(gp);
                z = gp;
            } 
            else {
                if (z == z->p->l) {
                    z = z->p;
                    _rotate_right(root, z);
                }

                SET_ECACHE_BLACK(z->p);
                SET_ECACHE_RED(gp);
                _rotate_left(root, gp);
            }
        }
    }

    SET_ECACHE_BLACK((*root));
    return 1;
}

ecache_t* ecache_insert(ecache_t* root, ripemd160_t hash, unsigned char is_dir, cluster_addr_t ca) {
    ecache_t* z = (ecache_t*)malloc_s(sizeof(ecache_t));
    if (!z) return root;

    str_memcpy(z->hash, hash, sizeof(ripemd160_t));
    z->ca = ca;
    z->l = z->r = z->p = NULL;
    SET_ECACHE_RED(z);
    if (is_dir) SET_ECACHE_DIR(z);
    else SET_ECACHE_FILE(z); 

    ecache_t* y = NULL;
    ecache_t* x = root;

    while (x) {
        y = x;
        int cmp = str_memcmp(hash, x->hash, sizeof(ripemd160_t));
        if (cmp < 0) x = x->l;
        else if (cmp > 0) x = x->r;
        else {
            free_s(z);
            return root;
        }
    }

    z->p = y;
    if (!y) root = z;
    else if (str_memcmp(hash, y->hash, sizeof(ripemd160_t)) < 0) y->l = z;
    else y->r = z;

    _fix_insert(&root, z);
    return root;
}

ecache_t* ecache_find(ecache_t* root, ripemd160_t hash) {
    while (root) {
        int cmp = str_memcmp(hash, root->hash, sizeof(ripemd160_t));
        if (cmp < 0) root = root->l;
        else if (cmp > 0) root = root->r;
        else return root;
    }

    return NULL;
}

int ecache_free(ecache_t* root) {
    if (!root) return 0;
    ecache_free(root->l);
    ecache_free(root->r);
    free_s(root);
    return 1;
}
