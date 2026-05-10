#include "basic-context.hh"
#include "hdql/types.h"

#include "hdql/value.h"

namespace hdql {
namespace test {

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

