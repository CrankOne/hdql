#pragma once

#include <gtest/gtest.h>

struct hdql_Context;
struct hdql_AttrDef;
struct hdql_Query;

namespace hdql {
namespace test {

class BasicQuery : public ::testing::Test {
protected:
    hdql_Context * _context;
    hdql_AttrDef * _ad;
    hdql_Query * _q;
public:
    BasicQuery() : _context(nullptr), _q(nullptr) {}
    void SetUp() override;
    void TearDown() override;
};

}
}

