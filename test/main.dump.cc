#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/helpers/compounds.hh"
#include "hdql/helpers/query-results-handler-csv.h"
#include "hdql/helpers/fancy-print-err.h"
#include "hdql/helpers/query-results-handler.h"
#include "samples.hh"

#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/operations.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/function.h"

#include "events-struct.hh"

#include <cstdlib>
#include <cstdio>
#include <cassert>

//
// Example of event structs

static int
test_query_on_data( int nSample, const char * expression ) {
    hdql_Context_t ctx = hdql_context_create( HDQL_CTX_PRINT_PUSH_ERROR );

    // reentrant table with type interfaces
    hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(valTypes);

    // create and operations table
    hdql_Operations * operations = hdql_context_get_operations(ctx);
    // add std types arithmetics
    hdql_op_define_std_arith(operations, valTypes);
    // emit testing compound types definitions
    hdql::helpers::Compounds compounds = hdql::test::define_compound_types(ctx);
    if(compounds.empty()) return -1;
    hdql_Compound * eventCompound;
    {
        auto it = compounds.find(typeid(hdql::test::Event));
        if(compounds.end() == it) {
            return -1;
        }
        eventCompound = it->second;
    }
    // add standard functions
    hdql_functions_add_standard_math(hdql_context_get_functions(ctx));
    // add standard type conversions
    hdql_converters_add_std(hdql_context_get_conversions(ctx), hdql_context_get_types(ctx));

    // Fill object
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);  // TODO: utilize #nSample to select sample
    
    // Interpret the query
    int rc = 0;
    hdql_Query * q; {
        char * expCpy = strdup(expression);
        char errBuf[256] = "";
        int errDetails[5] = {0, -1, -1, -1, -1};  // error code, first line, first column, last line, last column
        q = hdql_compile_query( expression
                              , eventCompound
                              , ctx
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
        rc = errDetails[0];
        if(rc) {
            char fancyprintbuf[1024];
            rc = hdql_fancy_error_print(fancyprintbuf, sizeof(fancyprintbuf)
                    , expCpy, errDetails, errBuf, 0x1);
            fputs(fancyprintbuf, stderr);
            if(rc) {
                fputs("(truncated)\n", stderr);
            }
            free(expCpy);
            return 1;
        }
        free(expCpy);
    }
    assert(q);
    
    /* initialize CSV dump interface
     * Particular results handler is the extension point; usually specified at
     * the runtime */
    struct hdql_iQueryResultsHandler iqr;
    rc = hdql_query_results_handler_csv_init(&iqr, stdout, ",", "/", "\n", ctx); {
        /* common: instantiate workspace, relying on specified results handler */
        struct hdql_QueryResultsWorkspace * ws = hdql_query_results_init(
                  q
                , NULL  // const char ** attrs
                , &iqr  // struct hdql_iQueryResultsHandler * iqr
                , ctx  // struct hdql_Context * ctx
                );
        if(!ws) {
            fputs("Fatal error: failed to initialize results handler workspace.\n", stderr);
            exit(EXIT_FAILURE);
        }

        /* common: this is the loop -- when new root data instance comes from
         * stream, we re-set the handler and iterate */
        hdql_query_results_process_records_from((hdql_Datum_t) &ev, ws);

        /* common: cleanup common iteration caches */
        hdql_query_results_destroy(ws);
    } hdql_query_results_handler_csv_cleanup(&iqr);
    /* ^^^ clear specific caches, respecting interface specialization */

    hdql_query_destroy(q, ctx);
    hdql_context_destroy(ctx);

    for(auto & ce : compounds) {
        hdql_compound_destroy(ce.second, ctx);
    }

    return rc;
}

//
// Entry point test

int
main(int argc, char * argv[]) {
    return test_query_on_data(1, argv[2]);
}
