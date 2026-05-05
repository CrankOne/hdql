#ifndef H_HDQL_HELPERS_PRINT_TREE_LIKE_STRUCT_H
#define H_HDQL_HELPERS_PRINT_TREE_LIKE_STRUCT_H 1

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_AttrDef;  /* fwd */
struct hdql_Context;  /* fwd */
struct hdql_Datum;

struct hdql_TreeLikeNode;
typedef const struct hdql_TreeLikeNode * hdql_TreeLikeNode_t;

struct hdql_TreeLikeNodeIFace {
    /* should return number of child nodes */
    size_t (*nchildren)(hdql_TreeLikeNode_t n, void *);
    /* should return n-th child's handle, possibly allocating resource(s) */
    hdql_TreeLikeNode_t (*acquire_child)(hdql_TreeLikeNode_t n, size_t i, void *);
    /* should return 0 if node is not branching */
    int (*is_leaf)(hdql_TreeLikeNode_t n, void *);
    /* should print node label to a buffer */
    const char * (*label)(char * buf, size_t bufSize
            , hdql_TreeLikeNode_t n, void *);
    /* should release child, possibly releasing resources; can be NULL */
    void (*release_child)(hdql_TreeLikeNode_t);
};

void
hdql_print_tree_like( const hdql_TreeLikeNode_t node, void * userdata
              , struct hdql_TreeLikeNodeIFace
              , FILE * outFile, size_t lineWidth);

/**\brief Prints AD as tree-like multiline structure
 *
 * This function can be utilized in debugging routines to print attribute
 * definition (the type). Useful for expanding compound types.
 *
 * If \p recursive is set to non-zero value, compounds names will be recursively
 * expanded till most basic parent (for eponymous 1st takes precedence).
 * */
void
hdql_attr_def_tree_print(const struct hdql_AttrDef * ad
        , struct hdql_Context * ctx
        , size_t lineWidth
        , FILE * dest
        , int recursive
        );

/**\brief Shallow print of the HDQL value, with datum and AD provided
 *
 * One-liner, usually giving insightful information about the type and datum.
 * Does not expand collections and compounds, does not follow forwarding
 * queries.
 * */
void
hdql_print_value_shallow(const struct hdql_Datum * r
        , const struct hdql_AttrDef * topAttrDef
        , struct hdql_Context * ctx
        , char * buf, size_t bufSize
        );

/**\brief Prints AD as tree-like multiline structure, expanding values
 *
 * This function can be utilized in debugging routines to print attribute
 * definition (the type). Useful for expanding compound types and some data
 * the root datum can provide without evaluation of forwarding queries,
 * collections, etc.
 *
 * If \p recursive is set to non-zero value, compounds names will be recursively
 * expanded till most basic parent (for eponymous 1st takes precedence).
 * */
void
hdql_attr_def_tree_data_print(const struct hdql_AttrDef * ad
        , const struct hdql_Datum * datum
        , struct hdql_Context * ctx
        , size_t lineWidth
        , FILE * dest
        , int recursive
        );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_HELPERS_PRINT_TREE_LIKE_STRUCT_H */
