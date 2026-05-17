#include "../events-struct.hh"
#include "../samples.hh"
#include "hdql/context.h"
#include "hdql/query-key.h"
#include "hdql/value.h"

#include <gtest/gtest.h>

using hdql::test::TestingEventStruct;

//
// Iteration with basic filtering works for virtual compound arithmetics

TEST_F(TestingEventStruct, iterationWorksOnDifferentLevelsOfRootObject) {
    const char expression[] = ".hits.rawData.time + .eventID";
    static struct {
        int key;
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {101, .01 + 104501 },
        {102, .02 + 104501 },
        {103, .03 + 104501 },
        {301, .05 + 104501 },

        {401, .10 + 104502 },
        {402, .11 + 104502 },
        {501, .12 + 104502 },
        {502, .15 + 104502 },

        {10, .21 + 104503 },
        {11, .23 + 104503 },
        {12, .24 + 104503 },
        {99, .25 + 104503 },
    };

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
    
    hdql_Key * key = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(q, key, _compounds.context_ptr()));

    const hdql_AttrDef * ad = hdql_query_top_attr(q);
    ASSERT_TRUE( ad );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_NE( hdql_attr_def_get_atomic_value_type_code(ad), 0x0 );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    size_t flatKeyViewLen = hdql_key_flat_view_size(key, _compounds.context_ptr());
    ASSERT_EQ(1, flatKeyViewLen);
    hdql_Key ** keyView = new hdql_Key * [flatKeyViewLen];
    hdql_key_flat_view_populate(key, keyView);

    ASSERT_TRUE(keyView[0]);
    ASSERT_NE(hdql_key_datum_get_type_code(*keyView), 0x0);

    const hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    const hdql_ValueInterface * kvi = hdql_types_get_type(types, hdql_key_datum_get_type_code(*keyView));
    ASSERT_TRUE(kvi);
    ASSERT_TRUE(kvi->get_as_int);
    
    typedef void (*FillSampleCallback_t)(hdql::test::Event &);
    FillSampleCallback_t fs[] = { &hdql::test::fill_data_sample_1
                                , &hdql::test::fill_data_sample_2
                                , &hdql::test::fill_data_sample_3
                                };

    for(int nExample = 0; nExample < 3; ++nExample) {
        hdql::test::Event ev;
        fs[nExample](ev);
        int rc = hdql_query_reset(q, reinterpret_cast<hdql_Datum_t>(&ev), _compounds.context_ptr());
        ASSERT_EQ(HDQL_ERR_CODE_OK, rc);
        while(NULL != (r = hdql_query_get(q, key, _compounds.context_ptr()))) {
            // locate and mark as visited, assuring it was not visited before
            bool found = false;
            auto keyAsInt = kvi->get_as_int(hdql_key_datum_get(*keyView));
            for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
                if(keyAsInt != expectedQueryResults[i].key) continue;
                found = true;
                EXPECT_FALSE(expectedQueryResults[i].visited)
                    << "Item is visited again: id=" << expectedQueryResults[i].key;
                expectedQueryResults[i].visited = true;
                break;
            }
            EXPECT_TRUE(found);
        }
    }
    
    EXPECT_EQ(0, hdql_key_destroy(key, _compounds.context_ptr()));
    delete [] keyView;

    hdql_query_destroy(q, _compounds.context_ptr());

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited)
            << "Item was not visited: id=" << expectedQueryResults[i].key;
    }
};

