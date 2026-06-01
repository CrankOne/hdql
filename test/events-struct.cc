#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/helpers/compounds.hh"
#include <typeindex>

namespace hdql {
namespace test {

void
define_test_event_compound(hdql_Context_t context) {
    helpers::CompoundTypes types(context);
    types.new_compound<RawData>("RawData")
        .attr<&RawData::time>("time")
        .attr<&RawData::samples, SimpleRangeSelection>("samples")
    .end_compound()
    .new_compound<Hit>("Hit")
        .attr<&Hit::energyDeposition>("energyDeposition")
        .attr<&Hit::time>("time")
        .attr<&Hit::x>("x")
        .attr<&Hit::y>("y")
        .attr<&Hit::z>("z")
        .attr<&Hit::rawData>("rawData")
    .end_compound()
    .new_compound<Track>("track")
        .attr<&Track::chi2>("chi2")
        .attr<&Track::ndf>("ndf")
        .attr<&Track::pValue>("pValue")
        .attr<&Track::hits, SimpleRangeSelection>("hits")
    .end_compound()
    .new_compound<Event>("Event")
        .attr<&Event::eventID>("eventID")
        .attr<&Event::hits, SimpleRangeSelection>("hits")
        .attr<&Event::tracks, SimpleRangeSelection>("tracks")
    .end_compound();
}

}  // namespace ::hdql::test
}  // namespace hdql


namespace hdql {
namespace test {

void
TestingEventStruct::_define_compounds(struct hdql_Context *context, const hdql_Compound *&rootCompound) {
    ::hdql::test::define_test_event_compound(context);
    std::type_index ti = typeid(hdql::test::Event);
    rootCompound = hdql_compounds_get_by_type_id(hdql_context_get_compounds(context)
            , &ti, sizeof(std::type_index));
}

SimpleRangeSelection
compile_simple_selection(const char * expression_) {
    assert(expression_);
    SimpleRangeSelection r;
    if('\0' == *expression_) {
        return {std::numeric_limits<size_t>::min(), std::numeric_limits<size_t>::max()};
    }
    assert('\0' != *expression_);
    const std::string expression(expression_);
    size_t n = expression.find(':');
    std::string fromStr = expression.substr(0, n);
    if(fromStr != "...") {
        r.first = std::stoul(fromStr);
    } else {
        r.first = std::numeric_limits<decltype(r.first)>::min();
    }
    if(std::string::npos != n) {
        std::string toStr = expression.substr(n+1);
        if(toStr != "...") {
            r.second = std::stoul(toStr);
        } else {
            r.second = std::numeric_limits<decltype(r.second)>::max();
        }
    } else {
        r.second = r.first + 1;
    }
    return r;
}

}  // namespace ::hdql::test
}  // namespace hdql

