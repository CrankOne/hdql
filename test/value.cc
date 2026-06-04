#include "basic-context.hh"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include <gtest/gtest.h>

#include "hdql/value.h"

namespace hdql {
namespace test {

class Types : public TestingContext {
protected:
    hdql_ValueTypes *_vts;
public:
    void SetUp() override {
        TestingContext::SetUp();
        _vts = hdql_context_get_types(_context);
        ASSERT_TRUE(_vts);
    }
};

TEST_F(Types, canDefineAType) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    int rc = hdql_types_define(_vts, &iface, NULL);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
}

TEST_F(Types, cantDefineATypeWithEmptyName) {
    hdql_ValueInterface iface = {.name = NULL, .size = 42};
    int rc = hdql_types_define(_vts, &iface, NULL);
    ASSERT_EQ(rc, HDQL_ERR_BAD_ARGUMENT);

    iface.name = "";
    hdql_ValueTypeCode_t tc = 0x0;
    rc = hdql_types_define(_vts, &iface, &tc);
    ASSERT_EQ(rc, HDQL_ERR_BAD_ARGUMENT);
    EXPECT_EQ(tc, 0x0);
}

TEST_F(Types, canRetrieveADefinedTypeByCode) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(_vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    EXPECT_NE(tc, 0x0);

    const hdql_ValueInterface *ifacePtr = hdql_types_get_type(_vts, tc);
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(ifacePtr->size, iface.size);
}

TEST_F(Types, canRetrieveADefinedTypeByName) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    int rc = hdql_types_define(_vts, &iface, NULL);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    
    const hdql_ValueInterface *ifacePtr = hdql_types_get_type_by_name(_vts, "TypeOne");
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(ifacePtr->size, iface.size);
}

TEST_F(Types, canRetrieveADefinedTypeByAlias) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(_vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    ASSERT_NE(tc, 0x0);
    
    rc = hdql_types_alias(_vts, "TypeUno", tc);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);

    const hdql_ValueInterface * ifacePtr = hdql_types_get_type_by_name(_vts, "TypeUno");
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(iface.size, ifacePtr->size);
}


TEST_F(Types, canRetrieveADefinedTypeByCodeFromParent) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(_vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    EXPECT_NE(tc, 0x0);

    hdql_Context_t descContext
        = hdql_context_create_descendant(_context, HDQL_CTX_PRINT_PUSH_ERROR);
    ASSERT_TRUE(descContext);

    hdql_ValueTypes *vts = hdql_context_get_types(descContext);
    ASSERT_TRUE(vts);

    const hdql_ValueInterface *ifacePtr = hdql_types_get_type(vts, tc);
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(ifacePtr->size, iface.size);

    hdql_context_destroy(descContext);
}

TEST_F(Types, canRetrieveADefinedTypeByNameFromParent) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(_vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    EXPECT_NE(tc, 0x0);

    hdql_Context_t descContext
        = hdql_context_create_descendant(_context, HDQL_CTX_PRINT_PUSH_ERROR);
    ASSERT_TRUE(descContext);

    hdql_ValueTypes *vts = hdql_context_get_types(descContext);
    ASSERT_TRUE(vts);

    const hdql_ValueInterface *ifacePtr = hdql_types_get_type_by_name(vts, "TypeOne");
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(ifacePtr->size, iface.size);

    hdql_context_destroy(descContext);
}

TEST_F(Types, canRetrieveADefinedTypeByNameFromParentByAliasInChild) {
    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(_vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    EXPECT_NE(tc, 0x0);

    hdql_Context_t descContext
        = hdql_context_create_descendant(_context, HDQL_CTX_PRINT_PUSH_ERROR);
    ASSERT_TRUE(descContext);

    hdql_ValueTypes *vts = hdql_context_get_types(descContext);
    ASSERT_TRUE(vts);

    rc = hdql_types_alias(vts, "TypeUno", tc);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK);

    const hdql_ValueInterface *ifacePtr = hdql_types_get_type_by_name(vts, "TypeUno");
    ASSERT_TRUE(ifacePtr);
    EXPECT_STREQ(ifacePtr->name, iface.name);
    EXPECT_EQ(ifacePtr->size, iface.size);

    hdql_context_destroy(descContext);
}

