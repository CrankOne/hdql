#include "events-struct.hh"
#include "hdql/operations.h"

namespace hdql {
namespace test {

helpers::CompoundTypes
define_compound_types(hdql_Context_t context) {
    #if 1
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
    return types;
    #else
    hdql::helpers::Compounds compounds;
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", context);
    compounds.emplace(typeid(RawData), rawDataCompound);
    {  // RawData::time
        struct hdql_AtomicTypeFeatures typeInfo = helpers::IFace<&RawData::time>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface = helpers::IFace<&RawData::time>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( rawDataCompound
                              , "time"
                              , ad);
    }
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo = helpers::IFace<&RawData::samples>::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface = helpers::IFace<&RawData::samples>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "size_t");
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( rawDataCompound
                              , "samples"
                              , ad);
    }

    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", context);
    compounds.emplace(typeid(Hit), hitCompound);
    {  // Hit::RawData
        struct hdql_Compound * typeInfo = helpers::IFace<&Hit::rawData>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface = helpers::IFace<&Hit::rawData>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_scalar(
                  typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( hitCompound
                              , "rawData"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", context);
    compounds.emplace(typeid(Track), trackCompound);
    {  // Tracks::hits
        struct hdql_Compound * typeInfo = helpers::IFace<&Track::hits>::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface = helpers::IFace<&Track::hits>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( trackCompound
                              , "hits"
                              , ad);
    }
    #endif
}

}  // namespace ::hdql::test
}  // namespace hdql


namespace hdql {
namespace test {

void
TestingEventStruct::SetUp() {
    _ctx = hdql_context_create(HDQL_CTX_PRINT_PUSH_ERROR);

    // reentrant table with type interfaces
    _valueTypes = hdql_context_get_types(_ctx);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(_valueTypes);
    // reentrant table with operations
    _operations = hdql_context_get_operations(_ctx);
    hdql_op_define_std_arith(_operations, _valueTypes);
    // this is the compound types definitions
    _compounds = hdql::test::define_compound_types(_ctx);
    //_compounds = hdql::helpers::CompoundTypes(ctx);
    if(_compounds.empty()) throw std::runtime_error("failed to initialize type tables");
    {
        auto it = _compounds.find(typeid(hdql::test::Event));
        if(_compounds.end() == it) {
            throw std::runtime_error("No root compound");
        }
        _eventCompound = it->second;
    }
}

void
TestingEventStruct::TearDown() {
    // sic! in this order: non-virtual compounds get destroyed AFTER context as
    // they are used to resolve attribute definitions in queries while cleaning
    // up queries
    hdql_context_destroy(_ctx);
    for(auto & ce : _compounds) {
        hdql_compound_destroy(ce.second, _compounds.context_ptr());
    }
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

