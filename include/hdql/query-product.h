#ifndef H_HDQL_QUERY_PRODUCT_H
#define H_HDQL_QUERY_PRODUCT_H

#include "hdql/query.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Re-sets a set of queries built on top of the same root object
 *
 * Used to initialize ground state and get first item in a cartesian product
 * of a set of queries. \p keys can be NULL. \p values, \p qs (and \p keys, if
 * not NULL) must be pre-allocated arrays of same size.
 *
 * Forwards error code if `hdql_query_reset()` from any query returned code
 * other than `HDQL_ERR_CODE_OK`.
 *
 * \returns `HDQL_ERR_CODE_OK` on success
 * \returns `HDQL_ERR_EMPTY_SET` if at least one query does not yield a value
 * */
int
hdql_query_product_reset( struct hdql_Query ** qs
        , struct hdql_CollectionKey ** keys
        , hdql_Datum_t * values
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t ctx
        );

/**\brief Obtains next value in a Cartesian product of queries.
 *
 * Advances Cartesian product state of set of queries built from same root
 * object.
 *
 * Forwards error code if `hdql_query_reset()` from any query returned code
 * other than `HDQL_ERR_CODE_OK`.
 *
 * \returns `HDQL_ERR_CODE_OK` on success
 * \returns `HDQL_ERR_EMPTY_SET` when query product exhausted.
 * */
int
hdql_query_product_advance( struct hdql_Query ** qs
        , struct hdql_CollectionKey ** keys
        , hdql_Datum_t * values
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t ctx
        );

#if 0
/**\brief Keeps state of cartesian product of queries, opaque type */
struct hdql_QueryProduct;

/**\brief Returns number of queries in product */
size_t hdql_query_product_n_queries(const struct hdql_QueryProduct *);

/**\bried Creates new cartesian product instance using null-terminated
 *        set of queries
 *
 * Provided `values` pointer will be allocated to contain array of query
 * results. Created object does not own provided queries, but stores pointers
 * to them, so user code must keep query instances at least till the product
 * lifecycle is done, or retrieve copied pointers useing
 * `hdql_query_product_get_query()`.
 * */
struct hdql_QueryProduct *
hdql_query_product_create(struct hdql_Query **
        , hdql_Datum_t ** values
        , hdql_Context_t ctx
        );

/**\brief Advances cartesian product of the queries returning whether or not
 *        a valid product set was yielded by advance */
bool
hdql_query_product_advance( hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context );

/**\brief Re-sets cartesian product */
bool
hdql_query_product_reset( hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context );

/**\brief Reserves keys for cartesian product */
struct hdql_CollectionKey **
hdql_query_product_reserve_keys( struct hdql_Query ** qs
        , hdql_Context_t context );

/**\brief Returns pointer to collection of queries used in product 
 *
 * Usually used for cleanup operations */
struct hdql_Query **
hdql_query_product_get_query(struct hdql_QueryProduct * qp);

/**\brief Destroys cartesian query product
 *
 * \note `values` provided to `hdql_query_product_create()` will be freed as well */
void
hdql_query_product_destroy(struct hdql_QueryProduct *qp
        , hdql_Context_t context );
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_PRODUCT_H */
