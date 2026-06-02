#include "hdql/util/avl-tree.h"
#include "hdql/types.h"
#include "hdql/util/allocator.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* max depth of the AVL tree for non-recursive implementations */
#ifndef AVL_MAX_HEIGHT
#   define AVL_MAX_HEIGHT 128
#endif

typedef struct AVLNode AVLNode;

struct AVLNode {
    AVLNode *left
          , *right;
    int height;
    void *value;
    unsigned char *key;
};

struct hdql_avl {
    AVLNode *root;
    const struct hdql_Allocator *alloc;
    size_t keyLen;
    size_t nItems;
};


static int h(const AVLNode *n) { return n ? n->height : 0; }
static int max_i(int a, int b) { return a > b ? a : b; }
static void fix_height(AVLNode *n) { n->height = 1 + max_i(h(n->left), h(n->right)); }
static int balance_factor(const AVLNode *n) { return h(n->right) - h(n->left); }
/* unsigned big-endian integer comparison */
static int key_cmp(const unsigned char *a, const unsigned char *b, size_t n) { return memcmp(a, b, n); }


static AVLNode *
rot_left(AVLNode *a) {
    AVLNode *b = a->right;
    a->right = b->left;
    b->left = a;

    fix_height(a);
    fix_height(b);
    return b;
}

static AVLNode *
rot_right(AVLNode *a) {
    AVLNode *b = a->left;
    a->left = b->right;
    b->right = a;

    fix_height(a);
    fix_height(b);
    return b;
}

