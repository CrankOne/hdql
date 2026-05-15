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

TEST_F(TestMonoidal, lenTypeInQueryResultsInAKeylessU64Scalar) {
    using namespace hdql::test;

    CompileQuery("len(.a)");

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
    hdql_ValueTypeCode_t ui64tc = hdql_types_get_type_code(types, "uint64_t");
    ASSERT_NE(ui64tc, 0x0);

    EXPECT_EQ(ui64tc, hdql_attr_def_get_atomic_value_type_code(ad));

    EXPECT_EQ(0, hdql_query_keys_destroy(keys, _compounds.context_ptr()));
}

// TODO this UT is ok, but the parser failure leaves `.a` query un-deleted
// See #20
//TEST_F(TestMonoidal, lenRefusesScalarType) {
//    using namespace hdql::test;
//    RootItem root;
//    char errBuf[128]; int errDetails[5];
//    _query = hdql_compile_query(".a{l:=len(.bf)}.l", _rootCompound, _compounds.context_ptr()
//            , errBuf, sizeof(errBuf), errDetails );
//    EXPECT_FALSE(_query);
//    EXPECT_EQ( errDetails[0]
//             , HDQL_ERR_TRANSLATION_FAILURE
//             );
//}

TEST_F(TestMonoidal, lenApplicableToScalarAttrOfACollection) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("len(.a.bf)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    ASSERT_TRUE(_query);
    ASSERT_EQ( errDetails[0], HDQL_ERR_CODE_OK);
}

TEST_F(TestMonoidal, lenRefusesMultipleArguments) {
    using namespace hdql::test;
    RootItem root;
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query("len(.a, .b)", _rootCompound, _compounds.context_ptr()
            , errBuf, sizeof(errBuf), errDetails );
    EXPECT_FALSE(_query);
    ASSERT_EQ( errDetails[0]
             , HDQL_ERR_TRANSLATION_FAILURE
             );
}

// Result value tests
//

TEST_F(TestMonoidal, lenOfAnEmptyCollectionIsZero) {
    using namespace hdql::test;
    RootItem root;
    CompileQuery("len(.a)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_EQ(*((uint64_t*) r), 0);
}

TEST_F(TestMonoidal, lenOfASingleElementInACollectionIsOne) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    item1->u32f = 123;
    root.a.push_back(item1);
    CompileQuery("len(.a)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    EXPECT_EQ(1, *((uint64_t*) r));
}

TEST_F(TestMonoidal, lenOfACollection) {
    using namespace hdql::test;
    RootItem root;
    std::shared_ptr<Item> item1 = std::make_shared<Item>();
    root.a.push_back(item1);
    std::shared_ptr<Item> item2 = std::make_shared<Item>();
    root.a.push_back(item2);
    std::shared_ptr<Item> item3 = std::make_shared<Item>();
    root.a.push_back(item2);
    CompileQuery("len(.a)");
    hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(&root), _ctx);
    hdql_Datum_t r = hdql_query_get(_query, NULL, _compounds.context_ptr());
    ASSERT_TRUE(r);
    EXPECT_EQ(3, *((uint64_t*) r));
}

