#include "cross-type-operations.hh"
#include "events-struct.hh"  // for SingleRangeSelection
#include "hdql/compound.h"
#include "hdql/context.h"

namespace hdql {
namespace test {

namespace {
void
define_compound_types(hdql_Context_t context) {
    helpers::CompoundTypes types(context);
    types.new_compound<Item>("Item")
        .attr<&Item::bf  >("bf")
        .attr<&Item::i8f >("i8f")
        .attr<&Item::u8f >("u8f")
        .attr<&Item::i16f>("i16f")
        .attr<&Item::u16f>("u16f")
        .attr<&Item::i32f>("i32f")
        .attr<&Item::u32f>("u32f")
        .attr<&Item::i64f>("i64f")
        .attr<&Item::u64f>("u64f")
        .attr<&Item::ff  >("ff")
        .attr<&Item::df  >("df")
    .end_compound()
    .new_compound<RootItem>("Root")
        .attr<&RootItem::a, SimpleRangeSelection>("a")
        .attr<&RootItem::b, SimpleRangeSelection>("b")
        .attr<&RootItem::u16f>("u16f")
    .end_compound();
}
}

void
TestCrossTypeOperations::_define_compounds(hdql_Context * context
        , const hdql_Compound *&rootCompound) {
    define_compound_types(context);
    rootCompound = hdql_compounds_get_by_name(hdql_context_get_compounds(context), "Root");
}

}  // namespace ::hdql::test
}  // namespace hdql

