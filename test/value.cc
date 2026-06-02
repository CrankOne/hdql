#include "basic-context.hh"
#include "hdql/types.h"

#include "hdql/value.h"

namespace hdql {
namespace test {

#warning "TODO: restore code"
#if 0
TEST(CommonTypes, BasicTypeDifinitionWorks) {
    hdql_ValueTypes vts;
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);
    EXPECT_EQ(vts.get_code_by_name("TypeThree"), 0x0);
    EXPECT_STREQ(vts.get_by_name("TypeOne")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("TypeTwo")->name, iface2.name);
    EXPECT_FALSE(vts.get_by_name("TypeThree"));
}  // TEST(CommonTypes, BasicTypeDifinitionWorks)

TEST(CommonTypes, AliasedTypeDifinitionWorks) {
    hdql_ValueTypes vts;
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
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

TEST(CommonTypes, AliasedTypeDifinitionWithParentWorks) {
    hdql_ValueTypes vts;
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

TEST(CommonTypes, ShortIntegerParsingWorks) {
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

TEST(CommonTypes, ShortIntegerConversionsWorks) {
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

