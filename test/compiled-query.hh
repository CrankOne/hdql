#pragma once

#include "basic-context.hh"

#include "hdql/helpers/compounds.hh"
#include "hdql/compound.h"

#include "basic-context.hh"

namespace hdql {
namespace test {

class TestCompiledQuery : public TestingContext {
protected:
    hdql::helpers::CompoundTypes _compounds;

    virtual helpers::CompoundTypes _define_compounds(hdql_Context *) = 0;
public:
    TestCompiledQuery()
            : _compounds(nullptr)
            {}

    void SetUp() override;
    void TearDown() override;
};

}  // namespace ::hdql::test
}  // namespace hdql

