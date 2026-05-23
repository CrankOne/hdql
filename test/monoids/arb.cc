#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "monoids.hh"
#include <gtest/gtest.h>
#include <memory>

using ::hdql::test::TestMonoidal;

// Tests basic type preservation/promotion rules
//

TEST_F(TestMonoidal, narbTypeInQueryResultsInAKeylessInheritedScalar) {
    using namespace hdql::test;

    CompileQuery("narb(.a.i32f)");

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
    hdql_ValueTypeCode_t i32tc = hdql_types_get_type_code(types, "int32_t");
    ASSERT_NE(i32tc, 0x0);

    EXPECT_EQ(i32tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

TEST_F(TestMonoidal, narbRefusesCompoundType) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("narb(.a)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

TEST_F(TestMonoidal, narbFromAnEmptyCollectionIsNone) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("narb(.a.bf)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
    ASSERT_FALSE(r);
}

TEST_F(TestMonoidal, narbOfASingleElement) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->i32f = -123;
    root.a.push_back(item1);
    CompileQuery("narb(.a.i32f)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    EXPECT_EQ(*((int32_t *) r), -123);
}

TEST_F(TestMonoidal, narbOfASequence) {
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

    std::unordered_map<uint16_t, size_t> counts;
    CompileQuery("narb(.a.u16f)");
    for(int i = 0; i < 100; ++i) {
        hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
        ASSERT_TRUE(r);
        const uint16_t val = *((uint16_t *) r);
        if(counts.end() == counts.find(val)) {
            counts.emplace(val, 1);
        } else {
            ++counts[val];
        }
    }
    EXPECT_EQ(counts.size(), 3);
    ASSERT_FALSE(counts.find(0x0)  == counts.end());
    ASSERT_FALSE(counts.find(0x11) == counts.end());
    ASSERT_FALSE(counts.find(0xff) == counts.end());
    EXPECT_GT(counts[0x0 ], 10);
    EXPECT_GT(counts[0x11], 10);
    EXPECT_GT(counts[0xff], 10);
}

TEST_F(TestMonoidal, narbOfTwoSequences) {
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

    CompileQuery("narb(.a.u16f, .b.i32f)");
    std::unordered_map<int32_t, size_t> counts;
    for(int i = 0; i < 100; ++i) {
        hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
        ASSERT_TRUE(r);
        const int32_t val = *((int32_t *) r);
        if(counts.end() == counts.find(val)) {
            counts.emplace(val, 1);
        } else {
            ++counts[val];
        }
    }
    EXPECT_EQ(counts.size(), 3);
    ASSERT_FALSE(counts.find(0x0)  == counts.end());
    ASSERT_FALSE(counts.find(0x11) == counts.end());
    ASSERT_FALSE(counts.find(-54) == counts.end());
    EXPECT_GT(counts[0x0 ], 10);
    EXPECT_GT(counts[0x11], 10);
    EXPECT_GT(counts[ -54], 10);
}
