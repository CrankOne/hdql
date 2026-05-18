#if 0
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
#endif

#include "../events-struct.hh"
#include "../samples.hh"

#include <gtest/gtest.h>

using hdql::test::TestingEventStruct;

TEST_F(TestingEventStruct, boundAttribute_DataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = ".tracks{h := *.hits}.h.energyDeposition";
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

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(2, flatKeyViewLen);
    hdql_Key * keysViews[2];
    hdql_key_flat_view_populate(key, keysViews);

    ASSERT_TRUE(keysViews[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[0]), 0x0);
    ASSERT_TRUE(keysViews[1]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[1]), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi1 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[0]));
    ASSERT_TRUE(kvi1);
    ASSERT_TRUE(kvi1->get_as_int);
    const hdql_ValueInterface * kvi2 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[1]));
    ASSERT_TRUE(kvi2);
    ASSERT_TRUE(kvi2->get_as_int);
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) != expectedQueryResults[i].keys[0]
             || kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) != expectedQueryResults[i].keys[1]
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

TEST_F(TestingEventStruct, boundAttribute_DataIterationWorksAsCartesianProduct) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = "{th:=*.tracks.hits, h:=*.hits}{dx:=.h.x-.th.x}.dx";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{101, 0, 101}, 3.4 - 3.4},
        {{102, 0, 101}, 4.5 - 3.4},
        {{103, 0, 101}, 5.6 - 3.4},
        {{202, 0, 101}, 6.7 - 3.4},
        {{301, 0, 101}, 7.8 - 3.4},

        {{101, 0, 102}, 3.4 - 4.5},
        {{102, 0, 102}, 4.5 - 4.5},
        {{103, 0, 102}, 5.6 - 4.5},
        {{202, 0, 102}, 6.7 - 4.5},
        {{301, 0, 102}, 7.8 - 4.5},

        {{101, 2, 103}, 3.4 - 5.6},
        {{102, 2, 103}, 4.5 - 5.6},
        {{103, 2, 103}, 5.6 - 5.6},
        {{202, 2, 103}, 6.7 - 5.6},
        {{301, 2, 103}, 7.8 - 5.6},

        {{101, 2, 202}, 3.4 - 6.7},
        {{102, 2, 202}, 4.5 - 6.7},
        {{103, 2, 202}, 5.6 - 6.7},
        {{202, 2, 202}, 6.7 - 6.7},
        {{301, 2, 202}, 7.8 - 6.7},

        {{101, 2, 301}, 3.4 - 7.8},
        {{102, 2, 301}, 4.5 - 7.8},
        {{103, 2, 301}, 5.6 - 7.8},
        {{202, 2, 301}, 6.7 - 7.8},
        {{301, 2, 301}, 7.8 - 7.8},
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
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 3);
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_float);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(3, flatKeyViewLen);
    hdql_Key * keysViews[3];
    hdql_key_flat_view_populate(key, keysViews);

    ASSERT_TRUE(keysViews[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[0]), 0x0);
    ASSERT_TRUE(keysViews[1]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[1]), 0x0);
    ASSERT_TRUE(keysViews[2]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[2]), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi1 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[0]));
    ASSERT_TRUE(kvi1);
    ASSERT_TRUE(kvi1->get_as_int);
    const hdql_ValueInterface * kvi2 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[1]));
    ASSERT_TRUE(kvi2);
    ASSERT_TRUE(kvi2->get_as_int);
    const hdql_ValueInterface * kvi3 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[2]));
    ASSERT_TRUE(kvi3);
    ASSERT_TRUE(kvi3->get_as_int);
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    double v;
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        v = vi->get_as_float(r);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) != expectedQueryResults[i].keys[0]
             || kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) != expectedQueryResults[i].keys[1]
             || kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            EXPECT_NEAR(expectedQueryResults[i].result, v, 1e-6);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found) << "extra value: key=("
            << kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) << ", "
            << kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) << ", "
            << kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) << ") value="
            << v;
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};


TEST_F(TestingEventStruct, boundAttribute_DataIterationWorksAsCartesianProductOfFiltered) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = "{th:=*.tracks.hits{:.x > 5.7}, h:=*.hits{:.x < 5.7}}{dx:=.h.x-.th.x}.dx";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{101, 2, 202}, 3.4 - 6.7},
        {{102, 2, 202}, 4.5 - 6.7},
        {{103, 2, 202}, 5.6 - 6.7},

        {{101, 2, 301}, 3.4 - 7.8},
        {{102, 2, 301}, 4.5 - 7.8},
        {{103, 2, 301}, 5.6 - 7.8},
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
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 3);
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_float);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(3, flatKeyViewLen);
    hdql_Key * keysViews[3];
    hdql_key_flat_view_populate(key, keysViews);

    ASSERT_TRUE(keysViews[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[0]), 0x0);
    ASSERT_TRUE(keysViews[1]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[1]), 0x0);
    ASSERT_TRUE(keysViews[2]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[2]), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi1 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[0]));
    ASSERT_TRUE(kvi1);
    ASSERT_TRUE(kvi1->get_as_int);
    const hdql_ValueInterface * kvi2 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[1]));
    ASSERT_TRUE(kvi2);
    ASSERT_TRUE(kvi2->get_as_int);
    const hdql_ValueInterface * kvi3 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[2]));
    ASSERT_TRUE(kvi3);
    ASSERT_TRUE(kvi3->get_as_int);
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    double v;
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        v = vi->get_as_float(r);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) != expectedQueryResults[i].keys[0]
             || kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) != expectedQueryResults[i].keys[1]
             || kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            EXPECT_NEAR(expectedQueryResults[i].result, v, 1e-6);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found) << "extra value: key=("
            << kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) << ", "
            << kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) << ", "
            << kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) << ") value="
            << v;
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};


