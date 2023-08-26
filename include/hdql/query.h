#ifndef H_HDQL_QUERY_H
#define H_HDQL_QUERY_H

#include "hdql/compound.h"
#include "hdql/types.h"
#include "hdql/context.h"
#include "hdql/value.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_CollectionKey {
    /**\brief Collection key type
     *
     * HDQL manages key instance lifecycle, so key must be one of the
     * registered types.
     *
     * Zero code is reserved for empty key placeholders and keys defined
     * by argument queries. */
    hdql_ValueTypeCode_t code:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
    /**\brief Ptr to actual key value */
    hdql_Datum_t datum;
};

/**\brief Represents a query instance of HDQL
 *
 * A query in HDQL is a stateful object combining functions of data traversing,
 * selection and inspection. It can be examined for a return type before
 * applied to a certain instance.
 * */
struct hdql_Query;

/**\brief Compiles query object from string expression */
struct hdql_Query *
hdql_compile_query( const char * strexpr
                  , struct hdql_Compound * rootCompound
                  , struct hdql_Context * ctx
                  , char * errBuf
                  , unsigned int errBufLength
                  , int * errDetails
                  );

/**\brief Creates a query object for the given compound type */
struct hdql_Query *
hdql_query_create(
          const struct hdql_AttributeDefinition * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t ctx
        );

/**\brief Queries within a query result
 *
 * Concatenates query taking first argument as a parent (queried table) and
 * second as a child (sub-query). Resulting query resolves parent in a child. */
struct hdql_Query *
hdql_query_append( 
          struct hdql_Query * current
        , struct hdql_Query * next
        );

/**\brief Returns depth of hdql query */
size_t hdql_query_depth(struct hdql_Query * current);

/**\brief Returns true if the entire query evaluates to scalar value */
bool hdql_query_is_fully_scalar(struct hdql_Query * q);

/**\brief Returns attribute definition of topmost query
 *
 * May be used in various context to inspect the data type of what the query is
 * supposed to return actually. */
const struct hdql_AttributeDefinition * hdql_query_top_attr( const struct hdql_Query * );

/**\brief Retruens query's current attribute */
const struct hdql_AttributeDefinition * hdql_query_attr(const struct hdql_Query *);

/**\brief Reserves keys for the query
 *
 * \note if `keys` refer to a non-NULL value, its content will be used to deply
 *       keys, otherwise it will be allocated
 * */
int
hdql_query_reserve_keys_for( struct hdql_Query * query
                           , struct hdql_CollectionKey ** keys
                           , hdql_Context_t ctx );

int
hdql_query_copy_keys( struct hdql_Query * query
                    , struct hdql_CollectionKey * dest
                    , const struct hdql_CollectionKey * src
                    , hdql_Context_t ctx
                    );

typedef void (*hdql_KeyIterationCallback_t)(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey *
        , hdql_Context_t
        , void *
        , size_t queryLevel
        , size_t queryNoInLevel
        );

int
hdql_query_for_every_key( const struct hdql_Query * query
                        , const struct hdql_CollectionKey *
                        , hdql_Context_t ctx
                        , hdql_KeyIterationCallback_t callback
                        , void * userdata
                        );

int
hdql_query_destroy_keys_for( struct hdql_Query * query
                           , struct hdql_CollectionKey * keys
                           , hdql_Context_t ctx );

/**\brief Retrieves the result using current query, by provided pointer
 *
 * Returns pointer to a queried data (which type can be retrieved by
 * `hdql_query_top_attr()`) or NULL if query evaluated to no result.
 *
 * \note if `keys` set to NULL, no keys will be copied for query.
 * */
hdql_Datum_t
hdql_query_get( struct hdql_Query * query
              , hdql_Datum_t root
              , struct hdql_CollectionKey * keys
              , hdql_Context_t ctx
              );

void hdql_query_reset(struct hdql_Query * query, hdql_Datum_t, hdql_Context_t ctx);

void hdql_query_destroy(struct hdql_Query *, hdql_Context_t ctx);

/**\brief Dumps built query internals */
void hdql_query_dump(FILE *, struct hdql_Query *, hdql_Context_t ctx);

/*
 * Cartesian product of queries
 */

/**\brief Keeps state of cartesian product of queries */
struct hdql_QueryProduct;

/**\bried Creates new cartesian product instance using null-terminated
 *        set of queries */
struct hdql_QueryProduct * hdql_query_product_create(struct hdql_Query **, hdql_Context_t);

/**\brief Advances cartesian product of the queries */
bool hdql_query_cartesian_product_advance( hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context );

/**\brief Returns list of query results for current cartesian product query */
int hdql_query_cartesian_product_get(struct hdql_QueryProduct *qp, hdql_Datum_t * result);

/**\brief Destroys cartesian query product */
void hdql_query_cartesian_product_destroy(struct hdql_QueryProduct *qp, hdql_Context_t context);

/*
 * Flat key view
 */

struct hdql_KeyPrintParams {
    char * strBuf;
    size_t strBufLen;
};

void
hdql_query_print_key_to_buf(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey * key
        , hdql_Context_t ctx
        , void * userdata
        , size_t queryLevel
        , size_t queryNoInLevel
        );


size_t
hdql_keys_flat_view_size( const struct hdql_Query * q
                        , const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        );

struct hdql_KeyView {
    hdql_ValueTypeCode_t               code;
    const struct hdql_CollectionKey  * keyPtr;
    const struct hdql_ValueInterface * interface;
};

int
hdql_keys_flat_view_update( const struct hdql_Query * q
                          , const struct hdql_CollectionKey * keys
                          , struct hdql_KeyView * dest
                          , hdql_Context_t ctx );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* H_HDQL_QUERY_H */
