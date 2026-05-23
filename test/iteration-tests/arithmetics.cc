#include "../iteration-results.hh"
#include "../samples.hh"

#include <gtest/gtest.h>

using hdql::test::QueryIterationTest;

//
// Iteration with basic filtering works for virtual compound arithmetics

TEST_F(QueryIterationTest, iterationWorksOnDifferentLevelsOfRootObject) {
    CompileQuery(".hits.rawData.time + .eventID", true);

    ExpectedEntry expectedQueryResults[] = {
        {{101, -1}, .01 + 104501 },
        {{102, -1}, .02 + 104501 },
        {{103, -1}, .03 + 104501 },
        {{301, -1}, .05 + 104501 },

        {{401, -1}, .10 + 104502 },
        {{402, -1}, .11 + 104502 },
        {{501, -1}, .12 + 104502 },
        {{502, -1}, .15 + 104502 },

        {{10, -1}, .21 + 104503 },
        {{11, -1}, .23 + 104503 },
        {{12, -1}, .24 + 104503 },
        {{99, -1}, .25 + 104503 },
        {{-1}}  // sentinel
    };
    SetExpectations(expectedQueryResults);

    {
        hdql::test::Event event;
        hdql::test::fill_data_sample_1(event);
        IterateResultsOn(event);
    }

    {
        hdql::test::Event event;
        hdql::test::fill_data_sample_2(event);
        IterateResultsOn(event);
    }

    {
        hdql::test::Event event;
        hdql::test::fill_data_sample_3(event);
        IterateResultsOn(event);
    }

    CheckAllResolved();
};