TEST_F(TestingEventStruct, boundAttribute_FilteredDataIterationWorksAsCartesianProduct_1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = "{th:=*.tracks.hits, h:=*.hits : .h.x > .th.x}{dx:=.h.x-.th.x}.dx";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{102, 0, 101}, 4.5 - 3.4},
        {{103, 0, 101}, 5.6 - 3.4},
        {{202, 0, 101}, 6.7 - 3.4},
        {{301, 0, 101}, 7.8 - 3.4},

        {{103, 0, 102}, 5.6 - 4.5},
        {{202, 0, 102}, 6.7 - 4.5},
        {{301, 0, 102}, 7.8 - 4.5},

        {{202, 2, 103}, 6.7 - 5.6},
        {{301, 2, 103}, 7.8 - 5.6},

        {{301, 2, 202}, 7.8 - 6.7},
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
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 3);
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_float);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(3, flatKeyViewLen);
    hdql_Key * keysViews[3];
    hdql_key_flat_view_populate(key, keysViews);

    ASSERT_TRUE(keysViews[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[0]), 0x0);
    ASSERT_TRUE(keysViews[1]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[1]), 0x0);
    ASSERT_TRUE(keysViews[2]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[2]), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi1 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[0]));
    ASSERT_TRUE(kvi1);
    ASSERT_TRUE(kvi1->get_as_int);
    const hdql_ValueInterface * kvi2 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[1]));
    ASSERT_TRUE(kvi2);
    ASSERT_TRUE(kvi2->get_as_int);
    const hdql_ValueInterface * kvi3 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[2]));
    ASSERT_TRUE(kvi3);
    ASSERT_TRUE(kvi3->get_as_int);
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    double v;
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        v = vi->get_as_float(r);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) != expectedQueryResults[i].keys[0]
             || kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) != expectedQueryResults[i].keys[1]
             || kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            EXPECT_NEAR(expectedQueryResults[i].result, v, 1e-6);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found) << "extra value: key=("
            << kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) << ", "
            << kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) << ", "
            << kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) << ") value="
            << v;
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

TEST_F(TestingEventStruct, boundAttribute_FilteredDataIterationWorksAsCartesianProduct_2) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    const char expression[] = "{th:=*.tracks.hits, h:=*.hits : .th.x > .h.x}{dx:=.th.x-.h.x}.dx";
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{101, 0, 102},  4.5 - 3.4},

        {{101, 2, 103},  5.6 - 3.4},
        {{102, 2, 103},  5.6 - 4.5},

        {{101, 2, 202},  6.7 - 3.4},
        {{102, 2, 202},  6.7 - 4.5},
        {{103, 2, 202},  6.7 - 5.6},

        {{101, 2, 301},  7.8 - 3.4},
        {{102, 2, 301},  7.8 - 4.5},
        {{103, 2, 301},  7.8 - 5.6},
        {{202, 2, 301},  7.8 - 6.7},
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
    size_t keysDepth = hdql_query_depth(q);
    ASSERT_EQ(keysDepth, 3);
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    ASSERT_TRUE(vi->get_as_float);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(3, flatKeyViewLen);
    hdql_Key * keysViews[3];
    hdql_key_flat_view_populate(key, keysViews);

    ASSERT_TRUE(keysViews[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[0]), 0x0);
    ASSERT_TRUE(keysViews[1]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[1]), 0x0);
    ASSERT_TRUE(keysViews[2]);
    ASSERT_NE(hdql_key_datum_get_type_code(keysViews[2]), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi1 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[0]));
    ASSERT_TRUE(kvi1);
    ASSERT_TRUE(kvi1->get_as_int);
    const hdql_ValueInterface * kvi2 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[1]));
    ASSERT_TRUE(kvi2);
    ASSERT_TRUE(kvi2->get_as_int);
    const hdql_ValueInterface * kvi3 = hdql_types_get_type(types, hdql_key_datum_get_type_code(keysViews[2]));
    ASSERT_TRUE(kvi3);
    ASSERT_TRUE(kvi3->get_as_int);
    
    hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
    double v;
    while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        v = vi->get_as_float(r);
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) != expectedQueryResults[i].keys[0]
             || kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) != expectedQueryResults[i].keys[1]
             || kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            EXPECT_NEAR(expectedQueryResults[i].result, v, 1e-6);
            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found) << "extra value: key=("
            << kvi1->get_as_int(hdql_key_datum_get(keysViews[0])) << ", "
            << kvi2->get_as_int(hdql_key_datum_get(keysViews[1])) << ", "
            << kvi3->get_as_int(hdql_key_datum_get(keysViews[2])) << ") value="
            << v;
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
};

