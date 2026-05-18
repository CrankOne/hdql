#include "compiled-query.hh"

namespace hdql {
namespace test {

void
TestCompiledQuery::SetUp() {
    TestingContext::SetUp();
    // this is the compound types definitions
    _compounds = _define_compounds(_ctx);
}

void
TestCompiledQuery::TearDown() {
    // sic! in this order: non-virtual compounds get destroyed AFTER context as
    // they are used to resolve attribute definitions in queries while cleaning
    // up queries
    TestingContext::TearDown();
    for(auto & ce : _compounds) {
        hdql_compound_destroy(ce.second, _compounds.context_ptr());
    }
}

}  // namespace test
}  // namespace hdql

