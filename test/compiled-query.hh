#pragma once

#include "basic-context.hh"

#include "hdql/helpers/compounds.hh"

#include "basic-context.hh"
#include "hdql/types.h"

namespace hdql {
namespace test {

class TestCompiledQuery : public TestingContext {
protected:
    // dedicated context instance for compounds and other user-spaced definitions
    hdql_Context *_compoundsContext;
    const hdql_Compound *_rootCompound;
    hdql_Query *_query;
    hdql_Key *_queryKey;
    size_t _flatKeyViewLen;
    hdql_Key **_flatKeyView;
    const hdql_ValueInterface **_flatKeyIfaces;

    virtual void _define_compounds(hdql_Context *, const hdql_Compound *&) = 0;
public:
    TestCompiledQuery();

    virtual void CompileQuery(const char *, bool enableKeys=false);
    void ResetQuery(hdql_Datum *datum, hdql_Datum *&result);
    void AdvanceQuery(hdql_Datum *&result);

    void SetUp() override;
    void TearDown() override;
};

}  // namespace ::hdql::test
}  // namespace hdql

