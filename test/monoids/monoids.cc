#include "monoids.hh"
#include "../events-struct.hh"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/helpers/compounds.hh"
#include "hdql/value.h"

namespace hdql {
namespace test {

namespace {
helpers::CompoundTypes
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
    .end_compound();
    return types;
}
}

helpers::CompoundTypes
TestMonoidal::_define_compounds(hdql_Context * context, hdql_Compound *& rootCompound) {
    helpers::CompoundTypes compounds = define_compound_types(context);
    if(compounds.empty()) throw std::runtime_error("failed to initialize type tables");
    {
        auto it = compounds.find(typeid(hdql::test::RootItem));
        if(compounds.end() == it) {
            throw std::runtime_error("No root compound");
        }
        rootCompound = it->second;
    }
    return compounds;
}

void
TestMonoidal::SetUp() {
    TestCompiledQuery::SetUp();
    hdql_converters_add_std( hdql_context_get_conversions(_ctx)
            , hdql_context_get_types(_ctx)
            , _ctx);
    hdql_functions_add_monoids(hdql_context_get_functions(_ctx));
}

}  // namespace ::hdql::test
}  // namespace hdql

