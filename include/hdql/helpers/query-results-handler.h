#ifndef H_HDQL_QUERY_RESULTS_HANDLER_H
#define H_HDQL_QUERY_RESULTS_HANDLER_H 1

#include "hdql/attr-def.h"
#include "hdql/query-key.h"

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_QueryResultsWorkspace;  /* opaque */

/**\brief Query results receiver assuming tabular logic
 *
 * This helper interface is meant to simplify integration with query object
 * defining a rule to populate table with query results. The query always
 * evaluates to scalar instance, producing a series
 * of ((key1, key22, ...), value) pair every time it is advanced. In the
 * simplest case the value is atomic scalar value (of arithmetic or string
 * type). More elaborated and very important scenario is when the value is
 * of (virtual) compound type to which user would like to apply sub-queries.
 * For instance, populating 2D histogram would require obtaining two random
 * joint variables --- which is the subject of two (possibly, very simple)
 * sub-queries to root query result.
 *
 * */
struct hdql_iQueryResultsHandler {
    /** Arbitrary user pointer forwarded into all interface's calls */
    void * userdata;

    /**\brief Callback to handle attribute definition in query result
     *
     * Interface implementation may benefit from these calls in order to
     * prepare streaming schema.
     *
     * Called in `hdql_query_result_table_init()`. */
    int (*handle_attr_def)(const char *, const struct hdql_AttrDef *, void *);

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
    int (*finalize_schema)(void * userdata);

    int (*handle_record)(hdql_Datum_t, void *);
};

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
