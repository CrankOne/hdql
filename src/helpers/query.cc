#include "hdql/helpers/query.hh"

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/context.h"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace hdql {
namespace errors {

HDQLCompileError::HDQLCompileError( const char * expressionString_
            , const char * reason, int errCode_
            , int lineStart_, int lineEnd_
            , int charStart_, int charEnd_
            ) : /*HDQLExpressionError(expressionString_
                    , util::format("HDQL expression error [%d] (%d:%d - %d:%d): %s"
                        , errCode_, lineStart_, charStart_, lineEnd_, charEnd_
                        , reason ).c_str())*/
                HDQLExpressionError(expressionString_, reason)
              , errCode(errCode_)
              , lineStart(lineStart_), lineEnd(lineEnd_)
              , charStart(charStart_), charEnd(charEnd_)
              {};

}  // namespace ::hdql::errors

namespace helpers {
namespace detail {
//
// Query access caches helper

void
QueryAccessCache::_lock_by(BaseQueryCursor & c) {
    if(_cCursor) {
        // TODO: "query instance %p is currently locked by %p while another query result is requested"
        throw std::runtime_error("concurrent lock (TODO: dedicated error class)");
    }
    _cCursor = &c;
}

void
QueryAccessCache::_unlock(BaseQueryCursor & c) {
    assert(_cCursor);
    assert(_cCursor == &c);
    _cCursor = nullptr;
}

//
// Base query sursor

BaseQueryCursor::BaseQueryCursor() : _qPtr(nullptr) {
}

BaseQueryCursor::~BaseQueryCursor() {
}

void
BaseQueryCursor::_unbind() {
    _qPtr->_unlock(*this);
    _qPtr = nullptr;
}


Query &
BaseQueryCursor::_query_instance() {
    assert(_is_bound());
    assert(_qPtr);
    return *static_cast<Query*>(_qPtr);
}

const Query &
BaseQueryCursor::_query_instance() const {
    assert(_is_bound());
    return *static_cast<Query*>(_qPtr);
}
}  // namespace ::hdql::helpers::detail

//
// Generic query result cursor

GenericQueryCursor::GenericQueryCursor(Query & q) {
    BaseQueryCursor::_bind(q);
}

GenericQueryCursor::~GenericQueryCursor() {
    BaseQueryCursor::_unbind();
}

GenericQueryCursor::operator bool() const {
    return _query_instance()._is_available();
}

GenericQueryCursor &
GenericQueryCursor::operator++() {
    _query_instance()._next();
    return *this;
}

//
// Query

Query::Query( const char * expression
         , const hdql_Compound * rootCompound
         , hdql_Context * rootContext
         , const std::unordered_map<std::type_index, hdql_Compound *> & compounds
         , bool keysNeeded )
                : _isSet(false)
                , _ownContext(hdql_context_create_descendant(rootContext, HDQL_CTX_PRINT_PUSH_ERROR))
                , _rootCompound(rootCompound)
                , _query(nullptr)
                , _keys(nullptr)
                , _topAttrDef(nullptr)
                , _kv(nullptr)
                , _compounds(compounds)
                , _r(nullptr)
                {
    assert(expression);
    assert(rootCompound);
    assert(rootContext);

    int rc;
    char errBuf[256];
    {  // compile the query
        // 0 error code,
        // 1 first column, 2 first line
        // 3 last column, 4 last line
        int errDetails[5];
        _query = hdql_compile_query( expression
                                   , rootCompound
                                   , _ownContext
                                   , errBuf, sizeof(errBuf)
                                   , errDetails
                                   );
        rc = errDetails[0];
        if(rc != HDQL_ERR_CODE_OK) {
            throw errors::HDQLCompileError(expression, errBuf, errDetails[0]
                    , errDetails[1], errDetails[3]
                    , errDetails[2], errDetails[4]
                    );
        }
        assert(NULL != _query);
    }
    // TODO: use this to provide debug log when built in UNIX (fmemopen()
    // restriction)
    //if(logCat.getPriority() >= log4cpp::Priority::DEBUG) {
    //    const size_t hdqlQueryLogSize = 4*1024;
    //    char * hdqlQueryLogStr = reinterpret_cast<char*>(malloc(hdqlQueryLogSize));
    //    FILE * hdqlQueryLog = fmemopen(hdqlQueryLogStr, hdqlQueryLogSize, "w");
    //    hdql_query_dump(hdqlQueryLog, _query, _ownContext);
    //    fclose(hdqlQueryLog);
    //    logCat << log4cpp::Priority::DEBUG << "Compiled query (multiline msg): " << hdqlQueryLogStr
    //        << " (end of multiline msg)";
    //    free(hdqlQueryLogStr);
    //}
    _topAttrDef = hdql_query_top_attr(_query);
    if(!_topAttrDef) {
        // this is generally a severe error in HDQL engine
        throw errors::HDQLExpressionError(expression
                , "HDQL did not provide query result type"
                " definition (top attribute)" );
    }
    // reserve keys and flat keys view, if need
    if(keysNeeded) {
        // reserve ordinary keys list
        rc = hdql_query_keys_reserve(_query, &_keys, _ownContext);
        if(HDQL_ERR_CODE_OK != rc) {
            snprintf(errBuf, sizeof(errBuf), "Failed to reserve keys for HDQL"
                    " query result; returns code is %d: %s.", rc, hdql_err_str(rc));
            throw errors::HDQLExpressionError(expression, errBuf);
        }
        // reserve flat keys view
        size_t flatKeyViewLength = hdql_keys_flat_view_size(_query, _keys, _ownContext);
        _kv = flatKeyViewLength
                      ? (hdql_KeyView *) malloc(sizeof(hdql_KeyView)*flatKeyViewLength)
                      : NULL;
        hdql_keys_flat_view_update(_query, _keys, _kv, _ownContext);
    }
}

bool
Query::_is_available() const {
    return _isSet && _r != nullptr;
}

void
Query::_next() {
    assert(_query);
    if(!_isSet) {
        throw errors::NoQueryTarget();
    }
    _r = hdql_query_get(_query, _keys, _ownContext);
}

Query::~Query() {
    if(_keys) {
        hdql_query_keys_destroy(_keys, _ownContext);
    }
    if(_kv) {
        free(_kv);
    }
    if(_query && _ownContext) {
        hdql_query_destroy(_query, _ownContext);
    }
    if(_ownContext) {
        hdql_context_destroy(_ownContext);
    }
    for(auto & p : _converters()) {
        if(p.second.second)
            hdql_context_free(_ownContext
                    , reinterpret_cast<hdql_Datum_t>(p.second.second)  // LABEL:CONVERSION-DEST_BUF:FREE
                    );
    }
}

size_t
Query::keys_depth() const {
    assert(_query);
    return hdql_query_depth(_query);
}

const hdql_CollectionKey *
Query::keys() const {
    return _keys;
}

detail::QueryAccessCache::Converter
Query::_get_converter_to(const std::type_info & destTypeInfo) const {
    auto cnvIt = _converters().find(destTypeInfo);
    if(_converters().end() != cnvIt) return &(*cnvIt);

    std::pair<ConverterFunc_t, uint8_t *> cnvFAndBuf(nullptr, nullptr);
    if(is_compound()) {
        // create compound type conversion
        const hdql_Compound * srcCompoundType = hdql_attr_def_compound_type_info(_topAttrDef);
        assert(srcCompoundType);
        // compound type expected, try to find dest compound type
        auto it = _compounds.find(destTypeInfo);
        if(_compounds.end() == it) {
            char errbf[128];
            snprintf(errbf, sizeof(errbf), "Conversion from compound %s to an"
                    " unknown type requested."
                    , hdql_compound_get_name(srcCompoundType) );
        }
        // conversion target compound type found
        // TODO: this if-clause only suceeds if compound type match for the
        //       query result and requested type.
        if( hdql_compound_is_virtual(srcCompoundType)
         || hdql_compound_is_virtual(it->second)
          ) {
            // TODO: from/to virtual compound conversion
            char errbf[128];
            snprintf( errbf, sizeof(errbf)
                    , "Conversion to/from virtual compound"
                    " type (%s -> %s) is not supported by HDQLang"
                    " C++ wrapper yet."
                    , hdql_compound_get_name(srcCompoundType)
                    , hdql_compound_get_name(it->second)
                    );
            throw std::runtime_error(errbf);
        }

        if(srcCompoundType != it->second) {
            // TODO: conversion between compound types
            char errbf[128];
            snprintf( errbf, sizeof(errbf)
                    , "Conversion between compound"
                    " types (%s -> %s) is not supported by HDQLang yet."
                    , hdql_compound_get_name(srcCompoundType)
                    , hdql_compound_get_name(it->second)
                    );
            throw std::runtime_error(errbf);
        }

        assert(srcCompoundType == it->second);
        cnvIt = _converters().emplace(destTypeInfo, cnvFAndBuf).first;
    } else {  // create atomic type conversion
        hdql_Converters * cnvs = hdql_context_get_conversions(_ownContext);
        assert(cnvs);
        hdql_ValueTypes * vts = hdql_context_get_types(_ownContext);
        assert(vts);
        // get demangled type name of dest type
        char destTypeNameBf[64];
        hdql_cxx_demangle(destTypeInfo.name(), destTypeNameBf, sizeof(destTypeNameBf));
        // get type code for dest type using demangled name (aliases accounted)
        hdql_ValueTypeCode_t destTypeCode = hdql_types_get_type_code(vts, destTypeNameBf);
        if(0x0 == destTypeCode) {
            char errbf[256];
            snprintf(errbf, sizeof(errbf), "Atomic type of conversion target"
                    " \"%s\" is not registered within HDQLang context (%p)"
                    " in use."
                    , destTypeNameBf, _ownContext );
            throw std::runtime_error(errbf);
        }
        hdql_ValueTypeCode_t srcTypeCode = hdql_attr_def_get_atomic_value_type_code(_topAttrDef);
        assert(srcTypeCode);  // if expression compiled at all, this must be true
        
        if(destTypeCode != srcTypeCode) {
            cnvFAndBuf.first = hdql_converters_get(cnvs, destTypeCode, srcTypeCode);
            if(!cnvFAndBuf.first) {
                char errbf[128];
                snprintf(errbf, sizeof(errbf), "No conversion defined for case"
                        " %s -> %s"
                        , hdql_types_get_type(vts, srcTypeCode)->name
                        , hdql_types_get_type(vts, destTypeCode)->name
                        );
                throw std::runtime_error(errbf);
            }  // if conversion not found
            // conversion found -- allocate buffer in own context
            cnvFAndBuf.second = reinterpret_cast<uint8_t*>(hdql_context_alloc(_ownContext
                    , hdql_types_get_type(vts, destTypeCode)->size ));  // LABEL:CONVERSION-DEST_BUF:ALLOC
            if(!cnvFAndBuf.second) {
                char errbf[128];
                snprintf( errbf, sizeof(errbf)
                        , "Context failed to allocate buffer of size %zub"
                          " for conversion destination of type %s"
                        , hdql_types_get_type(vts, destTypeCode)->size
                        , hdql_types_get_type(vts, destTypeCode)->name
                        );
                throw std::runtime_error(errbf);
            }
            cnvIt = _converters().emplace(destTypeInfo, cnvFAndBuf).first;
            return &(*cnvIt);
        }  //  if conversion needed
        return nullptr;
    }  // if of atomic type
    return &(*cnvIt);
}

void
Query::_reset_subject_instance(hdql_Datum_t item) {
    assert(item);
    int rc = hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(item), _ownContext);
    if(HDQL_ERR_CODE_OK != rc) {
        throw std::runtime_error("hdql_query_reset() failed");
    }
    _r = hdql_query_get(_query, _keys, _ownContext);
    _isSet = true;
}

