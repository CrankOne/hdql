#include "hdql/attr-def.h"
#include "hdql/query-key.h"
#ifndef H_HDQL_QUERY_RESULTS_TABLE_H
#define H_HDQL_QUERY_RESULTS_TABLE_H 1

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

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

struct hdql_iQueryResultsTable {
    /** Arbitrary user pointer forwarded into all interface's calls */
    void * userdata;

    int (*handle_column_definition)(const char *, const struct hdql_AttrDef *, void *);
    int (*columns_defined)(void * userdata);

    int (*handle_new_record)(struct hdql_KeyView * kv, void *);
    int (*handle_record_value)(hdql_Datum_t datum, void *);
};



#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_RESULTS_TABLE_H */

#if 0
#ifndef H_HDQL_RESULTS_STREAMER_H
#define H_HDQL_RESULTS_STREAMER_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_Context;  /* fwd */
struct hdql_CollectionKey;  /* fwd */
struct hdql_Query;  /* fwd */

/**\brief Tabular results abstraction
 *
 * Generally, lifecycle implies following stages, most of which are optional:
 *  1. Init table with columns. One may request full list of columns provided
 *     by query result with `hdql_stream_column_names()`, optionally re-order
 *     and pick ones of interest and set them with `set_columns()`. Note, that
 *     `set_columns()` can be null (for strict contexts when very specific
 *     result is expected and columns are known).
 *  2. Init table with (flatten) key views, by calling `set_key_view()`. Note,
 *     that this pointer can also be null for usage contexts where keys are
 *     well-defined or not important.
 *  3. Provide instance of this struct to `hdql_dump_query_r()` function, where
 *      3.1) Prior to every result record retrieval, a `new_record()` is called
 *           with flatten keys
 *      3.2) Every value in a row will be provided with `add_value()` call
 *      3.3) `finalize_row()` will be called if not null.
 *
 * Order of stages 1. and 2. is not guaranteed (2 may come before 1).
 * Thus, minimal implementation of this interface must implement only
 * `add_value()`.
 * */
typedef struct iQueryResultsTable {
    void * userdata;
    void (*set_columns)(void * userdata, const char ** colNames, size_t n_columns);
    void (*set_keys)(void * userdata);  // TODO: flat key view or keys?
    void (*add_value)(void * userdata, const char * value);
    void (*finalize_row)(void * userdata);
} iQueryResultsWriter;

/**\brief Collects column names for table
 *
 * For queries with results that can be present in a form of a table, collects
 * column names. Compound value names are concatenated with given delimiter. */
int
hdql_stream_column_names( struct hdql_Query * q
            , char ** names
            , size_t * nCols
            , char delimiter
            );

#if 0
int
hdql_stream_query_results( struct hdql_Query * q
                         , struct hdql_CollectionKey * keys
                         , struct hdql_Context * ctx
                         , const struct hdql_Types * valTypes
                         , iQueryResultsWriter * writer
                         );
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /*H_HDQL_RESULTS_STREAMER_H*/
#endif
