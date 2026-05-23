#include "../iteration-results.hh"
#include "../samples.hh"

#include <gtest/gtest.h>

using hdql::test::QueryIterationTest;

TEST_F(QueryIterationTest, boundAttribute_DataIterationWorksOnSample1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery(".tracks{h := *.hits}.h.energyDeposition", true);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    ExpectedEntry expectedQueryResults[] = {
        {{0, 101, -1}, 1.},
        {{0, 102, -1}, 2.},
        {{2, 103, -1}, 3.},
        {{2, 202, -1}, 4.},
        {{2, 301, -1}, 5.},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};

TEST_F(QueryIterationTest, boundAttribute_DataIterationWorksAsCartesianProduct) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery("{th:=*.tracks.hits, h:=*.hits}{dx:=.h.x-.th.x}.dx", true);

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    ExpectedEntry expectedQueryResults[] = {
        {{101, 0, 101, -1}, 3.4 - 3.4},
        {{102, 0, 101, -1}, 4.5 - 3.4},
        {{103, 0, 101, -1}, 5.6 - 3.4},
        {{202, 0, 101, -1}, 6.7 - 3.4},
        {{301, 0, 101, -1}, 7.8 - 3.4},

        {{101, 0, 102, -1}, 3.4 - 4.5},
        {{102, 0, 102, -1}, 4.5 - 4.5},
        {{103, 0, 102, -1}, 5.6 - 4.5},
        {{202, 0, 102, -1}, 6.7 - 4.5},
        {{301, 0, 102, -1}, 7.8 - 4.5},

        {{101, 2, 103, -1}, 3.4 - 5.6},
        {{102, 2, 103, -1}, 4.5 - 5.6},
        {{103, 2, 103, -1}, 5.6 - 5.6},
        {{202, 2, 103, -1}, 6.7 - 5.6},
        {{301, 2, 103, -1}, 7.8 - 5.6},

        {{101, 2, 202, -1}, 3.4 - 6.7},
        {{102, 2, 202, -1}, 4.5 - 6.7},
        {{103, 2, 202, -1}, 5.6 - 6.7},
        {{202, 2, 202, -1}, 6.7 - 6.7},
        {{301, 2, 202, -1}, 7.8 - 6.7},

        {{101, 2, 301, -1}, 3.4 - 7.8},
        {{102, 2, 301, -1}, 4.5 - 7.8},
        {{103, 2, 301, -1}, 5.6 - 7.8},
        {{202, 2, 301, -1}, 6.7 - 7.8},
        {{301, 2, 301, -1}, 7.8 - 7.8},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);
    
    CheckAllResolved();
};


TEST_F(QueryIterationTest, boundAttribute_DataIterationWorksAsCartesianProductOfFiltered) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery("{th:=*.tracks.hits{:.x > 5.7}, h:=*.hits{:.x < 5.7}}{dx:=.h.x-.th.x}.dx", true);
    
    ExpectedEntry expectedQueryResults[] = {
        {{101, 2, 202, -1}, 3.4 - 6.7},
        {{102, 2, 202, -1}, 4.5 - 6.7},
        {{103, 2, 202, -1}, 5.6 - 6.7},

        {{101, 2, 301, -1}, 3.4 - 7.8},
        {{102, 2, 301, -1}, 4.5 - 7.8},
        {{103, 2, 301, -1}, 5.6 - 7.8},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};


TEST_F(QueryIterationTest, boundAttribute_FilteredDataIterationWorksAsCartesianProduct_1) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery("{th:=*.tracks.hits, h:=*.hits : .h.x > .th.x}{dx:=.h.x-.th.x}.dx", true);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    
    ExpectedEntry expectedQueryResults[] = {
        {{102, 0, 101, -1}, 4.5 - 3.4},
        {{103, 0, 101, -1}, 5.6 - 3.4},
        {{202, 0, 101, -1}, 6.7 - 3.4},
        {{301, 0, 101, -1}, 7.8 - 3.4},

        {{103, 0, 102, -1}, 5.6 - 4.5},
        {{202, 0, 102, -1}, 6.7 - 4.5},
        {{301, 0, 102, -1}, 7.8 - 4.5},

        {{202, 2, 103, -1}, 6.7 - 5.6},
        {{301, 2, 103, -1}, 7.8 - 5.6},

        {{301, 2, 202, -1}, 7.8 - 6.7},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};

TEST_F(QueryIterationTest, boundAttribute_FilteredDataIterationWorksAsCartesianProduct_2) {
    // note, that since hits in track are in unordered_map, particular order is
    // not guaranteed
    CompileQuery("{th:=*.tracks.hits, h:=*.hits : .th.x > .h.x}{dx:=.th.x-.h.x}.dx", true);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE( ad );
    ASSERT_FALSE( hdql_attr_def_is_collection(ad) );
    ASSERT_TRUE( hdql_attr_def_is_atomic(ad) );
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    ExpectedEntry expectedQueryResults[] = {
        {{101, 0, 102, -1},  4.5 - 3.4},

        {{101, 2, 103, -1},  5.6 - 3.4},
        {{102, 2, 103, -1},  5.6 - 4.5},

        {{101, 2, 202, -1},  6.7 - 3.4},
        {{102, 2, 202, -1},  6.7 - 4.5},
        {{103, 2, 202, -1},  6.7 - 5.6},

        {{101, 2, 301, -1},  7.8 - 3.4},
        {{102, 2, 301, -1},  7.8 - 4.5},
        {{103, 2, 301, -1},  7.8 - 5.6},
        {{202, 2, 301, -1},  7.8 - 6.7},
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    hdql::test::Event ev;
    fill_data_sample_1(ev);
    IterateResultsOn(ev);

    CheckAllResolved();
};

