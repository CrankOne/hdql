#pragma once

// Fixture defining compounds meant for testing of the monoid aggregate
// functions, like sum(), max(), sort(), join(), etc.

#include "../basic-context.hh"

#include "hdql/helpers/compounds.hh"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

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
};

class TestAggFuncs : public TestingContext {
protected:
    hdql::helpers::CompoundTypes _compounds;
    hdql_Compound * _rootCompound;
    hdql_Query * _query;
protected:
    TestAggFuncs()
            : _compounds(nullptr)
            , _rootCompound(nullptr)
            {}
    void SetUp() override;

    void CompileQuery(const char *);

    void TearDown() override;
};  // class TestAggFuncs

}  // namespace ::hdql::test
}  // namespace hdql

