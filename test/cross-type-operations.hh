#pragma once 
#include "compiled-query.hh"

namespace hdql {
namespace test {

// Item containing attributes of various types to test type promotion validity
struct Item {
    bool     bf;
    int8_t   i8f;
    uint8_t  u8f;
    int16_t  i16f;
    uint16_t u16f;
    int32_t  i32f;
    uint32_t u32f;
    int64_t  i64f;
    uint64_t u64f;
    float    ff;
    double   df;
};

// Root item compound providing few collections for operations
struct RootItem {
    std::vector<std::shared_ptr<Item>> a, b;
    uint16_t u16f;
};

class TestCrossTypeOperations : public TestCompiledQuery {
protected:
    helpers::CompoundTypes _define_compounds(hdql_Context * context
            , hdql_Compound *& rootCompound) override;
};

}  // namespace ::hdql::test
}  // namespace hdql

