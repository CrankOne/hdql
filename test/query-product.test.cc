#include "hdql/query-product.h"
#include <cmath>
#include <gtest/gtest.h>

#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "samples.hh"

namespace hdql {
namespace test {

class QueryProductTest : public hdql::test::TestingEventStruct {
public:
    void SetUp() override;
    void TearDown() override;
};

void QueryProductTest::SetUp() {
    hdql::test::TestingEventStruct::SetUp();
}

void QueryProductTest::TearDown() {
    hdql::test::TestingEventStruct::TearDown();
}


TEST_F(QueryProductTest, worksOnTwoSeriesWithoutKeys) {
    const char q1Expr[] = ".tracks{:.chi2>0.2}.ndf"
             , q2Expr[] = ".hits.energyDeposition"
             ;
    hdql_Query * queries[2] = {NULL, NULL};
    hdql_Datum_t  values[2];
    hdql_CollectionKey * keys[2];
    // create and populate object
    hdql::test::Event ev;
    fill_data_sample_1(ev);

    // interpret (compile) queries
    // 1st: result in integer scalar
    char errBuf[128]; int errDetails[5];
    queries[0] = hdql_compile_query( q1Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q1Expr << "\": " << errDetails;
    const hdql_AttrDef * ad = hdql_query_top_attr(queries[0]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    hdql_ValueTypeCode_t tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi1 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi1 != NULL);

    queries[1] = hdql_compile_query( q2Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q2Expr << "\": " << errDetails;
    ad = hdql_query_top_attr(queries[1]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi2 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi2 != NULL);

    // re-set the product, getting first item in a list
    int rc = hdql_query_product_reset(queries, NULL, values, (hdql_Datum_t) &ev, 2, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "error creating query product";
    
    // build test set: a vector of items that must be found in the cartesian
    // product result
    std::vector<std::tuple<bool, int, float>> check;
    for(float eDep = 1; eDep < 5.5;eDep += 1.0) {
        std::tuple<bool, int, float> r = {false, 2, eDep};
        check.push_back(r);
        r = {false, 3, eDep};
        check.push_back(r);
    }

    do {
        int v1 = vi1->get_as_int(values[0]);
        float v2 = vi2->get_as_int(values[1]);
        // find item in checklist
        bool found = false;
        for(auto & item : check) {
            if(std::get<1>(item) != v1) continue;
            if(std::fabs(std::get<2>(item) - v2) > 1e-6) continue;
            found = true;
            ASSERT_FALSE(std::get<0>(item));
            std::get<0>(item) = true;
        }
        ASSERT_TRUE(found)
            << "product yielded combination ("
            << v1 << ", " << v2 << ") which is not anticipated";
    } while(HDQL_ERR_CODE_OK == hdql_query_product_advance(queries, NULL, values
                , (hdql_Datum_t) &ev, 2, _ctx));
    // check that all combinations passed
    for(const auto & item : check) {
        EXPECT_TRUE(std::get<0>(item)) << " item ("
            << std::get<1>(item) << ", " << std::get<2>(item)
            << ") was not produced by product";
    }
}  // worksOnTwoSeriesWithoutKeys

TEST_F(QueryProductTest, worksOnThreeSeriesWithKeys) {
    const char q1Expr[] = ".tracks{:.ndf>2}.ndf"
             //    0)  (1), scalar instance 0x5f3deefd5664 of type <int32_t> with value "3
             , q2Expr[] = ".hits.energyDeposition"
             //    0)  (301), scalar instance 0x61defeb9af20 of type <float> with value "5.000000e+00".
             //    1)  (202), scalar instance 0x61defeb9aee0 of type <float> with value "4.000000e+00".
             , q3Expr[] = "'.tracks.hits[100:103].rawData.samples[3]"
             //    0)  (0, 102, 3), value="14".
             //    1)  (0, 101, 3), value="4"
             ;
    hdql_Query * queries[3] = {NULL, NULL, NULL};
    hdql_Datum_t  values[3];
    hdql_CollectionKey * keys[3];
    // create and populate object
    hdql::test::Event ev;
    fill_data_sample_1(ev);

    // interpret (compile) queries
    // 1st: result in integer scalar
    char errBuf[128]; int errDetails[5];
    queries[0] = hdql_compile_query( q1Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q1Expr << "\": " << errDetails;
    const hdql_AttrDef * ad = hdql_query_top_attr(queries[0]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    hdql_ValueTypeCode_t tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi1 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi1 != NULL);

    queries[1] = hdql_compile_query( q2Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q2Expr << "\": " << errDetails;
    ad = hdql_query_top_attr(queries[1]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi2 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi2 != NULL);

    queries[2] = hdql_compile_query( q3Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q3Expr << "\": " << errDetails;
    ad = hdql_query_top_attr(queries[1]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi3 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi2 != NULL);

    // re-set the product, getting first item in a list
    int rc = hdql_query_product_reset(queries, NULL, values, (hdql_Datum_t) &ev, 2, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "error creating query product";
    
    // build test set: a vector of items that must be found in the cartesian
    // product result
    std::vector<std::tuple<bool, int, int, int, int, int,  int, float, int>> check = {
        {false, 1, 301, 0, 102, 3,      3, 5.0, 14},
        {false, 1, 301, 0, 101, 3,      3, 5.0,  4},

        {false, 1, 202, 0, 102, 3,      3, 4.0, 14},
        {false, 1, 202, 0, 101, 3,      3, 4.0,  4},
    };
    

    do {
        int v1 = vi1->get_as_int(values[0]);
        float v2 = vi2->get_as_int(values[1]);
        // find item in checklist
        bool found = false;
        for(auto & item : check) {
            if(std::get<1>(item) != v1) continue;
            if(std::fabs(std::get<2>(item) - v2) > 1e-6) continue;
            found = true;
            ASSERT_FALSE(std::get<0>(item));
            std::get<0>(item) = true;
        }
        ASSERT_TRUE(found)
            << "product yielded combination ("
            << v1 << ", " << v2 << ") which is not anticipated";
    } while(HDQL_ERR_CODE_OK == hdql_query_product_advance(queries, NULL, values
                , (hdql_Datum_t) &ev, 2, _ctx));
    // check that all combinations passed
    for(const auto & item : check) {
        EXPECT_TRUE(std::get<0>(item)) << " item ("
            << std::get<1>(item) << ", " << std::get<2>(item)
            << ") was not produced by product";
    }
}  // ...

}  // namespace ::hdql::test
}  // namespace hdql

