#include "../iteration-results.hh"
#include "../samples.hh"
#include "hdql/query-key.h"

#include <gtest/gtest.h>

using hdql::test::QueryIterationTest;

//
// Iteration of virtual compound providing collection

TEST_F(QueryIterationTest, forwardingAttribute_DataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks{h := .hits}.h.energyDeposition", true);

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, 1.},
        {{0, 102, -1}, 2.},
        {{2, 103, -1}, 3.},
        {{2, 202, -1}, 4.},
        {{2, 301, -1}, 5.},
        {{-1}}
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};

//
// Iteration works for two subsequent sub-queries

TEST_F(QueryIterationTest, twoSubqueriesIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks{h := .hits{eDep := .energyDeposition}}.h.eDep", true);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_NE( hdql_attr_def_get_atomic_value_type_code(ad), 0x0 );

    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, 1.},
        {{0, 102, -1}, 2.},
        {{2, 103, -1}, 3.},
        {{2, 202, -1}, 4.},
        {{2, 301, -1}, 5.},
        {{-1}}
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);
    
    CheckAllResolved();
};


//
// Iteration works for virtual compound arithmetics

TEST_F(QueryIterationTest, virtualCompoundArithmeticsWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks.hits{xy := .x*.y, xyz := .xy*.z}.xyz / 2", true);

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_TRUE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    // NOTE: this is nice illustration of subtle type incompatibility wrt C
    // used in <=v0.2 where all fp arithmetic ops resulted in a double.
    // Although this design choice gained some precision, the approach is
    // dubious as it was rather hard to predict and explain its implicit
    // conversion.
    #if 1
    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, ((3.4f*5.6f)*7.8f/2.0f) },
        {{0, 102, -1}, ((4.5f*6.7f)*8.9f/2.0f) },
        {{2, 103, -1}, ((5.6f*7.8f)*9.0f/2.0f) },
        {{2, 202, -1}, ((6.7f*8.9f)*0.5f/2.0f) },
        {{2, 301, -1}, ((7.8f*9.0f)*1.2f/2.0f) },
        {{-1}}
    };
    #else
    // TODO: arithmetic operations seem to break up usual C type promotion
    //       rules making floating point operations of always a double type.
    //       We must re-implement it to maintain compatibility.
    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, (((double) (3.4f*5.6f))*7.8f/2.0f) },
        {{0, 102, -1}, (((double) (4.5f*6.7f))*8.9f/2.0f) },
        {{2, 103, -1}, (((double) (5.6f*7.8f))*9.0f/2.0f) },
        {{2, 202, -1}, (((double) (6.7f*8.9f))*0.5f/2.0f) },
        {{2, 301, -1}, (((double) (7.8f*9.0f))*1.2f/2.0f) },
        {{-1}}
    };
    #endif
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};


//
// Iteration with basic filtering works for virtual compound arithmetics

TEST_F(QueryIterationTest, filteringWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed; however, most of the time for STL container this UT
    // nicely tests complex reset() case, when 2n reset() for hits yileds
    // none making reset() to go back and advance tracks iterator.
    CompileQuery(".tracks{: .chi2/.ndf > 3.4}.hits{: (.energyDeposition < 3.5 || .energyDeposition > 4.5)}.x"
            , true);
    ExpectedEntry expectedQueryResults[] = {
        {{2, 103, -1}, 5.6 },
        {{2, 301, -1}, 7.8 },
        {{-1}}
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};
