#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "monoids.hh"
#include <gtest/gtest.h>
#include <memory>

using ::hdql::test::TestMonoidal;

// Tests basic type preservation/promotion rules
//

TEST_F(TestMonoidal, bXORTypeInQueryResultsInAKeylessI32Scalar) {
    using namespace hdql::test;

    CompileQuery("bXOR(.a.i32f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compounds.context_ptr()));

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

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, bXORTypeInQueryResultsInAKeylessU16Scalar) {
    using namespace hdql::test;

    CompileQuery("bXOR(.b.u16f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compounds.context_ptr()));

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

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, bXORTypeInQueryResultsInAPromotedKeylessScalar) {
    using namespace hdql::test;

    CompileQuery("bXOR(.b.u16f, .a.i64f)");

    size_t keysDepth = hdql_query_depth(_query);
    EXPECT_EQ(1, keysDepth);

    // no keys
    hdql_Key * keys = hdql_key_new(_compounds.context_ptr());
    ASSERT_EQ(0, hdql_key_reserve_for_query(_query, keys, _compounds.context_ptr()));

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

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, bXORRefusesFloatingPointType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("bXOR(.a.ff)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, bXORRefusesDoubleType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("bXOR(.a.df)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    EXPECT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, bXORRefusesBooleanType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("bXOR(.a.bf)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    EXPECT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, bXORRefusesCompoundType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("bXOR(.a)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

// Result value tests
//

TEST_F(TestMonoidal, bXOROfAnEmptyCollectionIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("bXOR(.a.u32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, bXOROfASingleElement) {
    using namespace hdql::test;
    RootItem root;
    auto item1 = std::make_shared<Item>();
    item1->u32f = 0xfffae;
    root.a.push_back(item1);
    CompileQuery("bXOR(.a.u32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_EQ( hdql_attr_def_get_atomic_value_type_code(ad)
             , hdql_types_get_type_code(_valueTypes, "uint32_t")
             );
    EXPECT_EQ(0xfffae, *((uint32_t*) r));
}

TEST_F(TestMonoidal, bXOROfASingleCollectionArgument) {
    using namespace hdql::test;
    RootItem root;

    auto item1 = std::make_shared<Item>();
    item1->i32f = 0xff;
    root.a.push_back(item1);

    auto item2 = std::make_shared<Item>();
    item2->i32f = 0x1e;
    root.a.push_back(item2);

    CompileQuery("bXOR(.a.i32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_EQ( hdql_attr_def_get_atomic_value_type_code(ad)
             , hdql_types_get_type_code(_valueTypes, "int32_t")
             );
    EXPECT_EQ((int32_t)(0xff ^ 0x1e), *((int32_t*) r));
}

TEST_F(TestMonoidal, bXOROfRepeatedElementsCancel) {
    using namespace hdql::test;
    RootItem root;

    auto item1 = std::make_shared<Item>();
    item1->i32f = 0xff;
    root.a.push_back(item1);

    auto item2 = std::make_shared<Item>();
    item2->i32f = 0x1e;
    root.a.push_back(item2);

    auto item3 = std::make_shared<Item>();
    item3->i32f = 0xff;
    root.a.push_back(item3);

    CompileQuery("bXOR(.a.i32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_EQ( hdql_attr_def_get_atomic_value_type_code(ad)
             , hdql_types_get_type_code(_valueTypes, "int32_t")
             );
    EXPECT_EQ(0x1e, *((int32_t*) r));
}

TEST_F(TestMonoidal, bXOROfEmptyCollectionsIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("bXOR(.a.i32f, .b.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, bXOROfCollections) {
    using namespace hdql::test;
    RootItem root;

    auto item1 = std::make_shared<Item>();
    item1->i32f = 0xff;
    root.a.push_back(item1);

    auto item2 = std::make_shared<Item>();
    item2->u16f = 0xff00;
    root.b.push_back(item2);

    auto item3 = std::make_shared<Item>();
    item3->i32f = 0xff0000;
    root.a.push_back(item3);

    CompileQuery("bXOR(.a.i32f, .b.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0xffffff, vi->get_as_int(r));
}

TEST_F(TestMonoidal, bXOROfCollectionsWithCancellationAcrossArguments) {
    using namespace hdql::test;
    RootItem root;

    auto item1 = std::make_shared<Item>();
    item1->i32f = 0xff;
    root.a.push_back(item1);

    auto item2 = std::make_shared<Item>();
    item2->i64f = 0xff;
    root.b.push_back(item2);

    auto item3 = std::make_shared<Item>();
    item3->i32f = 0x1e;
    root.a.push_back(item3);

    CompileQuery("bXOR(.a.i32f, .b.i64f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);

    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_EQ( hdql_attr_def_get_atomic_value_type_code(ad)
             , hdql_types_get_type_code(_valueTypes, "int64_t")
             );

    EXPECT_EQ((int64_t)0x1e, *((int64_t*) r));
}
