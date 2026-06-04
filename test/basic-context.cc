#include "basic-context.hh"
#include "hdql/errors.h"
#include "hdql/random.h"

namespace hdql {
namespace test {

void
TestingContext::SetUp() {
    _context = hdql_context_create(HDQL_CTX_PRINT_PUSH_ERROR, &hdql_gHeapAllocator);
    hdql_rand_seed(hdql_context_get_randgen(_context), 0xdeadbeef, 0 );

    // reentrant table with type interfaces
    _valueTypes = hdql_context_get_types(_context);
    // add standard (int, float, etc) types
    int rc = hdql_value_types_table_add_std_types(_valueTypes);
    ASSERT_EQ(rc, HDQL_ERR_CODE_OK) << "failed to init std. types table:"
            << rc << " -- \"" << hdql_err_str(rc) << "\"";
            ;
    // reentrant table with operations
    _operations = hdql_context_get_operations(_context);
    hdql_op_define_std_arith(_operations, _valueTypes);
}

void
TestingContext::TearDown() {
    hdql_context_destroy(_context);
}

}  // namespace ::hdql::test
}  // namespace hdql

