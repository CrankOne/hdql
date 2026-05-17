#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/errors.h"
#include "hdql/helpers/compounds.hh"
#include "hdql/helpers/fancy-print-err.h"
#include "hdql/helpers/print-tree.h"
#include "hdql/query-key.h"
#include "hdql/random.h"
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
    hdql_rand_seed(hdql_context_get_randgen(ctx), 0xdeadbeef, 0 );

    // reentrant table with type interfaces
    hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(valTypes);

    // create and operations table
    hdql_Operations * operations = hdql_context_get_operations(ctx);
    // add std types arithmetics
    hdql_op_define_std_arith(operations, valTypes);
    // emit testing compound types definitions
    hdql::helpers::Compounds compounds = hdql::test::define_test_event_compound(ctx);
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
    // add standard monoid functions
    hdql_functions_add_monoids(hdql_context_get_functions(ctx));
    // add standard math constants
    hdql_constants_define_standard_math(hdql_context_get_constants(ctx));
    // add standard type conversions
    hdql_converters_add_std(hdql_context_get_conversions(ctx), hdql_context_get_types(ctx), ctx);
    
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
    puts("*** query composition (final result goes last):");
    hdql_query_dump(stdout, q, ctx);
    const hdql_AttrDef * topAttrDef = hdql_query_top_attr(q);
    puts("*** top attribute definition (recursive compounds expansion):");
    hdql_attr_def_tree_print(topAttrDef, ctx, 128, stdout, 0x1);
    // iterate over query results
    size_t nResult = 0;
    hdql_Datum_t r;
    hdql_Key * key = hdql_key_new(ctx);
    rc = hdql_key_reserve_for_query(q, key, ctx);
    assert(rc == 0);

    puts("*** resulting query key structure:");
    hdql_key_print_tree(key, stdout, ctx);

    //size_t flatKeyViewLength = hdql_keys_flat_view_size(key, ctx);
    //hdql_KeyView * kv = flatKeyViewLength
    //                  ? (hdql_KeyView *) malloc(sizeof(hdql_KeyView)*flatKeyViewLength)
    //                  : NULL;
    //hdql_keys_flat_view_update(q, keys, kv, ctx);
    //char flKStr[1024] = "";

    bool hadResult = false;

    hdql::test::Event ev;
    typedef void (*FillSampleCallback_t)(hdql::test::Event &);
    FillSampleCallback_t fs[] = { &hdql::test::fill_data_sample_1
                , &hdql::test::fill_data_sample_2
                , &hdql::test::fill_data_sample_3};
    for(int i = 0; i < 3; ++i) {
        fs[i](ev);  // TODO: utilize #nSample to select sample

        rc = hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), ctx);
        if(HDQL_ERR_CODE_OK != rc) {
            fprintf(stderr, "Can't initialize query with data object.\n");
            return -1;  // TODO: cleanup? details?
        }

        printf("*** sample #%d query results:\n", i);
        while(NULL != (r = hdql_query_get(q, key, ctx))) {
            hadResult = true;
            //if(flatKeyViewLength) {
            //    flKStr[0] = ' ';
            //    flKStr[1] = '(';
            //    flKStr[2] = '\0';
            //} else {
            //    *flKStr = '\0';
            //}
            //for(size_t nFKV = 0; nFKV < flatKeyViewLength; ++nFKV) {
            //    if(nFKV) strcat(flKStr, ", ");
            //    if(kv[nFKV].interface && kv[nFKV].interface->get_as_string) {
            //        char fkvBf[64];
            //        kv[nFKV].interface->get_as_string(kv[nFKV].keyPtr->pl.datum, fkvBf, sizeof(fkvBf), ctx);
            //        strcat(flKStr, fkvBf);
            //    } else {
            //        strcat(flKStr, "(no str.method)");
            //    }
            //}
            //if(flatKeyViewLength) {
            //    strcat(flKStr, "), ");
            //}

            printf(" %4lu) ", nResult);

            if(!topAttrDef) {
                fputs("??? (query did not provide attribute definition)", stdout);
            } else {  // valid result
            //    fputs(flKStr, stdout);  // flat keys
                if(hdql_attr_def_is_compound(topAttrDef)) {
                    putc('\n', stdout);
                }
                hdql_attr_def_tree_data_print(topAttrDef, r, ctx, 127, stdout, 0x0);
            }  // end of valid result handling
            ++nResult;
        }
    }
    //if(kv) free(kv);

    hdql_key_destroy(key, ctx);
    hdql_query_destroy(q, ctx);
    hdql_context_destroy(ctx);
    for(auto & ce : compounds) {
        hdql_compound_destroy(ce.second, ctx);
    }

    if(!hadResult) {
        fputs("Query resulted in empty set.\n", stdout);
    }

    return rc;
}

//
// Entry point test

int
main(int argc, char * argv[]) {
    if(argc == 3 && !strncmp(argv[1], "data", 4)) {
        return test_query_on_data(1, argv[2]);
    } else {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
}
