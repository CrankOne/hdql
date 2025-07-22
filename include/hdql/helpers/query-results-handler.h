#ifndef H_HDQL_QUERY_RESULTS_HANDLER_H
#define H_HDQL_QUERY_RESULTS_HANDLER_H 1

#include "hdql/attr-def.h"
#include "hdql/query-key.h"

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_QueryResultsWorkspace;  /* opaque */

/**\brief Query results receiver interface
 *
 * This helper interface is meant to simplify integration with query object
 * defining a rule to populate tables and objects with query results.
 *
 * The query always evaluates to scalar instance, producing a series
 * of ((key1, key22, ...), value) pair every time it is advanced. In the
 * simplest case the value is atomic scalar value (of arithmetic or string
 * type). More elaborated and very important scenario is when the value is
 * of (virtual) compound type to which user would like to apply sub-queries.
 * Handling code might also be interested in handling collections in some way.
 *
 * For instance, populating 2D histogram would require obtaining two random
 * joint variables -- which is the subject of two (possibly, very simple)
 * sub-queries to root query result.
 *
 * This interface and related functions provide a helepr API with some
 * boilerplate implementation to handle query results, possibly recursively.
 * */
struct hdql_iQueryResultsHandler {
    /** Arbitrary user pointer forwarded into all interface's calls */
    void * userdata;

    void * (*init_atomic_ad)(const struct hdql_AttrDef *, const struct hdql_ValueInterface *, void * rh);
    void * (*init_compound_ad)(const struct hdql_AttrDef *, const struct hdql_Compound * c, void * rh);
    void * (*init_collection_ad)(const struct hdql_AttrDef *, void * rh);

    int (*handle_atomic)(hdql_Datum_t, void * ah, void * rh);
    int (*handle_compound)(hdql_Datum_t, void * ah, void * rh);
    int (*handle_collection)(hdql_Datum_t, void * ah, void * rh);

    /**\brief Callback to handle keys definition
     *
     * Called in `hdql_query_result_table_init()`.
     * */
    int (*handle_keys)(struct hdql_CollectionKey * keys
            , struct hdql_KeyView * flatKeyViews
            , size_t nKeys
            , void *
            );

    /**\brief Optional callback finalizing definition of keys and attributes
     *
     * May be used by implementing object to finalize caches related to
     * attributes and keys definition.
     *
     * Called in `hdql_query_result_table_init()` when not null. */
    int (*finalize_schema)(void * rh);

    int (*handle_record)(hdql_Datum_t, void *);
};

/**\brief Initializes handler based on the query object and given interface
 *
 * 1. Retrieves top attribute definition (returned type), then depending on
 *    the query result:
 *      - if `handle_atomic_scalar()` is defined by iface and query result type
 *        is atomic, allocates scalar handle
 *      - if `handle_compound()` is defined by iface and query result type is
 *        compound, allocates compound handle
 * 2. If `handle_key_views()` is set, alloates keys for query and their flat
 *    views, forwards call to `handle_keys()`
 * 3. Calls `finalize_schema()` if defined.
 * */
struct hdql_QueryResultsWorkspace *
hdql_query_results_init(
          struct hdql_Query * q
        , const char ** attrs
        , struct hdql_iQueryResultsHandler * iqr
        , struct hdql_Context * ctx
        );

int
hdql_query_results_process_records_from( struct hdql_Datum * d
        , struct hdql_QueryResultsWorkspace * ws );

int hdql_query_results_destroy( struct hdql_QueryResultsWorkspace * );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_RESULTS_HANDLER_H */