TEST_F(Types, cantRetrieveADefinedTypeByCodeFromChild) {
    hdql_Context_t descContext
        = hdql_context_create_descendant(_context, HDQL_CTX_PRINT_PUSH_ERROR);
    ASSERT_TRUE(descContext);

    hdql_ValueTypes *vts = hdql_context_get_types(descContext);
    ASSERT_TRUE(vts);

    hdql_ValueInterface iface = {.name = "TypeOne", .size = 42};
    hdql_ValueTypeCode_t tc = 0x0;
    int rc = hdql_types_define(vts, &iface, &tc);
    EXPECT_EQ(rc, HDQL_ERR_CODE_OK) << hdql_err_str(rc);
    EXPECT_NE(tc, 0x0);

    const hdql_ValueInterface *ifacePtr = hdql_types_get_type(_vts, tc);
    ASSERT_FALSE(ifacePtr);

    hdql_context_destroy(descContext);
}

#if 0
TEST_F(TestingContext, BasicTypeDifinitionWorksCheckTwoTypes) {
    hdql_ValueTypes *vts = hdql_context_get_types(_context);
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(hdql_types_define(vts, &iface1), HDQL_ERR_CODE_OK);
    EXPECT_EQ(hdql_types_define(vts, &iface2), HDQL_ERR_CODE_OK);
    EXPECT_NE(hdql_types_get_type_code(vts, "TypeOne"), 0x0);
    EXPECT_NE(hdql_types_get_type_code(vts, "TypeTwo"), 0x0);
    EXPECT_EQ(hdql_types_get_type_code(vts, "TypeThree"), 0x0);
    ASSERT_TRUE(hdql_types_get_type_by_name(vts, "TypeOne"));
    EXPECT_STREQ(hdql_types_get_type_by_name(vts, "TypeOne")->name, iface1.name);
    EXPECT_STREQ(hdql_types_get_type_by_name(vts, "TypeTwo")->name, iface2.name);
    EXPECT_FALSE(hdql_types_get_type_by_name(vts, "TypeThree"));
}  // TEST(CommonTypes, BasicTypeDifinitionWorks)

TEST_F(TestingContext, AliasedTypeDifinitionWorks) {
    hdql_ValueTypes *vts = hdql_context_get_types(_context);
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(hdql_types_define(vts, &iface1), HDQL_ERR_CODE_OK);
    EXPECT_EQ(hdql_types_define(vts, &iface2), HDQL_ERR_CODE_OK);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);

    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne"),  0);
    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne2"), 0);
    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeTwo"), "AliasTwo"),  0);

    EXPECT_EQ(vts.get_code_by_name("TypeThree"), 0x0);
    EXPECT_STREQ(vts.get_by_name("TypeOne")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("TypeTwo")->name, iface2.name);
    EXPECT_FALSE(vts.get_by_name("TypeThree"));

    EXPECT_STREQ(vts.get_by_name("AliasOne")->name,  iface1.name);
    EXPECT_STREQ(vts.get_by_name("AliasOne2")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("AliasTwo")->name,  iface2.name);
}  // TEST(CommonTypes, AliasedTypeDifinitionWorks)

TEST_F(TestingContext, AliasedTypeDifinitionWithParentWorks) {
    hdql_ValueTypes *vts = hdql_context_get_types(_context);
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);

    hdql_ValueTypes vtsDesc(&vts);
    hdql_ValueInterface iface3 = {.name = "TypeTwo"}  // overrides parent's
                      , iface4 = {.name = "TypeThreeInDesc"};
    EXPECT_EQ(vtsDesc.define(iface3), 0);
    EXPECT_EQ(vtsDesc.define(iface4), 0);

    // it is legal to define alias to parent's type in children
    EXPECT_EQ(vtsDesc.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne"),  0);
    EXPECT_EQ(vtsDesc.define_alias(vtsDesc.get_code_by_name("TypeTwo"), "AliasTwo"),  0);
    
    EXPECT_NE(vts.get_by_name("TypeTwo"), vtsDesc.get_by_name("TypeTwo"));
    EXPECT_STREQ(vtsDesc.get_by_name("TypeTwo")->name, vtsDesc.get_by_name("TypeTwo")->name);

    EXPECT_NE(vtsDesc.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vtsDesc.get_code_by_name("TypeTwo"), 0x0);
    EXPECT_EQ(vtsDesc.get_code_by_name("TypeThree"), 0x0);
    EXPECT_NE(vtsDesc.get_code_by_name("TypeTwo"), vts.get_code_by_name("TypeTwo"));
}  // TEST(CommonTypes, BasicTypeDifinitionWorks)

