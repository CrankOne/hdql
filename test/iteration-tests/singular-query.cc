#include "../events-struct.hh"
#include "../samples.hh"
#include "hdql/attr-def.h"
#include "hdql/query-key.h"

#include <gtest/gtest.h>

using hdql::test::TestingEventStruct;

//
// Retrieval of simple scalar atomic field within root compound instance

TEST_F(TestingEventStruct, retrievesSimpleScalarValue) {
    const char expression[] = ".eventID";

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _rootCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    EXPECT_EQ(1, keysDepth);

    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));
    ASSERT_TRUE(hdql_key_is_list(key));  // query key result is always a list
    ASSERT_EQ(1, hdql_key_get_list_len(key));
    EXPECT_TRUE(hdql_key_is_empty(hdql_key_get_list_item(key, 0)));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE(ad);
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_int);

    size_t nVisited = 0;
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        EXPECT_TRUE(hdql_key_is_empty(hdql_key_get_list_item(key, 0)));
        EXPECT_EQ(ev.eventID, vi->get_as_int(r));
        ++nVisited;
    }
    EXPECT_EQ(1, nVisited);

    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());
}

//
// Iteration of two-level collection

TEST_F(TestingEventStruct, straightDataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks.hits.x";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101, -1}, 3.4},
        {{0, 102, -1}, 4.5},
        {{2, 103, -1}, 5.6},
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _rootCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));
    size_t keysDepth = hdql_key_get_list_len(key);
    ASSERT_GT(keysDepth, 0);

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE(ad);
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        //printf("#%zu: ", nResult++);  // xxx
        //char bf[32] = "";
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < keysDepth; ++lvl) {
            hdql_Key * sk = hdql_key_get_list_item(key, lvl);
            ASSERT_TRUE(sk);
            ASSERT_TRUE(hdql_key_is_datum(sk) || hdql_key_is_empty(sk));
            if(hdql_key_is_empty(sk)) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, hdql_key_datum_get_type_code(sk));
            assert(vi->get_as_int);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(hdql_key_datum_get(sk));
        }
        //vi->get_as_string(r, bf, sizeof(bf));
        //printf(" => %s\n", bf);
        //printf("  type: %s\n", hdql_qeury_result_type(qr) ? ... );
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kk[0] != expectedQueryResults[i].keys[0]
             || kk[1] != expectedQueryResults[i].keys[1]
             || kk[2] != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

//
// Selective iteration of two-level collection

TEST_F(TestingEventStruct, selectiveDataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks[2:...].hits[202:...].x";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _rootCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0) << "Can't compile query: " << errBuf;
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));
    size_t keysDepth = hdql_key_get_list_len(key);
    ASSERT_GT(keysDepth, 0);

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        //printf("#%zu: ", nResult++);  // xxx
        //char bf[32] = "";
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < keysDepth; ++lvl) {
            hdql_Key * sk = hdql_key_get_list_item(key, lvl);
            ASSERT_TRUE(sk);
            ASSERT_TRUE(hdql_key_is_datum(sk) || hdql_key_is_empty(sk));
            if(hdql_key_is_empty(sk)) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, hdql_key_datum_get_type_code(sk));
            assert(vi->get_as_int);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(hdql_key_datum_get(sk));
        }
        //vi->get_as_string(r, bf, sizeof(bf));
        //printf(" => %s\n", bf);
        //printf("  type: %s\n", hdql_qeury_result_type(qr) ? ... );
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kk[0] != expectedQueryResults[i].keys[0]
             || kk[1] != expectedQueryResults[i].keys[1]
             || kk[2] != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};
