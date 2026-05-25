#pragma once

#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/operations.h"

#include <gtest/gtest.h>

namespace hdql {
namespace test {

class TestingContext : public ::testing::Test {
protected:
    hdql_Context_t _context;
    hdql_ValueTypes * _valueTypes;
    hdql_Operations * _operations;
public:
    TestingContext()
            : _valueTypes(nullptr)
            , _operations(nullptr)
            {}

    void SetUp() override;
    void TearDown() override;
};

}  // namespace ::hdql::test
}  // namespace hdql
