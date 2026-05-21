#pragma once

#include <gtest/gtest.h>

struct hdql_Context;
struct hdql_AttrDef;
struct hdql_Query;

namespace hdql {
namespace test {

class BasicAttrDefQuery : public ::testing::Test {
protected:
    hdql_Context * _context;
    hdql_AttrDef * _ad;
    hdql_Query * _q;
public:
    BasicAttrDefQuery() : _context(nullptr), _q(nullptr) {}
    void SetUp() override;
    void TearDown() override;
};

}
}

