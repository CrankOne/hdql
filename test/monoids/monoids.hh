#pragma once

// Fixture defining compounds meant for testing of the monoid aggregate
// functions, like sum(), max(), sort(), join(), etc.

#include "../cross-type-operations.hh"

#include <gtest/gtest.h>

namespace hdql {
namespace test {

class TestMonoidal : public TestCrossTypeOperations {
public:
    TestMonoidal() {}
    void SetUp() override;
};  // class TestMonoidal

}  // namespace ::hdql::test
}  // namespace hdql

