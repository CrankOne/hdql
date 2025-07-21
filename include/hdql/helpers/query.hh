#pragma once

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>

struct hdql_Datum;  // fwd
struct hdql_Context;  // fwd
struct hdql_Query;  // fwd
struct hdql_CollectionKey;  // fwd
struct hdql_AttrDef;  // fwd
struct hdql_Compound;  // fwd
struct hdql_KeyView; // fwd

///\brief Matches `hdql_TypeConverter` callback function type
///
/// Alias introduced to not bring HDQL headers to top-level header.
typedef int (*ConverterFunc_t)(hdql_Datum * __restrict__ dest, hdql_Datum * __restrict__ src);

namespace hdql {
namespace errors {
/// Base error type for HDQL-related exceptions
class HDQLError : public std::runtime_error {
public:
    HDQLError(const char * reason) throw() : std::runtime_error(reason) {}
};  // class HDQLError

///\brief No subject instance bound to query
///
/// Indicates missing `Query::reset()` call -- query instance has no object
/// bound (which is done via curesor interface)
class NoQueryTarget : public HDQLError {
public:
    NoQueryTarget() : HDQLError("Query instance has no subject instance set") {}
};

/// HDQL expression error
class HDQLExpressionError : public HDQLError {
public:
    const char * expressionString;

    HDQLExpressionError( const char * expressionString_
                       , const char * message)
        : HDQLError(message)
        , expressionString(expressionString_ ? strdup(expressionString_) : NULL)
        {}
    ~HDQLExpressionError() {
        if(expressionString) free((void*) expressionString);
    }
};  // class HDQLExpressionError

/// Annotated HDQL query compilation error
class HDQLCompileError : public HDQLExpressionError {
public:
    const int errCode       ///< HDQL native error code
            , lineStart     ///< first line number of faulty fragment
            , lineEnd       ///< last line number of faulty fragment
            , charStart     ///< first character number of faulty fragment
            , charEnd       ///< last character number of faulty fragment
            ;

    HDQLCompileError( const char * expressionString_
            , const char * reason, int errCode_
            , int lineStart_, int lineEnd_
            , int charStart_, int charEnd_
            ) /*: HDQLExpressionError(expressionString_
                    , util::format("HDQL expression error [%d] (%d:%d - %d:%d): %s"
                        , errCode_, lineStart_, charStart_, lineEnd_, charEnd_
                        , reason ).c_str())
              , errCode(errCode_)
              , lineStart(lineStart_), lineEnd(lineEnd_)
              , charStart(charStart_), charEnd(charEnd_)
              {}*/;

    // TODO: fancy-shmancy printout function?
};
}  // namespace ::hdql::errors

namespace helpers {

class Query;

namespace detail {

class BaseQueryCursor;

/// Query lock interface
class QueryAccessCache {
public:
    typedef std::unordered_map<std::type_index, std::pair<ConverterFunc_t, uint8_t*> >
                ConvertResultValueDict;
    typedef std::add_pointer<typename ConvertResultValueDict::value_type>::type
                Converter;
private:
    /// Set to non-null value whenever the C++ cursor helper object is bound
    BaseQueryCursor * _cCursor;
    /// Conversion functions and buffers cache
    mutable ConvertResultValueDict _resultConversionsCache;
protected:
    QueryAccessCache() : _cCursor(nullptr) {}

    ConvertResultValueDict & _converters() const { return _resultConversionsCache; }

    /// Locks this query to given cursor
    void _lock_by(BaseQueryCursor &);
    /// Does only couple of assertion checks in debug build
    void _unlock(BaseQueryCursor &);

