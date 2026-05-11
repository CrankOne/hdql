#include "monoids.hh"
#include "../events-struct.hh"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/helpers/fancy-print-err.h"
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

}  // namespace ::hdql::test
}  // namespace hdql


namespace hdql {
namespace test {

void
TestAggFuncs::SetUp() {
    TestingContext::SetUp();
    hdql_converters_add_std( hdql_context_get_conversions(_ctx)
            , hdql_context_get_types(_ctx)
            , _ctx);
    hdql_functions_add_monoids(hdql_context_get_functions(_ctx));
    // this is the compound types definitions
    _compounds = define_compound_types(_ctx);
    //_compounds = hdql::helpers::CompoundTypes(ctx);
    if(_compounds.empty()) throw std::runtime_error("failed to initialize type tables");
    {
        auto it = _compounds.find(typeid(hdql::test::RootItem));
        if(_compounds.end() == it) {
            throw std::runtime_error("No root compound");
        }
        _rootCompound = it->second;
    }
}

void
TestAggFuncs::CompileQuery(const char * expression) {
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query( expression
                              , _rootCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    int rc = errDetails[0];
    if(rc) {
        char fancyprintbuf[1024];
        rc = hdql_fancy_error_print(fancyprintbuf, sizeof(fancyprintbuf)
                , expression, errDetails, errBuf, 0x1);
        fputs(fancyprintbuf, stderr);
        if(rc) {
            fputs("(truncated)\n", stderr);
        }
    }
    ASSERT_EQ(errDetails[0], 0);
}

void
TestAggFuncs::TearDown() {
    if(_query)
        hdql_query_destroy(_query, _compounds.context_ptr());
    // sic! in this order: non-virtual compounds get destroyed AFTER context as
    // they are used to resolve attribute definitions in queries while cleaning
    // up queries
    TestingContext::TearDown();
    for(auto & ce : _compounds) {
        hdql_compound_destroy(ce.second, _compounds.context_ptr());
    }
}

}  // namespace ::hdql::test
}  // namespace hdql

