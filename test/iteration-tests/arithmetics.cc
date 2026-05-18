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
    CompileQuery(".hits.rawData.time + .eventID", true);
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
    hdql_Datum_t r;

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_NE( hdql_attr_def_get_atomic_value_type_code(ad), 0x0 );
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    
    typedef void (*FillSampleCallback_t)(hdql::test::Event &);
    FillSampleCallback_t fs[] = { &hdql::test::fill_data_sample_1
                                , &hdql::test::fill_data_sample_2
                                , &hdql::test::fill_data_sample_3
                                };

    for(int nExample = 0; nExample < 3; ++nExample) {
        hdql::test::Event ev;
        fs[nExample](ev);
        ResetQuery(&ev);
        while(NULL != (r = hdql_query_get(_query, _queryKey, _compounds.context_ptr()))) {
            // locate and mark as visited, assuring it was not visited before
            bool found = false;
            auto keyAsInt = _flatKeyIfaces[0]->get_as_int(hdql_key_datum_get(*_flatKeyView));
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

    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited)
            << "Item was not visited: id=" << expectedQueryResults[i].key;
    }
};

