#include "events-struct.hh"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/operations.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include "events-struct.hh"

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>

//
// Example of event structs

static int
test_query_on_data( int nSample, const char * expression ) {
    hdql_Context_t ctx = hdql_context_create();

    // reentrant table with type interfaces
    hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(valTypes);

    // create and operations table
    hdql_Operations * operations = hdql_context_get_operations(ctx);
    // add std types arithmetics
    hdql_op_define_std_arith(operations, valTypes);
    // this is the compound types definitions (TODO: should be wrapped in C++
    // template-based helpers)
    struct hdql_Compound * eventCompound = hdql_compound_new("Event")
                       , * trackCompound = hdql_compound_new("Track")
                       , * hitCompound   = hdql_compound_new("Hit")
                       ;
    int rc = hdql::test::fill_tables(valTypes, eventCompound, trackCompound, hitCompound);
    if(rc) return -1;

    // Fill object

    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);  // TODO: utilize #nSample to select sample
    
    rc = 0;
    hdql_Query * q; {
        char errBuf[256] = "";
        int errDetails[5] = {0, -1, -1, -1, -1};  // error code, first column, first line, last column, last line
        q = hdql_compile_query( expression
                              , eventCompound
                              , ctx
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
        rc = errDetails[0];
        if(rc) {
            fprintf( stderr, "Expression error (%d,%d-%d,%d): %s\n"
                   , errDetails[1], errDetails[2], errDetails[3], errDetails[4]
                   , errBuf);
            return 1;
        }
    }
    assert(q);
    hdql_query_dump(stdout, q, ctx);
    const hdql_AttributeDefinition * topAttrDef = hdql_query_top_attr(q);
    // iterate over query results
    size_t nResult = 0;
    hdql_Datum_t r;
    hdql_CollectionKey * keys = NULL;
    rc = hdql_query_reserve_keys_for(q, &keys, ctx);
    assert(rc == 0);

    char keyStrBf[512] = "";  // 1st is null -- important!
    hdql_KeyPrintParams kpp;
    kpp.strBuf = keyStrBf;
    kpp.strBufLen = sizeof(keyStrBf);

    size_t flatKeyViewLength = hdql_keys_flat_view_size(q, keys, ctx);
    hdql_KeyView * kv = flatKeyViewLength
                      ? (hdql_KeyView *) malloc(sizeof(hdql_KeyView)*flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(q, keys, kv, ctx);
    char flKStr[1024] = "";

    bool hadResult = false;
    while(NULL != (r = hdql_query_get(q, reinterpret_cast<hdql_Datum_t>(&ev), keys, ctx))) {
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
                kv[nFKV].interface->get_as_string(kv[nFKV].keyPtr->datum, fkvBf, sizeof(fkvBf));
                strcat(flKStr, fkvBf);
            } else {
                strcat(flKStr, "(no str.method)");
            }
        }
        if(flatKeyViewLength) {
            strcat(flKStr, "), ");
        }

        hdql_query_for_every_key(q, keys, ctx, hdql_query_print_key_to_buf, &kpp);
        printf("#%zu: %s[%s] => ", nResult++, flKStr, kpp.strBuf);
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
    }
    if(kv) free(kv);

    hdql_query_destroy_keys_for(q, keys, ctx);
    hdql_query_destroy(q, ctx);
    hdql_context_destroy(ctx);

    hdql_compound_destroy(eventCompound);
    hdql_compound_destroy(hitCompound);
    hdql_compound_destroy(trackCompound);

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
