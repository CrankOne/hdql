#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "monoids.hh"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

using ::hdql::test::TestMonoidal;

// Tests basic type preservation/promotion rules
//

TEST_F(TestMonoidal, allTypeInQueryResultsInAKeylessBooleanScalar) {
    using namespace hdql::test;

    CompileQuery("all(.a.i32f)");

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
    // ...not a static value
    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));
    // ...has a scalar atomic value iface
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(vi);
    // ...of logic type
    struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
    ASSERT_TRUE(types);
    hdql_ValueTypeCode_t booltc = hdql_types_get_type_code(types, "bool");
    ASSERT_NE(booltc, 0x0);

    EXPECT_EQ(booltc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, allRefusesCompoundType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("all(.a)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, allOfAnEmptyCollectionIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("all(.a.bf)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, allOfASingleTrueElement) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->i32f = -1;
    root.a.push_back(item1);
    CompileQuery("all(.a.i32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_TRUE(vi->get_as_logic(r));
}

TEST_F(TestMonoidal, allOfASingleFalseElement) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u16f = 0x0;
    root.a.push_back(item1);
    CompileQuery("all(.a.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_FALSE(vi->get_as_logic(r));
}

// This behavior is quite general across IEEE-754-based systems.
TEST_F(TestMonoidal, allOfANANIsTrue) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->df = std::numeric_limits<double>::quiet_NaN();
    root.a.push_back(item1);
    CompileQuery("all(.a.df)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_TRUE(vi->get_as_logic(r));
}

TEST_F(TestMonoidal, allOfASequence) {
    using namespace hdql::test;
    RootItem root;

    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u16f = 0x11;
    root.a.push_back(item1);

    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    item2->u16f = 0x0;
    root.a.push_back(item2);

    std::shared_ptr<Item> item3 = std::make_shared<Item>();
    item3->u16f = 0xff;
    root.a.push_back(item3);

    CompileQuery("all(.a.u16f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_FALSE(vi->get_as_logic(r));
}

TEST_F(TestMonoidal, allOfTwoSequences) {
    using namespace hdql::test;
    RootItem root;

    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u16f = 0x11;
    root.a.push_back(item1);

    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    item2->i32f = 0x0;
    root.b.push_back(item2);

    std::shared_ptr<Item> item3 = std::make_shared<Item>();
    item3->i32f = -54;
    root.b.push_back(item3);

    CompileQuery("all(.a.u16f, .b.i32f)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    const hdql_ValueInterface * vi
        = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    EXPECT_FALSE(vi->get_as_logic(r));
}
