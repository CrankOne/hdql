#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/operations.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "samples.hh"

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
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    EXPECT_EQ(1, keysDepth);

    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE(ad);
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_int);

    size_t nVisited = 0;
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        EXPECT_EQ(0x0, keys[0].code);
        EXPECT_EQ(ev.eventID, vi->get_as_int(r));
        ++nVisited;
    }
    EXPECT_EQ(1, nVisited);

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

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
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE(ad);
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        //printf("#%zu: ", nResult++);  // xxx
        //char bf[32] = "";
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < keysDepth; ++lvl) {
            if(0x0 == keys[lvl].code) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, keys[lvl].code);
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(keys[lvl].pl.datum);
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
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

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
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0) << "Can't compile query: " << errBuf;
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE( hdql_attr_def_is_static_value(ad) );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        //printf("#%zu: ", nResult++);  // xxx
        //char bf[32] = "";
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < keysDepth; ++lvl) {
            if(0x0 == keys[lvl].code) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, keys[lvl].code);
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(keys[lvl].pl.datum);
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
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

//
// Iteration of virtual compound providing collection

TEST_F(TestingEventStruct, virtualCompoundDataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks{h := .hits}.h.energyDeposition";
    static struct {
        int keys[2];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101}, 1.},
        {{0, 102}, 2.},
        {{2, 103}, 3.},
        {{2, 202}, 4.},
        {{2, 301}, 5.},
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 4);
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE( hdql_attr_def_is_static_value(ad) );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    size_t flatKeyViewLen = hdql_keys_flat_view_size(keys, _compounds.context_ptr());
    ASSERT_EQ(2, flatKeyViewLen);
    hdql_KeyView keysViews[2];
    hdql_keys_flat_view_update(q, keys, keysViews, _compounds.context_ptr());
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        ASSERT_TRUE(keysViews[0].interface->get_as_int);
        ASSERT_TRUE(keysViews[1].interface->get_as_int);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( keysViews[0].interface->get_as_int(keysViews[0].keyPtr->pl.datum) != expectedQueryResults[i].keys[0]
             || keysViews[1].interface->get_as_int(keysViews[1].keyPtr->pl.datum) != expectedQueryResults[i].keys[1]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

//
// Iteration works for two subsequent sub-queries

TEST_F(TestingEventStruct, twoSubqueriesIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks{h := .hits{eDep := .energyDeposition}}.h.eDep";
    static struct {
        int keys[2];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101}, 1.},
        {{0, 102}, 2.},
        {{2, 103}, 3.},
        {{2, 202}, 4.},
        {{2, 301}, 5.},
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 4);
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE(ad);
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_NE( hdql_attr_def_get_atomic_value_type_code(ad), 0x0 );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    size_t flatKeyViewLen = hdql_keys_flat_view_size(keys, _compounds.context_ptr());
    ASSERT_EQ(2, flatKeyViewLen);
    hdql_KeyView keysViews[2];
    hdql_keys_flat_view_update(q, keys, keysViews, _compounds.context_ptr());
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        ASSERT_TRUE(keysViews[0].interface->get_as_int);
        ASSERT_TRUE(keysViews[1].interface->get_as_int);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( keysViews[0].interface->get_as_int(keysViews[0].keyPtr->pl.datum) != expectedQueryResults[i].keys[0]
             || keysViews[1].interface->get_as_int(keysViews[1].keyPtr->pl.datum) != expectedQueryResults[i].keys[1]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};


//
// Iteration works for virtual compound arithmetics

TEST_F(TestingEventStruct, virtualCompoundArithmeticsWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks.hits{xy := .x*.y, xyz := .xy*.z}.xyz / 2";
    static struct {
        int keys[2];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101}, 3.4*5.6*7.8/2 },
        {{0, 102}, 4.5*6.7*8.9/2 },
        {{2, 103}, 5.6*7.8*9.0/2 },
        {{2, 202}, 6.7*8.9*0.1/2 },
        {{2, 301}, 7.8*9.0*1.2/2 },
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 1);
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_TRUE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE( hdql_attr_def_is_static_value(ad) );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    size_t flatKeyViewLen = hdql_keys_flat_view_size(keys, _compounds.context_ptr());
    ASSERT_EQ(2, flatKeyViewLen);
    hdql_KeyView keysViews[2];
    hdql_keys_flat_view_update(q, keys, keysViews, _compounds.context_ptr());
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        ASSERT_TRUE(keysViews[0].interface->get_as_int);
        ASSERT_TRUE(keysViews[1].interface->get_as_int);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( keysViews[0].interface->get_as_int(keysViews[0].keyPtr->pl.datum) != expectedQueryResults[i].keys[0]
             || keysViews[1].interface->get_as_int(keysViews[1].keyPtr->pl.datum) != expectedQueryResults[i].keys[1]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

//
// Iteration with basic filtering works for virtual compound arithmetics

TEST_F(TestingEventStruct, filteringWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks{: .chi2/.ndf > 3.4}.hits{: (.energyDeposition < 3.5 || .energyDeposition > 4.5)}.x";
    static struct {
        int keys[2];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{2, 103}, .3 },
        {{2, 301}, .4 },
    };

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    char errBuf[128]; int errDetails[5];
    hdql_Query * q = hdql_compile_query( expression
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0);
    // iterate over query results, check all expected keys and values appeared
    //size_t nResult = 0;  // xxx
    hdql_Datum_t r;
    
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(q, &keys, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    //ASSERT_TRUE( topAttrDef->isCollection );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_NE( hdql_attr_def_get_atomic_value_type_code(ad), 0x0 );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    size_t flatKeyViewLen = hdql_keys_flat_view_size(keys, _compounds.context_ptr());
    ASSERT_EQ(2, flatKeyViewLen);
    hdql_KeyView keysViews[2];
    hdql_keys_flat_view_update(q, keys, keysViews, _compounds.context_ptr());
    
    int rc = hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    ASSERT_EQ(HDQL_ERR_CODE_OK, rc);
    while(NULL != (r = hdql_query_get(q, keys, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        ASSERT_TRUE(keysViews[0].interface->get_as_int);
        ASSERT_TRUE(keysViews[1].interface->get_as_int);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( keysViews[0].interface->get_as_int(keysViews[0].keyPtr->pl.datum) != expectedQueryResults[i].keys[0]
             || keysViews[1].interface->get_as_int(keysViews[1].keyPtr->pl.datum) != expectedQueryResults[i].keys[1]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    
    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

