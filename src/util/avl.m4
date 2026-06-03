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
    ifelse( _M_keyDynAlloc, `const',    `size_t keyLen;'
          , _M_keyDynAlloc, `vardc',    `int (*key_cmp)(const void *, const void *);
    size_t (*key_len)(const void *);')
    size_t nItems;
};


static int h(const _M_AVLNode *n) { return n ? n->height : 0; }
static int max_i(int a, int b) { return a > b ? a : b; }
static void fix_height(_M_AVLNode *n) { n->height = 1 + max_i(h(n->left), h(n->right)); }
static int balance_factor(const _M_AVLNode *n) { return h(n->right) - h(n->left); }

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
        ifelse(_M_keyDynAlloc,  `const',    `, size_t keyLen'
              ,_M_keyDynAlloc,  `vardc',    `, size_t (*key_len)(const void *)' )
        ifelse(_M_isMap,        `true',     `, void *value')
        , const struct hdql_Allocator *alloc) {
    _M_AVLNode *n = alloc->alloc(sizeof(*n), alloc->userdata);
    if (!n) return NULL;
ifelse(_M_keyDynAlloc, `const',
`    n->key = alloc->alloc(keyLen, alloc->userdata);
    if(!n->key) {
        alloc->free(n, alloc->userdata);
        return NULL;
    }
    memcpy(n->key, key, keyLen);'
    , _M_keyDynAlloc, `vardc',
`   const size_t keyLen = key_len(key);
    n->key = alloc->alloc(keyLen, alloc->userdata);
    if(!n->key) {
        alloc->free(n, alloc->userdata);
        return NULL;
    }
    memcpy(n->key, key, keyLen);'
    , _M_keyDynAlloc, `none', `n->key = key;')
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
    ifelse( _M_keyDynAlloc, `const', `alloc->free(n->key, alloc->userdata);'
          , _M_keyDynAlloc, `vardc', `alloc->free(n->key, alloc->userdata);'
          )
    alloc->free(n, alloc->userdata);
}

static int
insert_iterative( struct _M_AVL *m
        , const _M_keyType key
        ifelse(_M_isMap, `true', `, void *value')
        ) {
    _M_AVLNode **path[HDQL_AVL_MAX_HEIGHT];
    _M_AVLNode **link;
    _M_AVLNode *n;
    int depth = 0;

    if(!m) return HDQL_AVL_BAD_ARG_ERROR;
    ifelse( _M_keyDynAlloc, `const', `if(!key) return HDQL_AVL_BAD_ARG_ERROR;'
          , _M_keyDynAlloc, `vardc', `if(!key) return HDQL_AVL_BAD_ARG_ERROR;'
          )

    link = &m->root;

    while (*link) {
        int rc;
        if (depth >= HDQL_AVL_MAX_HEIGHT)
            return HDQL_AVL_MEM_ERROR; /* should not happen for a sane AVL tree */
        path[depth++] = link;
        n = *link;
        rc = _M_key_cmp(key, n->key ifelse(_M_keyDynAlloc, `const', `, m->keyLen'));
        if (rc == 0) {
            ifelse(_M_isMap, `true', `n->value = value;')
            return HDQL_AVL_CHANGED; /* replaced */
        }
        link = rc < 0 ? &n->left : &n->right;
    }
    *link = node_new(key
            ifelse( _M_keyDynAlloc, `const', `, m->keyLen'
                  , _M_keyDynAlloc, `vardc', `, m->key_len' )
            ifelse(_M_isMap, `true', `, value'), m->alloc);
    if (!*link)
        return HDQL_AVL_MEM_ERROR;

    /* including newly inserted leaf in the path is not necessary --
     * it already has height = 1. Rebalance ancestors from bottom
     * to top */
    while (depth > 0) {
        _M_AVLNode **plink = path[--depth];
        *plink = balance(*plink);
    }

    return HDQL_AVL_OK; /* inserted */
}

static void
node_delete(_M_AVLNode *n, const struct hdql_Allocator *alloc) {
    if(!n) return;
    ifelse( _M_keyDynAlloc, `const', `alloc->free(n->key, alloc->userdata);'
          , _M_keyDynAlloc, `vardc', `alloc->free(n->key, alloc->userdata);')
    alloc->free(n, alloc->userdata);
}

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
        rc = _M_key_cmp(key, n->key, m->keyLen);
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

/*
 * Public API
 */

struct _M_AVL *
_M4_pubfunc(_M_AVL, create)(
        ifelse( _M_keyDynAlloc, `const', `size_t keyLen, '
              , _M_keyDynAlloc, `vardc', `int (*key_cmp)(const void *, const void *), size_t (*key_len)(const void *), '
              )
        const struct hdql_Allocator *alloc) {
    struct _M_AVL *m = alloc->alloc(sizeof(struct _M_AVL), alloc->userdata);
    if(!m) return NULL;
    m->root = NULL;
    m->alloc = alloc;
    ifelse( _M_keyDynAlloc, `const', `m->keyLen = keyLen;'
          , _M_keyDynAlloc, `vardc', `m->key_cmp = key_cmp; m->key_len = key_len;'
          )
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
        insert_recursive(&m->root, m->root, key
        ifelse(_M_keyDynAlloc, `const', `, m->keyLen')
        ifelse(_M_isMap, `true', `, value'), m->alloc);
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
        int c = _M_key_cmp(key, n->key, m->keyLen);
        if(c == 0) return n->value;
        n = c < 0 ? n->left : n->right;
    }
    return NULL;
}',dnl
`bool
_M4_pubfunc(_M_AVL, has)(const struct _M_AVL *m, const _M_keyType key) {
    _M_AVLNode *n = m->root;
    while (n) {
        int c = _M_key_cmp(key, n->key, m->keyLen);
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
        m->root = erase_recursive(m->root, key
                ifelse(_M_keyDynAlloc, `const', `, m->keyLen')
                ifelse(_M_isMap, `true', `, oldValue'), &removed, m->alloc);
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
        , int (*callback)(const _M_keyType key
                ifelse(_M_keyDynAlloc, `const', `, size_t keyLen')
                ifelse(_M_isMap, `true', `, void **value'), void *userdata)
        , void *userdata) {
    _M_AVLNode *stack[HDQL_AVL_MAX_HEIGHT];
    _M_AVLNode *n;
    int top = 0;
    if(!m || !callback) return HDQL_AVL_BAD_ARG_ERROR;
    n = m->root;
    while(n || top > 0) {
        while(n) {
            if(top >= HDQL_AVL_MAX_HEIGHT) return HDQL_AVL_MEM_ERROR;
            stack[top++] = n;
            n = n->left;
        }
        n = stack[--top];
        {
            int rc = callback(n->key ifelse(_M_keyDynAlloc, `const', `, m->keyLen') ifelse(_M_isMap, `true', `, &n->value'), userdata);
            if (rc) return rc;
        }
        n = n->right;
    }
    return 0;
}

int
_M4_pubfunc(_M_AVL, iter_r)(struct _M_AVL *m
        , int (*callback)(const _M_keyType key
                ifelse(_M_keyDynAlloc, `const', `, size_t keyLen')
                ifelse(_M_isMap, `true', `, void **value'), void *userdata)
        , void *userdata) {
    _M_AVLNode *stack[HDQL_AVL_MAX_HEIGHT];
    _M_AVLNode *n;
    int top = 0;
    if (!m || !callback) return HDQL_AVL_BAD_ARG_ERROR;
    n = m->root;
    while (n || top > 0) {
        while (n) {
            if (top >= HDQL_AVL_MAX_HEIGHT) return HDQL_AVL_MEM_ERROR;
            stack[top++] = n;
            n = n->right;
        }
        n = stack[--top];
        {
            int rc = callback(n->key ifelse(_M_keyDynAlloc, `const', `, m->keyLen') ifelse(_M_isMap, `true', `, &n->value'), userdata);
            if (rc) return rc;
        }
        n = n->left;
    }
    return 0;
}
