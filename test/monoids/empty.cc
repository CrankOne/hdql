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

TEST_F(TestMonoidal, emptyTypeInQueryResultsInAKeylessBooleanScalar) {
    using namespace hdql::test;

    CompileQuery("empty(.a)");

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
    hdql_ValueTypeCode_t booltc = hdql_types_get_type_code(types, "bool");
    ASSERT_NE(booltc, 0x0);

    EXPECT_EQ(booltc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_key_destroy(keys, _compounds.context_ptr()));
}

// TODO this UT is ok, but the parser failure leaves `.a` query un-deleted
// See #20
//TEST_F(TestMonoidal, emptyRefusesScalarType) {
//    using namespace hdql::test;
//    RootItem root;
//    char errBuf[128]; int errDetails[5];
//    _query = hdql_compile_query(".a{l:=empty(.bf)}.l", _rootCompound, _compounds.context_ptr()
//            , errBuf, sizeof(errBuf), errDetails );
//    EXPECT_FALSE(_query);
//    EXPECT_EQ( errDetails[0]
//             , HDQL_ERR_TRANSLATION_FAILURE
//             );
//}

TEST_F(TestMonoidal, emptyApplicableToScalarAttrOfACollection) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("empty(.a.bf)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    ASSERT_TRUE(_query);
    ASSERT_EQ( errDetails[0], HDQL_ERR_CODE_OK);
}

TEST_F(TestMonoidal, emptyRefusesMultipleArguments) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("empty(.a, .b)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

// Result value tests
//

TEST_F(TestMonoidal, emptyOfAnEmptyCollectionIsTrue) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("empty(.a)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
    ASSERT_TRUE(*((bool*) r));
}

TEST_F(TestMonoidal, emptyOfACollectionWithOneElementIsFalse) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u32f = 123;
    root.a.push_back(item1);
    CompileQuery("empty(.a)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    EXPECT_FALSE(*((bool*) r));
}

TEST_F(TestMonoidal, emptyOfANonEmptyCollectionIsFalse) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    root.a.push_back(item1);
    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    root.a.push_back(item2);
    std::shared_ptr<Item> item3 = std::make_shared<Item>();
    root.a.push_back(item2);
    CompileQuery("empty(.a)");
    hdql_Datum_t r = hdql_query_reset(_query
            , reinterpret_cast<hdql_Datum_t>(&root), NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    EXPECT_FALSE(*((bool*) r));
}

