
// Test basic multi-level query iteration of attributes definitions of various
// kinds (atomic and compound, scalar and collection) created by C++ helpers.
// Queries are build by parser this time.
//
// Attribute definitions used in these queries are defined in testing event
// struct compounds.

#include "../iteration-results.hh"
#include "../samples.hh"
#include "hdql/attr-def.h"
#include "hdql/query-key.h"
#include "hdql/types.h"

#include <gtest/gtest.h>

using hdql::test::QueryIterationTest;

//
// Retrieval of simple scalar atomic field within root compound instance

TEST_F(QueryIterationTest, retrievesSimpleScalarValue) {
    CompileQuery(".eventID", true);
    EXPECT_EQ(1, hdql_query_depth(_query));

    hdql::test::Event ev;
    fill_data_sample_1(ev);

    ASSERT_EQ(1, hdql_key_get_list_len(_queryKey));
    EXPECT_TRUE(hdql_key_is_empty(hdql_key_get_list_item(_queryKey, 0)));

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
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
    // iterate over query results, check all expected keys and values appeared
    hdql_Datum_t r;
    ResetQuery(reinterpret_cast<hdql_Datum_t>(&ev), r);
    ASSERT_TRUE(r);
    while(r) {
        EXPECT_TRUE(hdql_key_is_empty(hdql_key_get_list_item(_queryKey, 0)));
        EXPECT_EQ(ev.eventID, vi->get_as_int(r));

        AdvanceQuery(r);
        ++nVisited;
    }
    EXPECT_EQ(1, nVisited);
}

//
// Iteration of two-level collection

TEST_F(QueryIterationTest, straightDataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks.hits.x", true);
    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, 3.4},
        {{0, 102, -1}, 4.5},
        {{2, 103, -1}, 5.6},
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);

    IterateResultsOn(ev);
    CheckAllResolved();
};

//
// Selective iteration of two-level collection

TEST_F(QueryIterationTest, selectiveDataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks[2:...].hits[202:...].x", true);
    ExpectedEntry expectedQueryResults[] = {
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);

    IterateResultsOn(ev);
    CheckAllResolved();
};
