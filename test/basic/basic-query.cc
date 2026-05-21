#include "basic-query.hh"

#include "hdql/context.h"
#include "hdql/query.h"
#include "hdql/attr-def.h"

namespace hdql {
namespace test {

void BasicQuery::SetUp() {
    _context = hdql_context_create(HDQL_CTX_PRINT_PUSH_ERROR);
    ASSERT_TRUE(_context);
}

void BasicQuery::TearDown() {
    if(!_context) return;
    if(_q)
        hdql_query_destroy(_q, _context);
    if(_ad)
        hdql_attr_def_destroy(_ad, _context);
    hdql_context_destroy(_context);
}

}
}

