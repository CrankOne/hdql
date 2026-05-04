#include "../events-struct.hh"
#include "../samples.hh"

#include <gtest/gtest.h>

using hdql::test::TestingEventStruct;

//
// Iteration of virtual compound providing collection

TEST_F(TestingEventStruct, forwardingAttribute_DataIterationWorksOnSample1) {
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

