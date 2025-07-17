#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/errors.h"
#include "hdql/helpers/compounds.hh"
#include "hdql/helpers/query-results-table.h"
#include "hdql/helpers/fancy-print-err.h"
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
            #if 0
            fputs(expression, stderr);
            fprintf( stderr, "Expression error (%d,%d-%d,%d): %s\n"
                   , errDetails[1], errDetails[2], errDetails[3], errDetails[4]
                   , errBuf);
            #else
            char fancyprintbuf[1024];
            rc = hdql_fancy_error_print(fancyprintbuf, sizeof(fancyprintbuf)
                    , expCpy, errDetails, errBuf, 0x1);
            fputs(fancyprintbuf, stderr);
            if(rc) {
                fputs("(truncated)\n", stderr);
            }
            #endif
            free(expCpy);
            return 1;
        }
        free(expCpy);
    }
    assert(q);
    puts("-- query composition:");
    hdql_query_dump(stdout, q, ctx);
    const hdql_AttrDef * topAttrDef = hdql_query_top_attr(q);
    // iterate over query results
    size_t nResult = 0;
    hdql_Datum_t r;
    hdql_CollectionKey * keys = NULL;
    rc = hdql_query_keys_reserve(q, &keys, ctx);
    assert(rc == 0);

    size_t flatKeyViewLength = hdql_keys_flat_view_size(q, keys, ctx);
    hdql_KeyView * kv = flatKeyViewLength
                      ? (hdql_KeyView *) malloc(sizeof(hdql_KeyView)*flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(q, keys, kv, ctx);
    char flKStr[1024] = "";

    bool hadResult = false;
    rc = hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), ctx);
    if(HDQL_ERR_CODE_OK != rc) {
        fprintf(stderr, "Can't initialize query with data object.\n");
        return -1;  // TODO: cleanup? details?
    }

    //puts("-- uninit keys topology:");
    //hdql_query_keys_dump(keys, keyStrBf, sizeof(keyStrBf), ctx);
    //puts(keyStrBf);

    #if 0
    puts("-- query columns:"); {
        char * colNames[256];
        size_t nColumns = 256;
        hdql_stream_column_names(q, colNames, &nColumns, '/');
        if(nColumns) {
            fflush(stdout);  // XXX
            for(size_t i = 0; i < nColumns; ++i) {
                if(i) fputs(", ", stdout);
                if(colNames[i])
                    fputs(colNames[i], stdout);
                else
                    fputs("(NULL)", stdout);
            }
            fputs("\n", stdout);
        } else {
            puts("(no columns)");
        }
    }
    #endif

    puts("-- query results:");
    while(NULL != (r = hdql_query_get(q, keys, ctx))) {
        hadResult = true;
        if(flatKeyViewLength) {
            flKStr[0] = ' ';
            flKStr[1] = '(';
            flKStr[2] = '\0';
        } else {
            *flKStr = '\0';
        }
        for(size_t nFKV = 0; nFKV < flatKeyViewLength; ++nFKV) {
            if(nFKV) strcat(flKStr, ", ");
            if(kv[nFKV].interface && kv[nFKV].interface->get_as_string) {
                char fkvBf[64];
                kv[nFKV].interface->get_as_string(kv[nFKV].keyPtr->pl.datum, fkvBf, sizeof(fkvBf), ctx);
                strcat(flKStr, fkvBf);
            } else {
                strcat(flKStr, "(no str.method)");
            }
        }
        if(flatKeyViewLength) {
            strcat(flKStr, "), ");
        }

        printf(" %4lu) ", nResult);

        if(!topAttrDef) {
            fputs("??? (query did not provide attribute definition)", stdout);
        } else {  // valid result
            fputs(flKStr, stdout);  // flat keys
            if(hdql_attr_def_is_static_value(topAttrDef)) {  // returns static value
                // xxx, example: `2 + 3'
                printf( "static value %p of type %d"
                      ,  hdql_attr_def_get_static_value(topAttrDef)
                      , (int) hdql_attr_def_get_atomic_value_type_code(topAttrDef)
                      );
                if(hdql_attr_def_get_atomic_value_type_code(topAttrDef)) {
                    const hdql_ValueInterface * vi
                        = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(topAttrDef));
                    if(vi && vi->get_as_string) {
                        char bf[32];
                        vi->get_as_string(r, bf, sizeof(bf), ctx);
                        printf( "value=\"%s\"", bf );
                    } else {
                        fputs(" (no value interface)", stdout);
                    }
                }
            } else if(hdql_attr_def_is_fwd_query(topAttrDef)) {  // forwarding query
                printf("subquery %p", hdql_attr_def_fwd_query(topAttrDef));
            } else if(!hdql_attr_def_is_atomic(topAttrDef)) {  // compound instance(s) of certain type
                assert(hdql_attr_def_is_compound(topAttrDef));
                const struct hdql_Compound * ct = hdql_attr_def_compound_type_info(topAttrDef);
                if(!hdql_compound_is_virtual(ct)) {
                    printf( "compound %s instance %p of type `%s'"
                          , hdql_attr_def_is_collection(topAttrDef) ? "collection" : "scalar"
                          , r
                          , hdql_compound_get_name(hdql_attr_def_compound_type_info(topAttrDef))
                          );
                } else {
                    char buf[128];
                    hdql_compound_get_full_type_str(ct, buf, sizeof(buf));
                    printf( "compound %s instance %p of type `%s'"
                          , hdql_attr_def_is_collection(topAttrDef) ? "collection" : "scalar"
                          , r
                          , buf
                          );
                }
            } else {  // atomic item
                if(!hdql_attr_def_is_collection(topAttrDef)) {  // scalar compound attribute
                    // xxx, example: `.eventID', `.hits.x'
                    const hdql_ValueInterface * vi = hdql_types_get_type(valTypes
                            , hdql_attr_def_get_atomic_value_type_code(topAttrDef) );
                    if(!vi) {
                        printf( "scalar instance %p of unknown type", r);
                    } else if(!vi->get_as_string) {
                        printf( "scalar instance %p of type <%s> (no to-string method)"
                                , r, vi->name );
                    } else {
                        char valueBf[32];
                        vi->get_as_string(r, valueBf, sizeof(valueBf), ctx);
                        printf( "scalar instance %p of type <%s> with value \"%s\""
                                , r, vi->name, valueBf );
                    }
                } else {  // item from collection of atomic scalars (1d list or array)
                    // xxx, example: `.hits.rawData.samples'
                    #if 1
                    const hdql_ValueInterface * vi
                        = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(topAttrDef));
                    if(vi && vi->get_as_string) {
                        char bf[32];
                        vi->get_as_string(r, bf, sizeof(bf), ctx);
                        printf( "value=\"%s\"", bf );
                    } else {
                        fputs(" (no value interface)", stdout);
                    }
                    #else
                    // Query resulted in weird item
                    printf("??? (attr.def. provided, %s, %s, %s %s)"
                          , topAttrDef->staticValueFlags ? "stat.val." : "not a static value"
                          , topAttrDef->isSubQuery ? "subquery" : "not a subquery"
                          , topAttrDef->isAtomic ? "atomic" : "compound"
                          , topAttrDef->isCollection ? "collection" : "scalar"
                          );
                    #endif
                }
            }
            puts(".");
        }  // end of valid result handling

        #if 0
        printf("#%zu: %s[%s] => ", nResult++, flKStr, keyStrBf);
        if(!topAttrDef) {
            fputs("??? (query did not provide attribute definition)", stdout);
        } else {
            if(topAttrDef->staticValueFlags) {
                printf( "static value %p of type %d"
                      , topAttrDef->typeInfo.subQuery
                      , (int) topAttrDef->typeInfo.staticValue.typeCode
                      );
                if(topAttrDef->typeInfo.staticValue.typeCode) {
                    const hdql_ValueInterface * vi
                        = hdql_types_get_type(valTypes, topAttrDef->typeInfo.staticValue.typeCode);
                    if(vi && vi->get_as_string) {
                        char bf[32];
                        vi->get_as_string(r, bf, sizeof(bf));
                        printf( "value=\"%s\"", bf );
                    } else {
                        fputs(" (no value interface)", stdout);
                    }
                }
            } else if(topAttrDef->isSubQuery) {
                printf("subquery %p", topAttrDef->typeInfo.subQuery);
            } else if(!topAttrDef->isAtomic) {
                printf( "compound %s instance %p of type `%s'"
                      , topAttrDef->isCollection ? "collection" : "scalar"
                      , r
                      , hdql_compound_get_name(topAttrDef->typeInfo.compound)
                      );
            } else {
                if(!topAttrDef->isCollection) {
                    const hdql_ValueInterface * vi = hdql_types_get_type(valTypes
                            , topAttrDef->typeInfo.atomic.arithTypeCode );
                    if(!vi) {
                        printf( "scalar intance %p of unknown type", r);
                    } else if(!vi->get_as_string) {
                        printf( "scalar intance %p of type %s (no to-string method)"
                                , r, vi->name );
                    } else {
                        char valueBf[32];
                        vi->get_as_string(r, valueBf, sizeof(valueBf));
                        printf( "scalar intance %p of type %s with value \"%s\""
                                , r, vi->name, valueBf );
                    }
                } else {
                    #if 1
                    // collection of atomic values
                    assert(!topAttrDef->staticValueFlags);
                    assert(!topAttrDef->isSubQuery);
                    assert(topAttrDef->isAtomic);
                    assert(topAttrDef->isCollection);
                    // this is true only for collections resulting in
                    // arithmetic 
                    assert(topAttrDef->typeInfo.atomic.arithTypeCode);
                    const hdql_ValueInterface * vi
                        = hdql_types_get_type(valTypes, topAttrDef->typeInfo.atomic.arithTypeCode);
                    if(vi && vi->get_as_string) {
                        char bf[32];
                        vi->get_as_string(r, bf, sizeof(bf));
                        printf( "value=\"%s\"", bf );
                    } else {
                        fputs(" (no value interface)", stdout);
                    }
                    #else
                    // Query resulted in weird item
                    printf("??? (attr.def. provided, %s, %s, %s %s)"
                          , topAttrDef->staticValueFlags ? "stat.val." : "not a static value"
                          , topAttrDef->isSubQuery ? "subquery" : "not a subquery"
                          , topAttrDef->isAtomic ? "atomic" : "compound"
                          , topAttrDef->isCollection ? "collection" : "scalar"
                          );
                    #endif
                }
            }
        }
        printf("\n");
        //printf("  type: %s\n", hdql_qeury_result_type(qr) ? ... );
        keyStrBf[0] = '\0';
        #endif
        ++nResult;
    }
    if(kv) free(kv);

    hdql_query_keys_destroy(keys, ctx);
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
