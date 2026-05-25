#pragma once

#include "../basic-context.hh"

#include <gtest/gtest.h>

struct hdql_Context;
struct hdql_AttrDef;
struct hdql_Query;

namespace hdql {
namespace test {

class BasicQuery : public TestingContext {
protected:
    hdql_AttrDef * _ad;
    hdql_Query * _q;
public:
    BasicQuery() : _q(nullptr) {}
    void SetUp() override;
    void TearDown() override;
};

}
}

