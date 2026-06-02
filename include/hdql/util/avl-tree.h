#ifndef H_HDQL_AVL_TREE_H
#define H_HDQL_AVL_TREE_H 1

/**\file
 * \brief AVL tree implementation for fixed-length keys
 *
 * \todo Non-recursive implementations to support large trees?
 * \todo Variadic key length to support string indeces?
 */

#include "hdql/types.h"

#include "hdql/util/allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HDQL_AVL_OK 0
#define HDQL_AVL_CHANGED 1
#define HDQL_AVL_MEM_ERROR -1
#define HDQL_AVL_BAD_ARG_ERROR -2

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

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_AVL_TREE_H */
