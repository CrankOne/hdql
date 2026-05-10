#include "basic-context.hh"

namespace hdql {
namespace test {

void
TestingContext::SetUp() {
    _ctx = hdql_context_create(HDQL_CTX_PRINT_PUSH_ERROR);

    // reentrant table with type interfaces
    _valueTypes = hdql_context_get_types(_ctx);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(_valueTypes);
    // reentrant table with operations
    _operations = hdql_context_get_operations(_ctx);
    hdql_op_define_std_arith(_operations, _valueTypes);
}

void
TestingContext::TearDown() {
    hdql_context_destroy(_ctx);
}

}  // namespace ::hdql::test
}  // namespace hdql

