#ifndef H_HDQL_QUERY_RESULTS_HANDLER_CSV_H
#define H_HDQL_QUERY_RESULTS_HANDLER_CSV_H 1

#include "hdql/helpers/query-results-handler.h"
#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_DSVFormatting {
    const char * valueDelimiter  /**< value in a record delimiter (`,` for CSV) */
       , * recordDelimiter  /**< record delimiter (newline for CSV) */
       , * attrDelimiter  /**< attribute delimiter, used in nested column names (like `attr.subattr`) */
       , * collectionLengthMarker  /**< prefix for number of items in collections */
       , * anonymousColumnName  /**< column name placeholder for anonymous columns */
       , * nullToken  /** token to print for null */
       , * unlabeledKeyColumnFormat  /**< Column fmt for unlabeled key, include `%zu` for order number */
       ;
};

/**\brief Initializes CSV printing `hdql_iQueryResultsHandler` interface
 *        implementation
 *
 * ...
 */
int
hdql_query_results_handler_csv_init( struct hdql_iQueryResultsHandler *
        , FILE * stream
        , const struct hdql_DSVFormatting * formatting
        , struct hdql_Context * ctx
        );

void hdql_query_results_handler_csv_cleanup(struct hdql_iQueryResultsHandler *);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_RESULTS_HANDLER_CSV_H */
