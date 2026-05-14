#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "monoids.hh"
#include <gtest/gtest.h>
#include <memory>

using ::hdql::test::TestMonoidal;

// Tests basic type preservation/promotion rules
//

TEST_F(TestMonoidal, maxTypeInQueryResultsInAKeylessI32Scalar) {
    using namespace hdql::test;

    CompileQuery("max(.a.i32f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(_query, &keys, _compounds.context_ptr()));

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

    struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t i32tc = hdql_types_get_type_code(types, "int32_t");
    ASSERT_NE(i32tc, 0x0);

    EXPECT_EQ(i32tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, maxTypeInQueryResultsInAKeylessU16Scalar) {
    using namespace hdql::test;

    CompileQuery("max(.b.u16f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(_query, &keys, _compounds.context_ptr()));

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

    struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t u16tc = hdql_types_get_type_code(types, "uint16_t");
    ASSERT_NE(u16tc, 0x0);

    EXPECT_EQ(u16tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, maxTypeInQueryResultsInAPromotedKeylessScalar) {
    using namespace hdql::test;

    CompileQuery("max(.b.u16f, .a.i64f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(_query, &keys, _compounds.context_ptr()));

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

    struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t i64tc = hdql_types_get_type_code(types, "int64_t");
    ASSERT_NE(i64tc, 0x0);

    EXPECT_EQ(i64tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, maxTypeInQueryResultsInAPromotedFloatingPointKeylessScalar) {
    using namespace hdql::test;

    CompileQuery("max(.b.df, .a.i64f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_CollectionKey * keys;
    ASSERT_EQ(0, hdql_query_keys_reserve(_query, &keys, _compounds.context_ptr()));

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

    struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t dbltc = hdql_types_get_type_code(types, "double");
    ASSERT_NE(dbltc, 0x0);

    EXPECT_EQ(dbltc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, maxRefusesBooleanType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("max(.a.bf)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    EXPECT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, maxRefusesCompoundType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("max(.a)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

// Result value tests
//

TEST_F(TestMonoidal, maxOfAnEmptyCollectionIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("max(.a.u32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, maxOfASingleElement) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u32f = 123;
    root.a.push_back(item1);
    CompileQuery("max(.a.u32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(123, vi->get_as_int(r));
}

TEST_F(TestMonoidal, maxOfASingleCollectionArgument) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->i32f = 123;
    root.a.push_back(item1);
    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    item2->i32f = -122;
    root.a.push_back(item2);
    CompileQuery("max(.a.i32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(123, vi->get_as_int(r));
}

TEST_F(TestMonoidal, maxOfEmptyCollectionsIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("max(.a.i32f, .b.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, maxOfCollections) {
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
    CompileQuery("max(.a.i32f, .b.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_EQ(123, vi->get_as_int(r));
}



