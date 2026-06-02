#ifndef H_HDQL_AVL_TREE_H
#define H_HDQL_AVL_TREE_H 1

/**\file
 * \brief Library-wide implementation for sorted map and set utility containers.
 *
 * The header declares API for family of sorted map and set implementations,
 * depending on the key type.
 *
 * The types are defined as `struct hdql_<key><set|map>', the API functions are
 * defined as hdql_<key><set|map>_<new|insert|has|get|erase|size|destroy|iter>(...)
 *
 *
 * Regarding key type, the cases provided by implementation:
 *  - signed long integer key (l)
 *  - unsigned long integer key (u)
 *  - double-precision floating point key (d)
 *  - fixed-length byte keys (f)
 *  - arbitrary length key implementation (v)
 *
 * For instance: `hdql_smap_insert()`, `hdql_vset_erase_()`, etc.
 *
 * \todo lower bound, upper bound.
 */

#include "hdql/types.h"

#include "hdql/util/allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HDQL_AVL_BAD_ARG_ERROR -2
#define HDQL_AVL_MEM_ERROR -1
#define HDQL_AVL_OK 0
#define HDQL_AVL_CHANGED 1

/*
 * unsigned long integer key
 * */

struct hdql_umap;
struct hdql_uset;

HDQL_API struct hdql_umap *hdql_umap_create(const struct hdql_Allocator *alloc);
HDQL_API struct hdql_uset *hdql_uset_create(const struct hdql_Allocator *alloc);

HDQL_API int hdql_umap_insert(struct hdql_umap *m, unsigned long key, void *);
HDQL_API int hdql_uset_insert(struct hdql_uset *m, unsigned long key);

HDQL_API void *hdql_umap_get(const struct hdql_umap *m, unsigned long key);
HDQL_API bool  hdql_uset_has(const struct hdql_uset *m, unsigned long key);

HDQL_API int hdql_umap_erase(struct hdql_umap *m, unsigned long key, void **oldVal);
HDQL_API int hdql_uset_erase(struct hdql_uset *m, unsigned long key);

HDQL_API size_t hdql_umap_size(const struct hdql_umap *m);
HDQL_API size_t hdql_uset_size(const struct hdql_uset *m);

HDQL_API void hdql_umap_destroy(struct hdql_umap *m);
HDQL_API void hdql_uset_destroy(struct hdql_uset *m);

HDQL_API int hdql_umap_iter(struct hdql_umap *m
        , int (*callback)(unsigned long key, size_t keyLen, void **value, void *userdata)
        , void *userdata);

HDQL_API int hdql_uset_iter(struct hdql_uset *m
        , int (*callback)(unsigned long key, size_t keyLen, void *userdata)
        , void *userdata);

/*
 * fixed-size binary key
 * */

struct hdql_fmap;
struct hdql_fset;

HDQL_API struct hdql_fmap *hdql_fmap_create(size_t keyLen, const struct hdql_Allocator *alloc);
HDQL_API struct hdql_fset *hdql_fset_create(size_t keyLen, const struct hdql_Allocator *alloc);

HDQL_API int hdql_fmap_insert(struct hdql_fmap *m, const void *key, void *);
HDQL_API int hdql_fset_insert(struct hdql_fset *m, const void *key);

HDQL_API void *hdql_fmap_get(const struct hdql_fmap *m, const void *key);
HDQL_API bool  hdql_fset_has(const struct hdql_fset *m, const void *key);

HDQL_API int hdql_fmap_erase(struct hdql_fmap *m, const void *key, void **oldVal);
HDQL_API int hdql_fset_erase(struct hdql_fset *m, const void *key);

HDQL_API size_t hdql_fmap_size(const struct hdql_fmap *m);
HDQL_API size_t hdql_fset_size(const struct hdql_fset *m);

HDQL_API void hdql_fmap_destroy(struct hdql_fmap *m);
HDQL_API void hdql_fset_destroy(struct hdql_fset *m);

HDQL_API int hdql_fmap_iter(struct hdql_fmap *m
        , int (*callback)(const unsigned char * key, size_t keyLen, void **value, void *userdata)
        , void *userdata);

HDQL_API int hdql_fset_iter(struct hdql_fset *m
        , int (*callback)(const unsigned char * key, size_t keyLen, void *userdata)
        , void *userdata);

#if 0
/*\brief Opaque type of binary search tree for keys of fixed length */
struct hdql_avl;

/**\brief Creates new AVL tree instance */
HDQL_API struct hdql_avl *hdql_avl_new(size_t keyLen, const struct hdql_Allocator *);

/**\brief Adds new item to the AVL tree
 *
 * \returns
 * - HDQL_AVL_OK on success
 * - HDQL_AVL_CHANGED if element was overwritten
 * - HDQL_AVL_MEM_ERROR on allocation failure
 * */
HDQL_API int hdql_avl_insert(struct hdql_avl *m, const void *key, void *value);

/**\brief Returns element by key or NULL */
HDQL_API void *hdql_avl_get(const struct hdql_avl *m, const void *key);

/**\brief Removes element from AVL tree
 *
 * \returns
 * - HDQL_AVL_OK if no element found
 * - HDQL_AVL_CHANGED if element was erased
 * */
HDQL_API int hdql_avl_erase(struct hdql_avl *m, const void *key, void **old_value);

/**\brief Returns size (number of nodes) of the AVL tree*/
HDQL_API size_t hdql_avl_size(const struct hdql_avl *m);

/**\brief Destroys AVL tree object */
HDQL_API void hdql_avl_destroy(struct hdql_avl *m);

/**\brief Iterates tree with callback
 *
 * \returns Any non-zero code returned by \p callback or zero once done. */
HDQL_API int hdql_avl_iter(struct hdql_avl *m
        , int (*callback)(const unsigned char * key, size_t keyLen, void ** value, void * userdata)
        , void *userdata);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_AVL_TREE_H */