bool
Query::_is_same_as_root_compound_type(const hdql_Compound * tgtCompound) {
    return hdql_compound_is_same(_rootCompound, tgtCompound);
}

bool
Query::is_atomic() const {
    return hdql_attr_def_is_atomic(_topAttrDef);
}

bool
Query::is_compound() const {
    return hdql_attr_def_is_compound(_topAttrDef);
}

bool
Query::is_scalar() const {
    return hdql_attr_def_is_scalar(_topAttrDef);
}

bool
Query::is_collection() const {
    return hdql_attr_def_is_collection(_topAttrDef);
}

bool
Query::is_fwd_query() const {
    return hdql_attr_def_is_fwd_query(_topAttrDef);
}

bool
Query::is_direct_query() const {
    return hdql_attr_def_is_direct_query(_topAttrDef);
}

bool
Query::is_static_value() const {
    return hdql_attr_def_is_static_value(_topAttrDef);
}

bool
Query::is_stray() const {
    return hdql_attr_def_is_stray(_topAttrDef);
}

#if 0
hdql_TypeConverter
get_atomic_scalar_converter_for( hdql_Context_t ownContext
        , hdql_AttrDef_t topAttrDef
        , const char * toTypeName  //=::hdql::helpers::detail::ArithTypeNames<T>::name
        ) {
    hdql_ValueTypes * vts = hdql_context_get_types(ownContext);
    assert(vts);
    // check that value is
    //  - atomic (i.e. of plain int/float/bool type)
    if(!hdql_attr_def_is_atomic(topAttrDef)) {
        throw errors::HDQLError("Request result in non-atomic value (not an"
                " integer, float, bool)");
    }
    //  - scalar (not an [associative] array)
    if(!hdql_attr_def_is_scalar(topAttrDef)) {
        throw errors::HDQLError("Request result in non-scalar value (a"
                " list or map)");
    }
    if(!hdql_attr_def_is_scalar(topAttrDef)) {
        throw errors::HDQLError("Request result in non-atomic value (not an"
                " integer, float, bool)");
    }
    hdql_ValueTypeCode_t typeFrom = hdql_attr_def_get_atomic_value_type_code( topAttrDef )
                       , typeTo   = hdql_types_get_type_code(vts, toTypeName)
                       ;
    if(0 == typeFrom) {
        // This should indicate context clash, severe error or an attempt to
        // cast a value from some temporary (compound) type
        throw errors::HDQLError("Context could not resolve query result type");
    }
    if(0 == typeTo) {
        // This can be possible if none of the context in chain has "standard
        // types" registered
        char errbf[128];
        snprintf(errbf, sizeof(errbf), "Context has no \"%s\" type"
                , toTypeName);
        throw errors::HDQLError(errbf);
    }
    //bzero(aiface.nativeObjectDest, ValueTraits<T>::nativeSize);
    if(typeFrom == typeTo) {
        // no conversion needed
        return nullptr;
    }

    hdql_Converters * cnvs = hdql_context_get_conversions(ownContext);
    assert(cnvs);
    ConverterFunc_t aiface = hdql_converters_get(cnvs, typeTo, typeFrom);
    if(!aiface) {
        // Type conversion is not defined
        // TODO: details
        assert(hdql_types_get_type(vts, typeFrom));
        char errbf[128];
        snprintf(errbf, sizeof(errbf), "Type conversion from %s to %s is"
                  " not defined in HDQL context"
                , hdql_types_get_type(vts, typeFrom)->name
                , hdql_types_get_type(vts, typeTo)->name
                );
        throw errors::HDQLError(errbf);
    }
    return aiface;
}
#endif

}  // namespace ::hdql::helpers
}  // namespace hdql

