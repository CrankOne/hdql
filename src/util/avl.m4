define(_M4_pubfunc, `$1_$2')dnl
dnl                                                                    _______
dnl _________________________________________________________________/ Implem
dnl
#include "hdql/util/avl-tree.h"
#include "hdql/types.h"
#include "hdql/util/allocator.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* max depth of the AVL tree for non-recursive implementations */
#ifndef HDQL_AVL_MAX_HEIGHT
#   define HDQL_AVL_MAX_HEIGHT 128
#endif

/*#define HDQL_AVL_RECURSIVE_IMPLEMS 1*/

typedef struct _M_AVLNode _M_AVLNode;

struct _M_AVLNode {
    _M_AVLNode *left, *right;
    int height;
    ifelse(_M_isMap, `true', `void *value;')
    _M_keyType key;
};

struct _M_AVL {
    _M_AVLNode *root;
    const struct hdql_Allocator *alloc;
    ifelse(_M_isKeyFixed, `true', `size_t keyLen;')
    size_t nItems;
};


static int h(const _M_AVLNode *n) { return n ? n->height : 0; }
static int max_i(int a, int b) { return a > b ? a : b; }
static void fix_height(_M_AVLNode *n) { n->height = 1 + max_i(h(n->left), h(n->right)); }
static int balance_factor(const _M_AVLNode *n) { return h(n->right) - h(n->left); }
/* unsigned big-endian integer comparison */
static int key_cmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

static _M_AVLNode *
rot_left(_M_AVLNode *a) {
    _M_AVLNode *b = a->right;
    a->right = b->left;
    b->left = a;

    fix_height(a);
    fix_height(b);
    return b;
}

static _M_AVLNode *
rot_right(_M_AVLNode *a) {
    _M_AVLNode *b = a->left;
    a->left = b->right;
    b->right = a;

    fix_height(a);
    fix_height(b);
    return b;
}

static _M_AVLNode *balance(_M_AVLNode *n) {
    fix_height(n);
    if (balance_factor(n) == 2) {
        if (balance_factor(n->right) < 0)
            n->right = rot_right(n->right);
        return rot_left(n);
    }
    if (balance_factor(n) == -2) {
        if (balance_factor(n->left) > 0)
            n->left = rot_left(n->left);
        return rot_right(n);
    }
    return n;
}

static _M_AVLNode *
node_new(const _M_keyType key
        ifelse(_M_isKeyFixed, `true', `, size_t keyLen')
        ifelse(_M_isMap, `true', `, void *value')
        , const struct hdql_Allocator *alloc) {
    _M_AVLNode *n = alloc->alloc(sizeof(*n), alloc->userdata);
    if (!n) return NULL;
    n->key = alloc->alloc(keyLen, alloc->userdata);
    if(!n->key) {
        alloc->free(n, alloc->userdata);
        return NULL;
    }
    memcpy(n->key, key, keyLen);
    n->left = NULL;
    n->right = NULL;
    n->height = 1;
    ifelse(_M_isMap, `true', `n->value = value;')
    return n;
}

static void node_free_all(_M_AVLNode *n, const struct hdql_Allocator *alloc) {
    if (!n) return;
    node_free_all(n->left, alloc);
    node_free_all(n->right, alloc);
    alloc->free(n->key, alloc->userdata);
    alloc->free(n, alloc->userdata);
}

#if defined(HDQL_AVL_RECURSIVE_IMPLEMS) && HDQL_AVL_RECURSIVE_IMPLEMS

static int
insert_recursive( _M_AVLNode **out
                , _M_AVLNode *n
                , const _M_keyType key
                , size_t keyLen
                ifelse(_M_isMap, `true', `, void *value')
                , const struct hdql_Allocator *alloc
                ) {
    if (!n) {
        *out = node_new(key, keyLen ifelse(_M_isMap, `true', `, value'), alloc);
        return *out ? HDQL_AVL_OK : HDQL_AVL_MEM_ERROR;
    }
    int c = key_cmp(key, n->key, keyLen);
    if (c == 0) {
        ifelse(_M_isMap, `true', `n->value = value;')
        *out = n;
        return HDQL_AVL_CHANGED;
    }
    int rc;
    if (c < 0)
        rc = insert_recursive(&n->left, n->left, key, keyLen ifelse(_M_isMap, `true', `, value'), alloc);
    else
        rc = insert_recursive(&n->right, n->right, key, keyLen ifelse(_M_isMap, `true', `, value'), alloc);
    if (rc < 0) {
        *out = n;
        return rc;
    }

    *out = balance(n);
    return rc;
}

#else  /* HDQL_AVL_RECURSIVE_IMPLEMS */

static int
insert_iterative( struct _M_AVL *m
        , const _M_keyType key
        ifelse(_M_isMap, `true', `, void *value')
        ) {
    _M_AVLNode **path[HDQL_AVL_MAX_HEIGHT];
    _M_AVLNode **link;
    _M_AVLNode *n;
    int depth = 0;

    if (!m || !key) return HDQL_AVL_BAD_ARG_ERROR;

    link = &m->root;

    while (*link) {
        int rc;

        if (depth >= HDQL_AVL_MAX_HEIGHT)
            return HDQL_AVL_MEM_ERROR; /* should not happen for a sane AVL tree */

        path[depth++] = link;

        n = *link;
        rc = key_cmp(key, n->key, m->keyLen);

        if (rc == 0) {
            ifelse(_M_isMap, `true', `n->value = value;')
            return HDQL_AVL_CHANGED; /* replaced */
        }

        link = rc < 0 ? &n->left : &n->right;
    }

    *link = node_new(key, m->keyLen ifelse(_M_isMap, `true', `, value'), m->alloc);
    if (!*link)
        return HDQL_AVL_MEM_ERROR;

    /* Also include the newly inserted leaf in the path? Not necessary:
     * it already has correct height = 1. Rebalance ancestors from bottom to top.
     */
    while (depth > 0) {
        _M_AVLNode **plink = path[--depth];
        *plink = balance(*plink);
    }

    return HDQL_AVL_OK; /* inserted */
}
#endif  /* HDQL_AVL_RECURSIVE_IMPLEMS */

static void
node_delete(_M_AVLNode *n, const struct hdql_Allocator *alloc) {
    if(!n) return;
    alloc->free(n->key, alloc->userdata);
    alloc->free(n, alloc->userdata);
}

#if defined(HDQL_AVL_RECURSIVE_IMPLEMS) && HDQL_AVL_RECURSIVE_IMPLEMS

static _M_AVLNode *
detach_min(_M_AVLNode *n, _M_AVLNode **min_node) {
    if (!n->left) {
        *min_node = n;
        return n->right;
    }
    n->left = detach_min(n->left, min_node);
    return balance(n);
}

static _M_AVLNode *
erase_recursive(_M_AVLNode *n
        , const _M_keyType key
        , size_t keyLen
        ifelse(_M_isMap, `true', `, void **oldValue')
        , int *removed
        , const struct hdql_Allocator *alloc
        ) {
    int rc;
    if(!n) return NULL;
    rc = key_cmp(key, n->key, keyLen);
    if (rc < 0) {
        n->left = erase_recursive(n->left, key, keyLen ifelse(_M_isMap, `true', `, oldValue'), removed, alloc);
        return balance(n);
    }

    if (rc > 0) {
        n->right = erase_recursive(n->right, key, keyLen ifelse(_M_isMap, `true', `, oldValue'), removed, alloc);
        return balance(n);
    }

    {
        _M_AVLNode *left = n->left;
        _M_AVLNode *right = n->right;
        *removed = HDQL_AVL_CHANGED;
        ifelse(_M_isMap, `true', `if(oldValue) *oldValue = n->value;')
        if (!right) {
            node_delete(n, alloc);
            return left;
        }

        {
            _M_AVLNode *min = NULL;
            right = detach_min(right, &min);
            min->left = left;
            min->right = right;
            node_delete(n, alloc);
            return balance(min);
        }
    }
}

#else  /* HDQL_AVL_RECURSIVE_IMPLEMS */

static int
erase_iterative(struct _M_AVL *m, const _M_keyType key ifelse(_M_isMap, `true', `, void **oldValue')) {
    _M_AVLNode **path[HDQL_AVL_MAX_HEIGHT];
    _M_AVLNode **link;
    _M_AVLNode *n;
    int depth = 0;
    if (!m || !key) return HDQL_AVL_BAD_ARG_ERROR;
    link = &m->root;
    while (*link) {
        int rc;
        if (depth >= HDQL_AVL_MAX_HEIGHT) return HDQL_AVL_MEM_ERROR;
        path[depth++] = link;
        n = *link;
        rc = key_cmp(key, n->key, m->keyLen);
        if (rc == 0)
            break;
        link = rc < 0 ? &n->left : &n->right;
    }
    if (!*link) return HDQL_AVL_OK;  /* not found */
    n = *link;
    ifelse(_M_isMap, `true', `if(oldValue) *oldValue = n->value;')
    if(!n->left) {
        *link = n->right;
        node_delete(n, m->alloc);
        --depth;
    } else if(!n->right) {
        *link = n->left;
        node_delete(n, m->alloc);
        --depth;
    } else {
        _M_AVLNode **succLink;
        _M_AVLNode *succ;
        succLink = &n->right;
        if (!(*succLink)->left) {
            succ = *succLink;
            succ->left = n->left;
            *link = succ;
            node_delete(n, m->alloc);
        } else {
            _M_AVLNode *r = n->right;
            succLink = &r->left;
            while ((*succLink)->left) {
                if (depth >= HDQL_AVL_MAX_HEIGHT)
                    return -2;

                path[depth++] = succLink;
                succLink = &(*succLink)->left;
            }
            succ = *succLink;
            *succLink = succ->right;
            succ->left = n->left;
            succ->right = n->right;
            *link = succ;
            node_delete(n, m->alloc);
        }
    }

    while (depth > 0) {
        _M_AVLNode **plink = path[--depth];
        if (*plink)
            *plink = balance(*plink);
    }
    return HDQL_AVL_CHANGED;
}

#endif  /* HDQL_AVL_RECURSIVE_IMPLEMS */

static int
avl_iter_node(_M_AVLNode *n
        , size_t keyLen
        , int (*callback)(const _M_keyType key, size_t keyLen ifelse(_M_isMap, `true', `, void **value'), void *userdata)
        , void *userdata) {
    int rc;
    if(!n) return 0;
    rc = avl_iter_node(n->left, keyLen, callback, userdata);
    if(rc) return rc;
    rc = callback(n->key, keyLen ifelse(_M_isMap, `true', `, &n->value'), userdata);
    if (rc) return rc;
    return avl_iter_node(n->right, keyLen, callback, userdata);
}

/*
 * Public API
 */

struct _M_AVL *
_M4_pubfunc(_M_AVL, create)(size_t keyLen, const struct hdql_Allocator *alloc) {
    struct _M_AVL *m = alloc->alloc(sizeof(struct _M_AVL), alloc->userdata);
    if(!m) return NULL;
    m->root = NULL;
    m->alloc = alloc;
    m->keyLen = keyLen;
    m->nItems = 0;
    return m;
}

int
_M4_pubfunc(_M_AVL, insert)(struct _M_AVL *m
        , const _M_keyType key
        ifelse(_M_isMap, `true', `, void *value')
        ) {
    int rc =
    #if defined(HDQL_AVL_RECURSIVE_IMPLEMS) && HDQL_AVL_RECURSIVE_IMPLEMS
        insert_recursive(&m->root, m->root, key, m->keyLen ifelse(_M_isMap, `true', `, value'), m->alloc);
    #else
        insert_iterative(m, key ifelse(_M_isMap, `true', `, value'));
    #endif
    if(HDQL_AVL_OK == rc) ++(m->nItems);
    return rc;
}

ifdef(`_M_isMap',dnl
`void *
_M4_pubfunc(_M_AVL, get)(const struct _M_AVL *m, const _M_keyType key) {
    _M_AVLNode *n = m->root;
    while (n) {
        int c = key_cmp(key, n->key, m->keyLen);
        if(c == 0) return n->value;
        n = c < 0 ? n->left : n->right;
    }
    return NULL;
}',dnl
`bool
_M4_pubfunc(_M_AVL, has)(const struct _M_AVL *m, const _M_keyType key) {
    _M_AVLNode *n = m->root;
    while (n) {
        int c = key_cmp(key, n->key, m->keyLen);
        if(c == 0) return true;
        n = c < 0 ? n->left : n->right;
    }
    return false;
}')dnl

int
_M4_pubfunc(_M_AVL, erase)(struct _M_AVL *m, const _M_keyType key ifelse(_M_isMap, `true', `, void **oldValue')) {
    int removed = 0;
    if(!m) return 0;
    #if defined(HDQL_AVL_RECURSIVE_IMPLEMS) && HDQL_AVL_RECURSIVE_IMPLEMS
        m->root = erase_recursive(m->root, key, m->keyLen ifelse(_M_isMap, `true', `, oldValue'), &removed, m->alloc);
    #else
        removed = erase_iterative(m, key ifelse(_M_isMap, `true', `, oldValue'));
    #endif
    if(HDQL_AVL_CHANGED == removed) --(m->nItems);
    return removed;
}

size_t
_M4_pubfunc(_M_AVL, size)(const struct _M_AVL *m) {
    return m->nItems;
}

void
_M4_pubfunc(_M_AVL, destroy)(struct _M_AVL *m) {
    node_free_all(m->root, m->alloc);
    m->root = NULL;
    m->alloc->free((hdql_Datum_t) m, m->alloc->userdata);
}

int
_M4_pubfunc(_M_AVL, iter)(struct _M_AVL *m
        , int (*callback)(const _M_keyType key, size_t keyLen ifelse(_M_isMap, `true', `, void **value'), void *userdata)
        , void *userdata) {
    if (!m || !callback) return HDQL_AVL_BAD_ARG_ERROR;
    return avl_iter_node(m->root, m->keyLen, callback, userdata);
}

