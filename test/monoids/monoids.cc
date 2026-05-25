#include "monoids.hh"
#include "../events-struct.hh"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/helpers/compounds.hh"
#include "hdql/value.h"

namespace hdql {
namespace test {

void
TestMonoidal::SetUp() {
    TestCompiledQuery::SetUp();
    hdql_converters_add_std( hdql_context_get_conversions(_context)
            , hdql_context_get_types(_context)
            , _context);
    hdql_functions_add_monoids(hdql_context_get_functions(_context));
}

}  // namespace ::hdql::test
}  // namespace hdql

