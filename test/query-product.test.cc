#include <cmath>
#include <gtest/gtest.h>

#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
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
    hdql_query_destroy(queries[0], _ctx);
    hdql_query_destroy(queries[1], _ctx);
}  // worksOnTwoSeriesWithoutKeys

TEST_F(QueryProductTest, worksOnThreeSeriesWithKeys) {
    int rc;
    const char q1Expr[] = ".tracks[1:5].ndf"
             //    0)  (1), scalar instance 0x5f3deefd5664 of type <int32_t> with value "3
             , q2Expr[] = ".hits[200:400].energyDeposition"
             //    0)  (301), scalar instance 0x61defeb9af20 of type <float> with value "5.000000e+00".
             //    1)  (202), scalar instance 0x61defeb9aee0 of type <float> with value "4.000000e+00".
             , q3Expr[] = ".tracks.hits[100:103].rawData.samples[3]"
             //    0)  (0, 102, 3), value="14".
             //    1)  (0, 101, 3), value="4"
             ;
    hdql_Query * queries[3] = {NULL, NULL, NULL};
    hdql_Datum_t  values[3];
    hdql_CollectionKey * keys[3] = {NULL, NULL, NULL};
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
    // allocate keys storage and their flat views
    rc = hdql_query_keys_reserve(queries[0], keys, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
    size_t flkl1 = hdql_keys_flat_view_size(keys[0], _ctx);
    ASSERT_GT(flkl1, 0);
    ASSERT_EQ(flkl1, 1);
    std::vector<hdql_KeyView> kv1(flkl1, hdql_KeyView{0x0, nullptr, nullptr, nullptr});

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
    // allocate keys storage and their flat views
    rc = hdql_query_keys_reserve(queries[1], keys + 1, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
    size_t flkl2 = hdql_keys_flat_view_size(keys[1], _ctx);
    ASSERT_GT(flkl2, 0);
    ASSERT_EQ(flkl2, 1);
    std::vector<hdql_KeyView> kv2(flkl2, hdql_KeyView{0x0, nullptr, nullptr, nullptr});

    queries[2] = hdql_compile_query( q3Expr
                              , _eventCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    ASSERT_EQ(errDetails[0], 0)
        << "Error compiling \"" << q3Expr << "\": " << errDetails;
    ad = hdql_query_top_attr(queries[2]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi3 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi3 != NULL);
    // allocate keys storage and their flat views
    rc = hdql_query_keys_reserve(queries[2], keys + 2, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
    size_t flkl3 = hdql_keys_flat_view_size(keys[2], _ctx);
    ASSERT_GT(flkl3, 0);
    ASSERT_EQ(flkl3, 3);
    std::vector<hdql_KeyView> kv3(flkl3, hdql_KeyView{0x0, nullptr, nullptr, nullptr});

    // init key views


    // re-set the product, getting first item in a list
    rc = hdql_query_product_reset(queries, keys, values, (hdql_Datum_t) &ev, 3, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "error creating query product";
    
    // build test set: a vector of items that must be found in the cartesian
    // product result
    std::vector<std::tuple<bool, int, int, int, int, int,  int, float, int>> check = {
        {false, 1, 301, 0, 102, 3,      3, 5.0, 14},
        {false, 1, 301, 0, 101, 3,      3, 5.0,  4},

        {false, 1, 202, 0, 102, 3,      3, 4.0, 14},
        {false, 1, 202, 0, 101, 3,      3, 4.0,  4},

        {false, 2, 301, 0, 102, 3,      2, 5.0, 14},
        {false, 2, 301, 0, 101, 3,      2, 5.0,  4},

        {false, 2, 202, 0, 102, 3,      2, 4.0, 14},
        {false, 2, 202, 0, 101, 3,      2, 4.0,  4},
        //   0  1    2  3    4  5       6    7   8
    };
    

    do {
        hdql_keys_flat_view_update(queries[0], keys[0], kv1.data(), _ctx);
        hdql_keys_flat_view_update(queries[1], keys[1], kv2.data(), _ctx);
        hdql_keys_flat_view_update(queries[2], keys[2], kv3.data(), _ctx);

        int k11 = kv1[0].interface->get_as_int(kv1[0].keyPtr->pl.datum);

        int k21 = kv2[0].interface->get_as_int(kv2[0].keyPtr->pl.datum);

        int k31 = kv3[0].interface->get_as_int(kv3[0].keyPtr->pl.datum);
        int k32 = kv3[1].interface->get_as_int(kv3[1].keyPtr->pl.datum);
        int k33 = kv3[2].interface->get_as_int(kv3[2].keyPtr->pl.datum);

        int   v1 = vi1->get_as_int(values[0]);
        float v2 = vi2->get_as_int(values[1]);
        int   v3 = vi3->get_as_int(values[2]);
        // find item in checklist
        bool found = false;
        for(auto & item : check) {
            if(std::get<1>(item) != k11) continue;
            if(std::get<2>(item) != k21) continue;
            if(std::get<3>(item) != k31) continue;
            if(std::get<4>(item) != k32) continue;
            if(std::get<5>(item) != k33) continue;

            if(std::get<6>(item)          != v1) continue;
            if(std::fabs(std::get<7>(item) - v2) > 1e-6) continue;
            if(std::get<8>(item)          != v3) continue;

            found = true;
            ASSERT_FALSE(std::get<0>(item));
            std::get<0>(item) = true;
        }
        ASSERT_TRUE(found)
            << "product yielded combination (k11="
            << k11 << ", k21=" << k21
            << ", k31=" << k31 << ", k32=" << k32 << ", k33=" << k33 << ", "
            << v1 << ", " << v2 << ", " << v3 << ") which is not anticipated";
    } while(HDQL_ERR_CODE_OK == hdql_query_product_advance(queries, keys, values
                , (hdql_Datum_t) &ev, 3, _ctx));
    // check that all combinations passed
    for(const auto & item : check) {
        EXPECT_TRUE(std::get<0>(item));
    }
    hdql_query_destroy(queries[0], _ctx);
    hdql_query_destroy(queries[1], _ctx);
    hdql_query_destroy(queries[2], _ctx);

    hdql_query_keys_destroy(keys[0], _ctx);
    hdql_query_keys_destroy(keys[1], _ctx);
    hdql_query_keys_destroy(keys[2], _ctx);
}  // worksOnThreeSeriesWithKeys

TEST_F(QueryProductTest, worksOnSingleQueryWithoutKeys) {
    const char q1Expr[] = ".hits[1:200].energyDeposition"
             ;
    hdql_Query * queries[1] = {NULL};
    hdql_Datum_t  values[1];
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

    // re-set the product, getting first item in a list
    int rc = hdql_query_product_reset(queries, NULL, values, (hdql_Datum_t) &ev, 1, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "error creating query product";
    
    // build test set: a vector of items that must be found in the cartesian
    // product result
    std::vector<std::tuple<bool, float>> check;
    std::tuple<bool, float> r = {false, 1};
    check.push_back(r);
    r = {false, 2};
    check.push_back(r);
    r = {false, 3};
    check.push_back(r);

    do {
        int v1 = vi1->get_as_int(values[0]);
        // find item in checklist
        bool found = false;
        for(auto & item : check) {
            if(std::get<1>(item) != v1) continue;
            found = true;
            ASSERT_FALSE(std::get<0>(item));
            std::get<0>(item) = true;
        }
        ASSERT_TRUE(found)
            << "product yielded ("
            << v1 << ") which is not anticipated";
    } while(HDQL_ERR_CODE_OK == hdql_query_product_advance(queries, NULL, values
                , (hdql_Datum_t) &ev, 1, _ctx));
    // check that all combinations passed
    for(const auto & item : check) {
        EXPECT_TRUE(std::get<0>(item)) << " item ("
            << std::get<1>(item)
            << ") was not produced by product";
    }
    hdql_query_destroy(queries[0], _ctx);
}  // worksOnSingleQueryWithoutKeys

TEST_F(QueryProductTest, worksOnSingleQueryWithKeys) {
    const char q1Expr[] = ".hits[1:200].energyDeposition"
             ;
    hdql_Query * queries[1] = {NULL};
    hdql_Datum_t  values[1];
    hdql_CollectionKey * keys[1] = {NULL};
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
    // allocate keys storage and their flat views
    int rc = hdql_query_keys_reserve(queries[0], keys, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
    size_t flkl1 = hdql_keys_flat_view_size(keys[0], _ctx);
    ASSERT_GT(flkl1, 0);
    ASSERT_EQ(flkl1, 1);
    std::vector<hdql_KeyView> kv1(flkl1, hdql_KeyView{0x0, nullptr, nullptr, nullptr});

    // re-set the product, getting first item in a list
    rc = hdql_query_product_reset(queries, keys, values, (hdql_Datum_t) &ev, 1, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "error creating query product";
    
    // build test set: a vector of items that must be found in the cartesian
    // product result
    std::vector<std::tuple<bool, int, float>> check;
    std::tuple<bool, int, float> r = {false, 101, 1};
    check.push_back(r);
    r = {false, 102, 2};
    check.push_back(r);
    r = {false, 103, 3};
    check.push_back(r);

    do {
        hdql_keys_flat_view_update(queries[0], keys[0], kv1.data(), _ctx);

        int k11 = kv1[0].interface->get_as_int(kv1[0].keyPtr->pl.datum);
        int v1 = vi1->get_as_int(values[0]);

        // find item in checklist
        bool found = false;
        for(auto & item : check) {
            if(std::get<1>(item) != k11) continue;
            if(std::fabs(std::get<2>(item) - v1) > 1e-6) continue;
            found = true;
            ASSERT_FALSE(std::get<0>(item));
            std::get<0>(item) = true;
        }
        ASSERT_TRUE(found)
            << "product yielded ("
            << v1 << ") which is not anticipated";
    } while(HDQL_ERR_CODE_OK == hdql_query_product_advance(queries, keys, values
                , (hdql_Datum_t) &ev, 1, _ctx));
    // check that all combinations passed
    for(const auto & item : check) {
        EXPECT_TRUE(std::get<0>(item)) << " item ("
            << std::get<1>(item)
            << ") was not produced by product";
    }
    hdql_query_destroy(queries[0], _ctx);
    hdql_query_keys_destroy(keys[0], _ctx);
}  // worksOnSingleQueryWithKeys

TEST_F(QueryProductTest, singleEmptyQueryYieldsEmptySet) {
    const char q1Expr[] = ".hits{:.energyDeposition>1.0e+6}.x"
             ;
    hdql_Query * queries[1] = {NULL};
    hdql_Datum_t  values[1];
    hdql_CollectionKey * keys[1] = {NULL};
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
        << "Error compiling \"" << q1Expr << "\": " << errBuf;
    const hdql_AttrDef * ad = hdql_query_top_attr(queries[0]);
    ASSERT_TRUE(ad != NULL);
    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    hdql_ValueTypeCode_t tc = hdql_attr_def_get_atomic_value_type_code(ad);
    const struct hdql_ValueInterface * vi1 = hdql_types_get_type(_valueTypes, tc);
    ASSERT_TRUE(vi1 != NULL);
    // allocate keys storage and their flat views
    int rc = hdql_query_keys_reserve(queries[0], keys, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
    size_t flkl1 = hdql_keys_flat_view_size(keys[0], _ctx);
    ASSERT_GT(flkl1, 0);
    ASSERT_EQ(flkl1, 1);
    std::vector<hdql_KeyView> kv1(flkl1, hdql_KeyView{0x0, nullptr, nullptr, nullptr});

    // re-set the product, getting first item in a list
    rc = hdql_query_product_reset(queries, keys, values, (hdql_Datum_t) &ev, 1, _ctx);
    ASSERT_EQ(rc, HDQL_ERR_EMPTY_SET);
    hdql_query_keys_destroy(keys[0], _ctx);
    hdql_query_destroy(queries[0], _ctx);
}  // singleEmptyQueryYieldsEmptySet

TEST_F(QueryProductTest, twoQueryYieldsEmptySet) {
    const char q1Expr[] = ".tracks{:.chi2>0.2}.ndf"
             , q2Expr[] = ".hits{:.energyDeposition>1.0e+6}.y"
             ;
    hdql_Query * queries[2] = {NULL, NULL};
    hdql_Datum_t  values[2];
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
    EXPECT_EQ(rc, HDQL_ERR_EMPTY_SET);
    hdql_query_destroy(queries[0], _ctx);
    hdql_query_destroy(queries[1], _ctx);
}  // twoQueryYieldsEmptySet

}  // namespace ::hdql::test
}  // namespace hdql


