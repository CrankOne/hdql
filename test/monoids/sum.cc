#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "monoids.hh"
#include <gtest/gtest.h>
#include <memory>

using ::hdql::test::TestMonoidal;

// Tests basic type preservation/promotion rules
//

TEST_F(TestMonoidal, sumTypeInQueryResultsInAKeylessI32Scalar) {
    using namespace hdql::test;

    CompileQuery("sum(.a.i32f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compoundsContext);
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compoundsContext));

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    // it's a scalar
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    // not a static value
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    // has a scalar atomic value iface
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    struct hdql_ValueTypes * types = hdql_context_get_types(_compoundsContext);
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t i32tc = hdql_types_get_type_code(types, "int32_t");
    ASSERT_NE(i32tc, 0x0);

    EXPECT_EQ(i32tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compoundsContext));
}

TEST_F(TestMonoidal, sumTypeInQueryResultsInAKeylessU16Scalar) {
    using namespace hdql::test;

    CompileQuery("sum(.b.u16f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compoundsContext);
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compoundsContext));

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    // it's a scalar
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    // not a static value
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    // has a scalar atomic value iface
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    struct hdql_ValueTypes * types = hdql_context_get_types(_compoundsContext);
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t u16tc = hdql_types_get_type_code(types, "uint16_t");
    ASSERT_NE(u16tc, 0x0);

    EXPECT_EQ(u16tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compoundsContext));
}

TEST_F(TestMonoidal, sumTypeInQueryResultsInAPromotedKeylessScalar) {
    using namespace hdql::test;

    CompileQuery("sum(.b.u16f, .a.i64f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compoundsContext);
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compoundsContext));

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    // it's a scalar
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    // not a static value
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    // has a scalar atomic value iface
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    struct hdql_ValueTypes * types = hdql_context_get_types(_compoundsContext);
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t i64tc = hdql_types_get_type_code(types, "int64_t");
    ASSERT_NE(i64tc, 0x0);

    EXPECT_EQ(i64tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compoundsContext));
}

TEST_F(TestMonoidal, sumTypeInQueryResultsInAPromotedFloatingPointKeylessScalar) {
    using namespace hdql::test;

    CompileQuery("sum(.b.df, .a.i64f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compoundsContext);
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compoundsContext));

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    // it's a scalar
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    // not a static value
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    // has a scalar atomic value iface
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);

    struct hdql_ValueTypes * types = hdql_context_get_types(_compoundsContext);
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t dbltc = hdql_types_get_type_code(types, "double");
    ASSERT_NE(dbltc, 0x0);

    EXPECT_EQ(dbltc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compoundsContext));
}

TEST_F(TestMonoidal, sumRefusesBooleanType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("sum(.a.bf)", _rootCompound, _compoundsContext
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    EXPECT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, sumRefusesCompoundType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("sum(.a)", _rootCompound, _compoundsContext
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

// Result value tests
//

TEST_F(TestMonoidal, sumOfAnEmptyCollectionIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("sum(.a.u32f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compoundsContext);
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, sumOfASingleElement) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u32f = 123;
    root.a.push_back(item1);
    CompileQuery("sum(.a.u32f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compoundsContext);
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(123, vi->get_as_int(r));
}

TEST_F(TestMonoidal, sumOfASingleCollectionArgument) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->i32f = 123;
    root.a.push_back(item1);
    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    item2->i32f = -122;
    root.a.push_back(item2);
    CompileQuery("sum(.a.i32f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compoundsContext);
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(1, vi->get_as_int(r));
}

TEST_F(TestMonoidal, sumOfEmptyCollectionsIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("sum(.a.i32f, .b.u16f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compoundsContext);
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, sumOfCollections) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->i32f = -120;
    root.a.push_back(item1);
    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    item2->u16f = 123;
    root.b.push_back(item2);
    std::shared_ptr<Item> item3 = std::make_shared<Item>();
    item3->i32f = -13;
    root.a.push_back(item3);
    CompileQuery("sum(.a.i32f, .b.u16f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compoundsContext);
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(-10, vi->get_as_int(r));
}

