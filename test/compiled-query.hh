#pragma once

#include "basic-context.hh"

#include "hdql/helpers/compounds.hh"

#include "basic-context.hh"

namespace hdql {
namespace test {

class TestCompiledQuery : public TestingContext {
protected:
    hdql::helpers::CompoundTypes _compounds;
    hdql_Compound *_rootCompound;
    hdql_Query *_query;
    hdql_Key *_queryKey;
    size_t _flatKeyViewLen;
    hdql_Key **_flatKeyView;
    const hdql_ValueInterface **_flatKeyIfaces;

    virtual helpers::CompoundTypes _define_compounds(hdql_Context *, hdql_Compound *&) = 0;
public:
    TestCompiledQuery();

    virtual void CompileQuery(const char *, bool enableKeys=false);
    void ResetQuery(void *datum, void *&result);
    void AdvanceQuery(void *&result);

    void SetUp() override;
    void TearDown() override;
};

}  // namespace ::hdql::test
}  // namespace hdql