    friend class BaseQueryCursor;
};

/// Static base class for query cursor subtypes, implements cursor (un)binding
class BaseQueryCursor {
private:
    QueryAccessCache * _qPtr;
protected:
    BaseQueryCursor();
    ~BaseQueryCursor();
    void _bind(QueryAccessCache & qac) {
        assert(!_is_bound());
        qac._lock_by(*this);
        _qPtr = &qac;
        assert(_is_bound());
    }
    void _unbind();
    bool _is_bound() const { return _qPtr; }
    Query & _query_instance();
    const Query & _query_instance() const;
public:
    template<typename T> void reset(T &);
};

}  // namespace ::hdql::helpers::detail

/**\brief Thin access wrapper for HDQLang query results
 *
 * Forwards access calls for query result depending on expected types. Used
 * further in combination with expression to leverage value conversions.
 *
 * Used to perform iterative retrieval of query results. Compating to
 * `QueryCursor<>` template provides slightly lower performance as resolves
 * conversion callback each time the `get()` method is called.
 *
 *  CompoundTypes compoundTypes;
 *  hdql_Context * rootContextPtr;
 *  // ... (fill compounds, set up root context)
 *
 *  RootData data;
 *  data.foo = 1.234;  // ... set other values in `data`
 *  // ... (fill data instance)
 *
 *  QueryCursor<int> qci;
 *
 *  Query q(".foo", compoundTypes.get_compound_ptr<RootData>(), rootContextPtr);
 *  // ^^^ or user might want some kind of lightweight helper aggregating
 *  //     compound and context assets:
 *  // auto q = HDQLAssets.query<RootData>(".foo");
 *
 *  // one can retrieve information on query return type, keys, etc and
 *  // fork the execution depending on result/key type(s). Essential is that
 *  // it often need to be done *before* query gets bound to the data object
 *  // (producing `QueryCursor' instance)
 *  if(q.is_atomic() && q.is_convertible_to<int>()) {
 *      qci.bind(q);
 *      for(; qci; ++qco) {
 *          //auto qr2 = q(data);  // error: "query instance %p is currently locked by %p while another query result is requested"
 *          // ... handle scalar query results
 *      }
 *      qci.unbind();
 *  } else {
 *      for(GenericQueryCursor qc = q(data, compounds); qc; ++qc) {
 *          // ... handle compound query results
 *      }
 *  }
 *
 * \note One `Query` instance can issue (be locked by) single cursor object at
 *       time.
 * */
class GenericQueryCursor : protected detail::BaseQueryCursor {
protected:
    GenericQueryCursor(Query & q);

    template<typename RootT>
    GenericQueryCursor(Query & q, RootT &);
public:
    ~GenericQueryCursor();
    /// Forwards to `Query::_is_available()`
    operator bool() const;  // { return _query_instance()._is_available(); }
    /// Forwards to `Query::_next()`
    GenericQueryCursor & operator++();  // { _query_instance()._next(); return *this; }
    /// Forwards to `Query::_get_as<T>()`
    template<typename T> const T & get();  // { return _query_instance()._get_as<T>(); }

    friend class Query;
};  // class GenericQueryCursor

/**\brief Faster variant to query cursor with cached access function
 *
 * Whenever certain result type is assumed, this query cursor variant provides
 * faster data retreival as has conversion function cached in this instance.
 * */
template<typename T>
class QueryCursor : public detail::BaseQueryCursor {
private:
    detail::QueryAccessCache::Converter _converter;
protected:
    ///\brief Constructs bound instance
    ///
    /// For immediate `bind()` usage scenario
    template<typename RootT>
    QueryCursor(detail::QueryAccessCache::Converter, Query &, RootT & rootObject);
    ///\brief Constructs unbound cursor for reentrant usage
    ///
    /// For deferred `bind()` usage scenario
    QueryCursor(detail::QueryAccessCache::Converter, Query &);
public:
    QueryCursor() = delete;
    QueryCursor(const QueryCursor<T> &) = delete;
    ~QueryCursor();

    void bind(Query & q);
    void safe_bind(Query & q);
    void unbind();

    /// Forwards to `Query::_is_available()`
    operator bool() const;  // { return _qInstance._is_available(); }
    /// Forwards to `Query::_next()`
    QueryCursor & operator++();  // { _qInstance.next(); return *this; }

    /// Use conversion function
    const T & get() const;  // { ... use own conversion function }

    friend class Query;
};  // class TypesQueryCursor<T>

/**\brief Wraps compiled HDQLang expression as C++ polymorphic object
 *
 * Interim layer providing C++ API of HDQLang expression. Intended usage
 * scenario is to provide pre-compiled expression object, capable to provide
 * type introspection information about result type and keys, for further
 * decision-making in terms of existing compounds and strongly-typed query
 * result.
 *
 * Instance needs to be parameterised with:
 *  1. HDQLang expression to apply to a subject data item
 *  2. Pointer to a root compound type (within which query is done)
 *  3. Root HDQLang context instance to retrieve types and operations
 *  ...
 *
 * \note `Query` instance is a stateful object because of native `hdql_Query`
 *      instance it maintains internally. Effectively, this means that same
 *      `Query` object can not be used with two data subject items
 *      simultaneously.
 * */
class Query : protected detail::QueryAccessCache {
protected:
    bool _isSet;
    /// Sub-context (descendant to root one provided in ctr)
    hdql_Context * _ownContext;
    ///\brief Root compound ptr specified in ctr
    ///
    /// Used only to verify compound type at cursor creation.
    ///
    /// \note HDQL C query instance does not keep this pointer
    const hdql_Compound * _rootCompound;
    /// Pointer to C query instance which class wraps
    hdql_Query * _query;
    /// Collection keys cache (can be NULL if disabled by ctr)
    hdql_CollectionKey * _keys;
    /// Query result type ptr (managed by context)
    const hdql_AttrDef * _topAttrDef;
    /// Flat key view; can be NULL if keys disabled by ctr
    hdql_KeyView * _kv;
    /// Reference to compounds dictionary
    const std::unordered_map<std::type_index, hdql_Compound *> & _compounds;
private:
    /// Current query result pointer
    hdql_Datum * _r;

