#ifndef H_HDQL_QUERY_RESULTS_HANDLER_CSV_H
#define H_HDQL_QUERY_RESULTS_HANDLER_CSV_H 1

#include "hdql/helpers/query-results-handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Initializes CSV printing `hdql_iQueryResultsHandler` interface
 *        implementation
 *
 * ...
 */
int
hdql_query_results_handler_csv_init( struct hdql_iQueryResultsHandler *
        , FILE * stream
        , const char * valueDelimiter, const char * recordDelimiter
        );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_RESULTS_HANDLER_CSV_H */