static AVLNode *balance(AVLNode *n) {
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

static AVLNode *
node_new(const void *key, size_t keyLen, void *value, const struct hdql_Allocator *alloc) {
    AVLNode *n = alloc->alloc(sizeof(*n), alloc->userdata);
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
    n->value = value;
    return n;
}

static void node_free_all(AVLNode *n, const struct hdql_Allocator *alloc) {
    if (!n) return;
    node_free_all(n->left, alloc);
    node_free_all(n->right, alloc);
    alloc->free(n->key, alloc->userdata);
    alloc->free(n, alloc->userdata);
}

static int
insert_recursive( AVLNode **out
                , AVLNode *n
                , const void *key
                , size_t keyLen
                , void *value
                , const struct hdql_Allocator *alloc
                ) {
    if (!n) {
        *out = node_new(key, keyLen, value, alloc);
        return *out ? HDQL_AVL_OK : HDQL_AVL_MEM_ERROR;
    }
    int c = key_cmp(key, n->key, keyLen);
    if (c == 0) {
        n->value = value;
        *out = n;
        return HDQL_AVL_CHANGED;
    }
    int rc;
    if (c < 0)
        rc = insert_recursive(&n->left, n->left, key, keyLen, value, alloc);
    else
        rc = insert_recursive(&n->right, n->right, key, keyLen, value, alloc);
    if (rc < 0) {
        *out = n;
        return rc;
    }

    *out = balance(n);
    return rc;
}

static int
insert_iterative(struct hdql_avl *m, const unsigned char *key, void *value) {
    AVLNode **path[AVL_MAX_HEIGHT];
    AVLNode **link;
    AVLNode *n;
    int depth = 0;

    if (!m || !key)
        return -1;

    link = &m->root;

    while (*link) {
        int rc;

        if (depth >= AVL_MAX_HEIGHT)
            return -2; /* should not happen for a sane AVL tree */

        path[depth++] = link;

        n = *link;
        rc = key_cmp(key, n->key, m->keyLen);

        if (rc == 0) {
            n->value = value;
            return 1; /* replaced */
        }

        link = rc < 0 ? &n->left : &n->right;
    }

    *link = node_new(key, m->keyLen, value, m->alloc);
    if (!*link)
        return -1;

    /*
     * Also include the newly inserted leaf in the path? Not necessary:
     * it already has correct height = 1.
     *
     * Rebalance ancestors from bottom to top.
     */
    while (depth > 0) {
        AVLNode **plink = path[--depth];
        *plink = balance(*plink);
    }

    return 0; /* inserted */
}

static AVLNode *
detach_min(AVLNode *n, AVLNode **min_node) {
    if (!n->left) {
        *min_node = n;
        return n->right;
    }
    n->left = detach_min(n->left, min_node);
    return balance(n);
}

static void
node_delete(AVLNode *n, const struct hdql_Allocator *alloc) {
    if(!n) return;
    alloc->free(n->key, alloc->userdata);
    alloc->free(n, alloc->userdata);
}

static AVLNode *
erase_recursive(AVLNode *n
        , const unsigned char *key
        , size_t keyLen
        , void **oldVal
        , int *removed
        , const struct hdql_Allocator *alloc
        ) {
    int rc;
    if(!n) return NULL;
    rc = key_cmp(key, n->key, keyLen);
    if (rc < 0) {
        n->left = erase_recursive(n->left, key, keyLen, oldVal, removed, alloc);
        return balance(n);
    }

    if (rc > 0) {
        n->right = erase_recursive(n->right, key, keyLen, oldVal, removed, alloc);
        return balance(n);
    }

    {
        AVLNode *left = n->left;
        AVLNode *right = n->right;
        *removed = HDQL_AVL_CHANGED;
        if (oldVal) *oldVal = n->value;
        if (!right) {
            node_delete(n, alloc);
            return left;
        }

        {
            AVLNode *min = NULL;
            right = detach_min(right, &min);
            min->left = left;
            min->right = right;
            node_delete(n, alloc);
            return balance(min);
        }
    }
}

int
erase_iterative(struct hdql_avl *m, const unsigned char *key, void **old_value) {
    AVLNode **path[AVL_MAX_HEIGHT];
    AVLNode **link;
    AVLNode *n;
    int depth = 0;

    if (!m || !key)
        return 0;

    link = &m->root;

    while (*link) {
        int rc;

        if (depth >= AVL_MAX_HEIGHT)
            return -2;

        path[depth++] = link;

        n = *link;
        rc = key_cmp(key, n->key, m->keyLen);

        if (rc == 0)
            break;

        link = rc < 0 ? &n->left : &n->right;
    }

    if (!*link)
        return 0; /* not found */

    n = *link;

    if (old_value)
        *old_value = n->value;

    if (!n->left) {
        *link = n->right;
        node_delete(n, m->alloc);
        --depth;
    } else if (!n->right) {
        *link = n->left;
        node_delete(n, m->alloc);
        --depth;
    } else {
        AVLNode **succ_link;
        AVLNode *succ;

        /* Find leftmost node in right subtree. It will replace n physically. */
        succ_link = &n->right;

        while ((*succ_link)->left) {
            if (depth >= AVL_MAX_HEIGHT)
                return -2;

            path[depth++] = succ_link;
            succ_link = &(*succ_link)->left;
        }

        succ = *succ_link;

        /* Detach successor from its old place.  Successor has no left child. */
        *succ_link = succ->right;

        /* Replace erased node by successor. */
        succ->left = n->left;

        if (succ != n->right)
            succ->right = n->right;

        *link = succ;

        node_delete(n, m->alloc);
        /* link already points to the replacement subtree root.
         * Keep it in the path so it is rebalanced too. */
    }

    while (depth > 0) {
        AVLNode **plink = path[--depth];

        if (*plink)
            *plink = balance(*plink);
    }

    return 1;
}

static int
avl_iter_node(AVLNode *n
        , size_t keyLen
        , int (*callback)(const unsigned char * key, size_t keyLen, void ** value, void * userdata)
        , void *userdata) {
    int rc;
    if(!n) return 0;
    rc = avl_iter_node(n->left, keyLen, callback, userdata);
    if(rc) return rc;
    rc = callback(n->key, keyLen, &n->value, userdata);
    if (rc) return rc;
    return avl_iter_node(n->right, keyLen, callback, userdata);
}

/*
 * Public API
 */

struct hdql_avl *
hdql_avl_new(size_t keyLen, const struct hdql_Allocator *alloc) {
    struct hdql_avl * m = alloc->alloc(sizeof(struct hdql_avl), alloc->userdata);
    if(!m) return NULL;
    m->root = NULL;
    m->alloc = alloc;
    m->keyLen = keyLen;
    m->nItems = 0;
    return m;
}

int
hdql_avl_insert(struct hdql_avl *m, const void *key, void *value) {
    int rc = insert_recursive(&m->root, m->root, key, m->keyLen, value, m->alloc);
    if(HDQL_AVL_OK == rc) ++(m->nItems);
    return rc;
}

void *
hdql_avl_get(const struct hdql_avl *m, const void *key) {
    AVLNode *n = m->root;
    while (n) {
        int c = key_cmp(key, n->key, m->keyLen);
        if(c == 0) return n->value;
        n = c < 0 ? n->left : n->right;
    }
    return NULL;
}

int
hdql_avl_erase(struct hdql_avl *m, const void *key, void **oldVal) {
    int removed = 0;
    if(!m) return 0;
    m->root = erase_recursive(m->root, key, m->keyLen, oldVal, &removed, m->alloc);
    if(HDQL_AVL_CHANGED == removed) --(m->nItems);
    return removed;
}

size_t
hdql_avl_size(const struct hdql_avl *m) {
    return m->nItems;
}

void
hdql_avl_destroy(struct hdql_avl *m) {
    node_free_all(m->root, m->alloc);
    m->root = NULL;
    m->alloc->free((hdql_Datum_t) m, m->alloc->userdata);
}

int
hdql_avl_iter(struct hdql_avl *m
        , int (*callback)(const unsigned char * key, size_t keyLen, void ** value, void * userdata)
        , void *userdata) {
    if (!m || !callback) return HDQL_AVL_BAD_ARG_ERROR;
    return avl_iter_node(m->root, m->keyLen, callback, userdata);
}

