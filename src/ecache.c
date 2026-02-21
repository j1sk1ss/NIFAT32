#include <src/ecache.h>

#ifndef NIFAT32_NO_ECACHE
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
#endif

ecache_t* ecache_insert(ecache_t* root, checksum_t hash, unsigned char is_dir, cluster_addr_t ca) {
#ifndef NIFAT32_NO_ECACHE
    ecache_t* z = (ecache_t*)malloc_s(sizeof(ecache_t));
    if (!z) return root;

    z->hash = hash;
    z->ca   = ca;
    z->l = z->r = z->p = NULL;
    SET_ECACHE_RED(z);
    if (is_dir) SET_ECACHE_DIR(z);
    else        SET_ECACHE_FILE(z); 

    ecache_t* y = NULL;
    ecache_t* x = root;
    while (x) {
        y = x;
        if (hash < x->hash) x = x->l;
        else if (hash > x->hash) x = x->r;
        else {
            free_s(z);
            return root;
        }
    }

    z->p = y;
    if (!y) root = z;
    else if (hash < y->hash) y->l = z;
    else y->r = z;

    _fix_insert(&root, z);
#endif
    return root;
}

ecache_t* ecache_find(ecache_t* root, checksum_t hash) {
#ifndef NIFAT32_NO_ECACHE
    while (root) {
        if (hash < root->hash)      root = root->l;
        else if (hash > root->hash) root = root->r;
        else return root;
    }
#endif
    return NULL;
}

#ifndef NIFAT32_NO_ECACHE
static ecache_t* _minimum(ecache_t* node) {
    while (node->l) node = node->l;
    return node;
}

static int _transplant(ecache_t** root, ecache_t* u, ecache_t* v) {
    if (!u->p) *root = v;
    else if (u == u->p->l) u->p->l = v;
    else u->p->r = v;
    if (v) v->p = u->p;
    return 1;
}

static int _fix_delete(ecache_t** root, ecache_t* x, ecache_t* x_parent) {
    while (x != *root && (!x || IS_ECACHE_BLACK(x))) {
        if (x == x_parent->l) {
            ecache_t* w = x_parent->r;
            if (IS_ECACHE_RED(w)) {
                SET_ECACHE_BLACK(w);
                SET_ECACHE_RED(x_parent);
                _rotate_left(root, x_parent);
                w = x_parent->r;
            }

            if ((!w->l || IS_ECACHE_BLACK(w->l)) &&
                (!w->r || IS_ECACHE_BLACK(w->r))) {
                SET_ECACHE_RED(w);
                x = x_parent;
                x_parent = x->p;
            } 
            else {
                if (!w->r || IS_ECACHE_BLACK(w->r)) {
                    if (w->l) SET_ECACHE_BLACK(w->l);
                    SET_ECACHE_RED(w);
                    _rotate_right(root, w);
                    w = x_parent->r;
                }

                if (w) {
                    if (IS_ECACHE_RED(x_parent)) SET_ECACHE_RED(w);
                    else SET_ECACHE_BLACK(w);
                }

                SET_ECACHE_BLACK(x_parent);
                if (w && w->r) SET_ECACHE_BLACK(w->r);
                _rotate_left(root, x_parent);
                x = *root;
                break;
            }
        } 
        else {
            ecache_t* w = x_parent->l;
            if (IS_ECACHE_RED(w)) {
                SET_ECACHE_BLACK(w);
                SET_ECACHE_RED(x_parent);
                _rotate_right(root, x_parent);
                w = x_parent->l;
            }

            if ((!w->r || IS_ECACHE_BLACK(w->r)) &&
                (!w->l || IS_ECACHE_BLACK(w->l))) {
                SET_ECACHE_RED(w);
                x = x_parent;
                x_parent = x->p;
            } 
            else {
                if (!w->l || IS_ECACHE_BLACK(w->l)) {
                    if (w->r) SET_ECACHE_BLACK(w->r);
                    SET_ECACHE_RED(w);
                    _rotate_left(root, w);
                    w = x_parent->l;
                }

                if (w) {
                    if (IS_ECACHE_RED(x_parent)) SET_ECACHE_RED(w);
                    else SET_ECACHE_BLACK(w);
                }

                SET_ECACHE_BLACK(x_parent);
                if (w && w->l) SET_ECACHE_BLACK(w->l);
                _rotate_right(root, x_parent);
                x = *root;
                break;
            }
        }
    }

    if (x) SET_ECACHE_BLACK(x);
    return 1;
}
#endif

ecache_t* ecache_delete(ecache_t* root, checksum_t hash) {
#ifndef NIFAT32_NO_ECACHE
    ecache_t* z = ecache_find(root, hash);
    if (!z) return root;

    ecache_t* y = z;
    ecache_t* x = NULL;
    ecache_t* x_parent = NULL;

    if (!z->l) {
        x = z->r;
        x_parent = z->p;
        _transplant(&root, z, z->r);
    } 
    else if (!z->r) {
        x = z->l;
        x_parent = z->p;
        _transplant(&root, z, z->l);
    } 
    else {
        y = _minimum(z->r);
        x = y->r;

        if (y->p == z) {
            if (x) x->p = y;
            x_parent = y;
        } 
        else {
            _transplant(&root, y, y->r);
            y->r = z->r;
            if (y->r) y->r->p = y;
            x_parent = y->p;
        }

        _transplant(&root, z, y);
        y->l = z->l;
        if (y->l) y->l->p = y;
        SET_ECACHE_COLOR_VAL(y, GET_ECACHE_COLOR(z));
    }

    free_s(z);
    if (IS_ECACHE_BLACK(y)) {
        _fix_delete(&root, x, x_parent);
    }
#endif
    return root;
}

int ecache_free(ecache_t* root) {
#ifndef NIFAT32_NO_ECACHE
    if (!root) return 0;
    ecache_free(root->l);
    ecache_free(root->r);
    free_s(root);
#endif
    return 1;
}