    template<typename RootT> hdql_Datum * _verify_root_compound_type(RootT & rootObject);
    bool _is_same_as_root_compound_type(const hdql_Compound *);  // TODO
protected:
    ///\brief Access for `BaseQueryCursor` subclass
    //detail::AttrOrder & _ordered_attr_queries()
    //        { return _attrQueries; }

    // cursor access functions

    /// Returns whether there is available query result
    bool _is_available() const;
    /// Retrieves new result
    void _next();
    

    //
    // converters
    template<typename T> Converter
    _get_converter() {
        auto it = _converters().find(typeid(T));
        if(it != _converters().end()) return &(*it);
        // get converter
        throw std::runtime_error("TODO: create converter assets item");
        //ConverterFunc_t cnvf = 
        //uint8_t * buf = hdql_alloc(_ownContext, );
        //auto ir = _converters().emplace(typeid(T), std::pair<ConverterFunc_t, uint8_t>(cnv_f, buf ));
        //it = ir.first;
        //return &(*it);
    }

    template<typename T> const T & _get_as() const;

    void _reset_subject_instance(hdql_Datum * e);

    /**\brief Returns compound C++ converter
     *
     * Should return initialized converter (adding it to
     * `QueryAccessCache::_resultConversionsCache`) if given compound type
     * differs from query result type or null if types match. Throws an
     * error if conversion is not defined.
     * */
    Converter _get_converter_to(const std::type_info &) const;
public:
    ///\brief Ctr for expressions expecting atomic or map query result
    ///
    /// Gets parameterised with HDQLang expression string, root compound type
    /// and execution context. \p keysNeeded switch may be set to `false` to
    /// suppress a little performance overhead when keys are not needed.
    /// \p attrsOrder should be provided in case compound result is expected,
    /// to claim the order of values in resulting value tuple.
    Query( const char * expression
         , const hdql_Compound * rootCompound
         , hdql_Context * rootContext
         , const std::unordered_map<std::type_index, hdql_Compound *> & compounds
         , bool keysNeeded=true
         //, const std::vector<std::string> & attrsOrder={}
         );

    Query() = delete;
    Query(const Query &) = delete;

    ~Query();

    /**\brief Returns whether the query result type of simple type
     *
     * True for simple arithmetic types and strings. Note, that collections
     * (arrays and maps) are also considered as "atomic". Same
     * as `!is_compound()`.
     * */
    bool is_atomic() const;
    /**\brief Returns whether the query result type is of compound type
     *
     * True for complex (compound) types. Same as `!is_atomic()`. */
    bool is_compound() const;

    bool is_scalar() const;
    bool is_collection() const;

    bool is_fwd_query() const;
    bool is_direct_query() const;

    bool is_static_value() const;

    bool is_transient() const;

    size_t keys_depth() const;
    const hdql_CollectionKey * keys() const;

    ///\brief Returns list of field names from the compound query result
    ///
    /// Only valid when `is_compound()` is true, throws `HDQLError` otherwise.
    /// If `recurseDelimiter` is set, will expand compound entries as well.
    std::vector<std::string> names(char recurseDelimiter='\0') const;

    std::vector<std::string> key_names() const;

    /// Returns generic (polymorphic) cursor instance
    ///
    /// Cursor locks the query, `RootT` must match the compound given ot
    /// construction (TODO: or be convertible to it)
    template<typename RootT> GenericQueryCursor generic_cursor_on(RootT &);
    /// Returns statically typed bound cursor instance
    ///
    /// Cursor locks the query, `RootT` must match the compound given ot
    /// construction (TODO: or be convertible to it)
    template<typename T, typename RootT> QueryCursor<T> cursor_on(RootT &);
    /// Returns statically typed unbound cursor instance
    ///
    /// Cursor does not lock the query
    template<typename T> QueryCursor<T> cursor();

