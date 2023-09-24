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

struct hdql_CollectionKey;  /* fwd, query-key.h */

/**\brief Represents a query instance of HDQL
 *
 * A query in HDQL is a stateful object combining functions of data traversing,
 * selection and inspection. It can be examined for a return type before
 * applied to a certain instance.
 * */
struct hdql_Query;

/**\brief Compiles query object from string expression
 *
 * \todo Rename to hdql_query_compile()
 * */
struct hdql_Query *
hdql_compile_query( const char * strexpr
                  , struct hdql_Compound * rootCompound
                  , struct hdql_Context * ctx
                  , char * errBuf, unsigned int errBufLength
                  , int * errDetails
                  );

/**\brief Creates a query object for the given compound type */
struct hdql_Query *
hdql_query_create(
          const struct hdql_AttrDef * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t ctx
        );

const struct hdql_AttrDef *
hdql_query_get_subject( struct hdql_Query * );

/**\brief Prints short one-line string describing query
 *
 * \note Does not follows query chain, prints only the given query details */
int
hdql_query_str( const struct hdql_Query *
              , char * strbuf, size_t buflen
              , hdql_Context_t
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
size_t
hdql_query_depth(struct hdql_Query * current);

/**\brief Returns true if the entire query evaluates to scalar value */
bool
hdql_query_is_fully_scalar(struct hdql_Query * q);

/**\brief Returns attribute definition of topmost query
 *
 * May be used in various context to inspect the data type of what the query is
 * supposed to return actually. */
const struct hdql_AttrDef *
hdql_query_top_attr( const struct hdql_Query * );

/**\brief Retruens query's current attribute */
const struct hdql_AttrDef *
hdql_query_attr(const struct hdql_Query *);

/**\brief Returns collection query's selection arguments */
hdql_SelectionArgs_t
hdql_query_get_collection_selection(struct hdql_Query *);

/**\brief Returns next dereference query item
 *
 * \note return NULL for terminal queries in list */
struct hdql_Query *
hdql_query_next_query(struct hdql_Query *);

/**\brief Retrieves the result using current query, by provided pointer
 *
 * Returns pointer to a queried data (which type can be retrieved by
 * `hdql_query_top_attr()`) or NULL if query evaluated to no result.
 *
 * \note if `keys` set to NULL, no keys will be copied for query.
 * */
hdql_Datum_t
hdql_query_get( struct hdql_Query * query
              , struct hdql_CollectionKey * keys
              , hdql_Context_t ctx
              );

int hdql_query_reset(struct hdql_Query * query, hdql_Datum_t, hdql_Context_t ctx);

void hdql_query_destroy(struct hdql_Query *, hdql_Context_t ctx);

/**\brief Dumps built query internals */
void hdql_query_dump(FILE *, struct hdql_Query *, hdql_Context_t);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* H_HDQL_QUERY_H */