TEST_F(TestingContext, ShortIntegerParsingWorks) {
    // tests that basic parsing works and detects value boundaries (leving
    // original value intact)
    int16_t v;
    int rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "32767", NULL);
    EXPECT_EQ(rc, 0);  // ok
    EXPECT_EQ(v, 32767);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "32768", NULL);
    EXPECT_NE(rc, 0);  // overflow
    EXPECT_EQ(v, 32767);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "-32768", NULL);
    EXPECT_EQ(rc, 0);  // ok
    EXPECT_EQ(v, -32768);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "-32769", NULL);
    EXPECT_NE(rc, 0);  // underflow
    EXPECT_EQ(v, -32768);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "345foo", NULL);
    EXPECT_NE(rc, 0);  // extra symbols on the tail (considered as error)
    EXPECT_EQ(v, -32768);
}

TEST_F(TestingContext, ShortIntegerConversionsWorks) {
    // tests that basic conversions works and detects value boundaries (leving
    // original value intact)
    int16_t v = std::numeric_limits<int16_t>::max();
    char bf[32];
    StdArithInterface<int16_t>::get_as_string(reinterpret_cast<hdql_Datum_t>(&v)
            , bf, sizeof(bf), NULL);
    EXPECT_STREQ(bf, "32767");
    // TODO: ... set as over-/underflow
}
#endif

#define _M_for_each_arithmetic_pair(m) \
    m( int8_t,          Int8,   int16_t,            Int16,      int16_t,            Int16)  \
    m( uint8_t,         UInt8,  uint16_t,           UInt16,     uint16_t,           UInt16) \
    m( int16_t,         Int16,  uint16_t,           UInt16,     int32_t,            Int32)  \
    m( unsigned char,   UChar,  unsigned short,     UShort,     unsigned short,     UShort) \
    m( unsigned char,   UChar,  double,             Double,     double,             Double) \
    /* ... */

#define _M_implement_case(lTp, lTpName, uTp, uTpName, rTp, rTpName) \
TEST_F(TestingContext, lTpName ## And ## uTpName ## PromotedTo ## uTpName) { \
    hdql_ValueTypeCode_t  \
          lowerType  = hdql_types_get_type_code(_valueTypes, # lTp)  \
        , upperType  = hdql_types_get_type_code(_valueTypes, # uTp)  \
        , resultType = hdql_types_get_type_code(_valueTypes, # rTp)  \
        ; \
    ASSERT_NE(lowerType,  0) << "missing type \"" # lTp "\""; \
    ASSERT_NE(upperType,  0) << "missing type \"" # uTp "\""; \
    ASSERT_NE(resultType, 0) << "missing type \"" # rTp "\""; \
    hdql_ValueTypeCode_t \
          promotedType1 = hdql_types_numeric_promote(_valueTypes, lowerType, upperType)  \
        , promotedType2 = hdql_types_numeric_promote(_valueTypes, upperType, lowerType); \
    EXPECT_EQ(promotedType1, resultType) << "\"" # lTp "\" and \"" # uTp "\" aren't promoted to \"" #rTp "\"";  \
    EXPECT_EQ(promotedType2, resultType) << "\"" # uTp "\" and \"" # lTp "\" aren't promoted to \"" #rTp "\"";  \
}

_M_for_each_arithmetic_pair(_M_implement_case);

TEST_F(TestingContext, LogicTypeIsNotPromotedToNumeric) {
    hdql_ValueTypeCode_t
          lowerType  = hdql_types_get_type_code(_valueTypes, "bool")
        , upperType  = hdql_types_get_type_code(_valueTypes, "int");
    ASSERT_NE(lowerType,  0) << "missing type \"bool\"";
    ASSERT_NE(upperType,  0) << "missing type \"int\"";
    hdql_ValueTypeCode_t
          promotedType1 = hdql_types_numeric_promote(_valueTypes, lowerType, upperType)
        , promotedType2 = hdql_types_numeric_promote(_valueTypes, upperType, lowerType);
    EXPECT_EQ(promotedType1, 0x0);
    EXPECT_EQ(promotedType2, 0x0);
}

}  // namespace ::hdql::test
}  // namespace hdql