    friend class detail::BaseQueryCursor;
    friend class GenericQueryCursor;
    template<typename T> friend class QueryCursor;
};  // class Query

template<typename RootT> hdql_Datum *
Query::_verify_root_compound_type(RootT & rootObject) {
    // find root compound type in the known ones
    auto it = _compounds.find(typeid(RootT));
    if(_compounds.end() == it) {
        throw errors::HDQLError("Unknown root compound type.");
    }
    // if compound type matches, do nothing
    if(_is_same_as_root_compound_type(it->second)) {
        return reinterpret_cast<hdql_Datum *>(&rootObject);
    }
    // otherwise, try to find compound conversion functions
    // TODO: from/to types printout
    throw errors::HDQLError("Compound conversion is not yet supported.");
}

template<typename RootT> GenericQueryCursor
Query::generic_cursor_on(RootT & rootObject) {
    //_reset_subject_instance(_verify_root_compound_type(rootObject));
    return GenericQueryCursor(*this, rootObject);
}

template<typename T, typename RootT> QueryCursor<T>
Query::cursor_on(RootT & rootObject) {
    //_reset_subject_instance(_verify_root_compound_type(rootObject));
    return QueryCursor<T>(_get_converter_to(typeid(T)), *this, rootObject);
}

template<typename T> QueryCursor<T>
Query::cursor() {
    //_reset_subject_instance(_verify_root_compound_type(rootObject));
    return QueryCursor<T>(_get_converter_to(typeid(T)), *this);
}

template<typename T> const T &
Query::_get_as() const {
    // Relying on QueryAccessCache, get the conversion function and apply it on
    // current result to retrieve the value if need.
    //
    // Note, that conversion is ignored if this is of a known compound type and
    // not possible if that's a virtual compound.
    assert(!is_collection());  // TODO: how to deal with collections?
    Converter cnv = _get_converter_to(typeid(T));
    if(!cnv) {  // same type
        return *reinterpret_cast<const T*>(_r);
    }
    cnv->second.first(reinterpret_cast<hdql_Datum*>(cnv->second.second), _r);
    return *reinterpret_cast<const T*>(cnv->second.second);
}

//
//

template<typename T> void
detail::BaseQueryCursor::reset(T & rootObject) {
    this->_query_instance()._reset_subject_instance(
            this->_query_instance()._verify_root_compound_type(rootObject));
}

//
// Generic query cursor getter (deferred implementation of template method)

template<typename RootT>
GenericQueryCursor::GenericQueryCursor(Query & q, RootT & rootObject)
        : GenericQueryCursor(q) {
    reset(rootObject);
}

template<typename T> const T &
GenericQueryCursor::get() {
    return _query_instance()._get_as<T>();
}

//
// Query cursor (template deferred implem)

template<typename T>
QueryCursor<T>::QueryCursor(detail::QueryAccessCache::Converter cnv, Query & q)
        : _converter(cnv) {
    bind(q);
}

template<typename T> template<typename RootT>
QueryCursor<T>::QueryCursor(detail::QueryAccessCache::Converter cnv, Query & q, RootT & rootObject)
        : QueryCursor<T>(cnv, q) {
    detail::BaseQueryCursor::reset(rootObject);
}

template<typename T>
QueryCursor<T>::~QueryCursor() {
    if(_is_bound())
        _unbind();
    assert(!_is_bound());
}

template<typename T> void
QueryCursor<T>::bind(Query &q) {
    assert(!_is_bound());
    // todo: if-debug check here that types are compatible?
    BaseQueryCursor::_bind(q);
    assert(_is_bound());
    //_query_instance()._next();  // ?
}

template<typename T> void
QueryCursor<T>::unbind() {
    assert(_is_bound());
    BaseQueryCursor::_unbind();
    assert(!_is_bound());
}

template<typename T>
QueryCursor<T>::operator bool() const {
    if(!_is_bound()) return false;
    return _query_instance()._is_available();
}

template<typename T> QueryCursor<T> &
QueryCursor<T>::operator++() {
    if(!_is_bound()) {
        throw std::runtime_error("Can't advance statically typed"
                " cursor instance because it is not bound to a query.");
    }
    _query_instance()._next();
    return *this;
}

template<typename T> const T &
QueryCursor<T>::get() const {
    if(!_is_bound()) {
        throw std::runtime_error("Can't obtain data from statically typed"
                " cursor instance because it is not bound to a query.");
    }
    if(_converter) {
        int rc 
            = _converter->second.first( reinterpret_cast<hdql_Datum *>(_converter->second.second)
                    , _query_instance()._r );
        if(rc) {
            char errbf[128];
            snprintf(errbf, sizeof(errbf), "Value conversion failed with error"
                    " code %d", rc);
            throw std::runtime_error(errbf);  // TODO: dedicated exception
        }
        return *reinterpret_cast<const T*>(_converter->second.second);
    } else {
        return *reinterpret_cast<const T *>(_query_instance()._r);
    }
}

}  // namespace ::hdql::helpers
}  // namespace hdql

