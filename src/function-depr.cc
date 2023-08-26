#include "hdql/function.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <cassert>
#include <cstring>
#include <unordered_map>

#include <iostream>  // XXX

static const unsigned int
          hdql_kArithOp = 0x1
        , hdql_kGenericFunc = 0x2
        , hdql_kCartFunc = 0x3
        ;

//
// Functions dictionary

namespace {
struct FuncCtrEntry {
    unsigned int type:2;
    unsigned int isStateful:1;
    union {
       // ...
    } ctr;
};
}  // anonymous namespace

struct hdql_Functions : public std::unordered_multimap<std::string, ::FuncCtrEntry> {
};


/* Returns name for defined function (or NULL) */
//const char * hdql_functions_function_name(struct hdql_Functions *, hdql_FunctionConstructor);
// ^^^ TODO: aliased functions?

// NOT exposed to public header
extern "C" struct hdql_Functions *
_hdql_functions_create(struct hdql_Context * ctx) {
    return new hdql_Functions;
}

// NOT exposed to public header
extern "C" void
_hdql_functions_destroy(struct hdql_Context * ctx, struct hdql_Functions * funcDict) {
    delete funcDict;
}

//
// Operations node

/* Function instance representation (internal)
 *
 * Technically, this is just a C++ class wrapper around function union.
 *
 * HDQL assume "function" to be a stateful object (actually a functor) to
 * implement generator-like approach of list iteration. This class wraps
 * some common routines for function-like objects:
 *  - arithmetic operations (unary, binary and ternary)
 *  - functions with arguments received as cartesian product
 *  - functions with sub-queries
 * */
struct hdql_Func {
public:
    // Iteration over set of arguments
    // WARNING: that this state is not the entire state actually, as query
    //      objects bound to `hdql_Func` instance are also changed during
    //      evaluation.
    // TODO: should be initialized and provided together with class instance
    struct State {
        unsigned char isResutValid:1;
        unsigned char isExhausted:1;
        hdql_CollectionKey ** keys;
        hdql_Datum_t * argValues;
        hdql_Datum_t result;  // gets invalidated on advance
    };
    typedef hdql_Datum_t (*CastArgumentFunc)(hdql_Datum_t, hdql_Context_t);
private:
    // Function type, used mainly for debugging, not checked during evaluation
    // in normal mode
    unsigned short _type:2;
    // Number of arguments in use
    unsigned short _nArgs:HDQL_FUNC_MAX_NARGS_2POW;
    // If set, stateful interface used
    bool _isStateful;
    // Used only when `_isStateful` is true
    hdql_Datum_t _fUserdata;
    // Function callback one-of
    union {
        struct hdql_OperationEvaluator arithOp;
        struct {
            hdql_ValueTypeCode_t returnType;
            hdql_StatelessScalarFunction_t call;
        } slScalar;
        struct hdql_StatefulScalarFunction sfScalar;
        struct hdql_GenericFunction sfGeneric;
    } _f;
    // Argument queries in use
    hdql_Query * _argQueries[(2 << HDQL_FUNC_MAX_NARGS_2POW) - 1];
    // Argument convesion functions
    CastArgumentFunc _argCast[(2 << HDQL_FUNC_MAX_NARGS_2POW) - 1];
protected:
    // Most crucial method for arithmetic operations and functions receiving
    // data objects
    //
    // Crucial and fragile algorithm, retrieving arguments as cartesian
    // product, keeping intermediate iteration result in state object. The
    // idea here is to, say, with f({a}, {b,c}, {d,e})
    // provide following sequence of invocations:
    //      f(a, b, d)
    //      f(a, c, d)
    //      f(a, b, e)
    //      f(a, c, e)
    // Iteration state is kept in `State` instance between invocations.
    // Algorithm used for arithmetic operations and (cartesian product)
    // functions, generic functions should use queries directly, with extra
    // caution.
    //
    // Not called for generic functions.
    bool _next_arguments( hdql_Datum_t root
                       , State & state
                       , hdql_Context_t context
                       );
    // Sets up function state for iterating arguments, used for arith and
    // cartesian functions, generic functions must not invoke it.
    // Assumes state is NOT initialized, except for keys pointer that can be
    // set. Sets state's flags: isResultValid -> `false`, `isExhausted` -> `false`
    // or `true` depending on whether arguments provided empty set. Gets called
    // every `reset()` invocation of the collection interface.
    //
    // Must not be called for generic functions.
    void _init_cart_state( hdql_Datum_t root
                           , State & state
                           , hdql_Context_t context
                           );
    // Called to initialize new state for cartesian-args functions
    void _alloc_state_vars(State & state, hdql_Context_t ctx);

    void _destroy_state( State & state, hdql_Context_t context );

    // evaluation method for arithmetic functions
    int _eval_arith( hdql_Datum_t root
                   , State & state
                   , hdql_Context_t context
                   );
    // evaluation method for stateless function nodes of cartesian arguments
    int _eval_sl_scalar( hdql_Datum_t root
                       , State & state
                       , hdql_Context_t context
                       );
    // evaluation method for stateful function nodes of cartesian arguments
    int _eval_sf_scalar( hdql_Datum_t root
                       , State & state
                       , hdql_Context_t context
                       );
    // evaluation method for generic functions
    int _eval_genf( hdql_Datum_t root
                  , State & state
                  , hdql_Context_t context
                  );
public:
    // arithmetic function ctr
    hdql_Func( const struct hdql_OperationEvaluator * op
             , size_t nArgs );
    // cart. prod. function ctr
    hdql_Func( hdql_StatelessScalarFunction_t op, hdql_ValueTypeCode_t returnType
             , size_t nArgs );
    hdql_Func( const struct hdql_StatefulScalarFunction * op
             , size_t nArgs );
    // generic function ctr
    hdql_Func( const struct hdql_GenericFunction * op
             , size_t nArgs );

    // sets query for n-th argument
    int set_arg(size_t n, hdql_Query * q);
    // reserves keys for function result
    hdql_CollectionKey * reserve_keys(hdql_Context_t);
    // copies keys for function result
    int copy_keys(hdql_CollectionKey * dest, const hdql_CollectionKey * src, hdql_Context_t);
    // evaluates callback with user data on every key respecting arguments
    // query topology
    int call_for_each_key( hdql_CollectionKey * keys
                         , hdql_KeyIterationCallback_t callback, void * userdata
                         , size_t cLevel, size_t nKeyAtLevel
                         , hdql_Context_t context
                         );
    // deletes keys for query result
    int destroy_keys(hdql_CollectionKey * dest, hdql_Context_t);

    //
    // As-collection interface to be used in queries

    // Reentrant stateful instance used as collection "iterator"
    struct StateAndFunc {
        hdql_Func * func;  // ptr to owning function definition
        hdql_Func::State * state;  // function high-level "state" (args caches)
    };

    // Create "iterator" instance based on function definition with
    // uninitialized state
    static hdql_It_t
    create( hdql_Datum_t argument, hdql_SelectionArgs_t funcDef_, hdql_Context_t ctx);
    
    // Returns previously received result or returns NULL, as defined for
    // collection's `dereference()` method
    static hdql_Datum_t
    dereference_arith( hdql_Datum_t owner
                     , hdql_It_t self_
                     , hdql_Context_t ctx
                     );

    // receive next value, arithmetic operators
    static hdql_It_t
    advance_arith( hdql_Datum_t owner
           , hdql_SelectionArgs_t sArgs
           , hdql_It_t self_
           , hdql_Context_t context
           );
    
    // receive next value, cart arg list funcs
    static hdql_It_t
    advance_cp( hdql_Datum_t owner
           , hdql_SelectionArgs_t sArgs
           , hdql_It_t self_
           , hdql_Context_t context
           );

    // receive next value, generic funcs
    static hdql_It_t
    advance_genf( hdql_Datum_t owner
           , hdql_SelectionArgs_t sArgs
           , hdql_It_t self_
           , hdql_Context_t context
           );

    // (re-)intitialize "state"
    static void reset( hdql_Datum_t owner
                     , hdql_SelectionArgs_t sArgs_
                     , hdql_It_t self_
                     , hdql_Context_t ctx
                     );

    static void destroy(hdql_It_t self_, hdql_Context_t context);
    
    static void get_key(hdql_Datum_t, hdql_It_t, struct hdql_CollectionKey *, hdql_Context_t);

    // Not implemented for functions:
    //static hdql_SelectionArgs_t compile_selection( const char * expr
    //    , const struct hdql_AttributeDefinition *
    //    , hdql_Context_t ctx
    //    , char * errBuf, size_t errBufLen) {
    //}

    static void free_selection(hdql_Context_t ctx, hdql_SelectionArgs_t);

    friend int
    ::hdql_context_new_function( struct hdql_Context * ctx
        , const char * name
        , struct hdql_Query ** argQueries
        , struct hdql_Func ** dest
        );
};

//
// hdql_Func internal methods implementation

bool
hdql_Func::_next_arguments( hdql_Datum_t root
                   , State & state
                   , hdql_Context_t context
                   ) {
    assert(_type != hdql_kGenericFunc);  // must not be invoked for gen.f.
    // fast check to prevent iteration of depleted sequence
    if(state.isExhausted) {
        assert(NULL == state.argValues[_nArgs-1]);
        return false;
    }
    assert(NULL != state.argValues[_nArgs-1]);
    // keys iterator (can be null)
    hdql_CollectionKey ** keys = state.keys;
    // datum instance pointer currently set
    hdql_Datum_t * argValue = state.argValues;
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++argValue ) {
        assert(*q);
        // retrieve next item from i-th list and consider it as an argument
        *argValue = hdql_query_get(*q, root, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        // if i-th value is retrieved, nothing else should be done with the
        // argument list
        if(NULL != *argValue) {
            state.isResutValid = 0x0;
            return true;
        }
        // i-th generator depleted
        // - if it is the last query list, we have exhausted argument list
        if(NULL == *(q+1)) {
            state.isExhausted = 0x1;
            return false;
        }
        // - otherwise, reset it and all previous and proceed with next
        {
            // supplementary iteration pointers to use at re-set
            hdql_CollectionKey ** keys2 = state.keys;
            hdql_Datum_t * argValue2 = state.argValues;
            // iterate till i-th inclusive
            for(hdql_Query ** q2 = _argQueries; q2 <= q; ++q2, ++argValue2 ) {
                assert(*q2);
                // re-set the query
                hdql_query_reset(*q2, root, context);
                // get 1st value of every query till i-th, inclusive
                *argValue2 = hdql_query_get(*q2, root, *keys2, context);
                // since we've iterated it already, a 1st lements of j-th
                // query should always exist, otherwise one of the argument
                // queris is not idempotent and that's an error
                assert(*argValue2);
                if(keys2) ++keys2;
            }
        }
    }
    assert(false);  // unforeseen loop exit, algorithm error
}

void
hdql_Func::_init_cart_state( hdql_Datum_t root
                           , State & state
                           , hdql_Context_t context
                           ) {
    assert(_type != hdql_kGenericFunc);  // must not be invoked for gen.f.
    // keys iterator (can be null)
    hdql_CollectionKey ** keys = state.keys;
    // datum instance pointer currently set
    assert(state.argValues);
    hdql_Datum_t * argValues = state.argValues;
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    bool succeed = true;
    if(_isStateful) {
        assert(_type == hdql_kCartFunc);
        _f.sfScalar.reset(_fUserdata);
    }
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++argValues ) {
        hdql_query_reset(*q, root, context);
        // retrieve next item from i-th list and consider it as an argument
        *argValues = hdql_query_get(*q, root, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        if(NULL == *argValues) {
            succeed = false;
            break;
        }
    }
    state.isResutValid = 0x0;
    state.isExhausted = succeed ? 0x0 : 0x1;
}

// Called to initialize new state for cartesian-args functions
void
hdql_Func::_alloc_state_vars(State & state, hdql_Context_t ctx) {
    // (debug) assure init state called once
    assert(NULL == state.keys);
    assert(NULL == state.argValues);
    assert(NULL == state.result);
    #ifndef NDEBUG
    {  // assure all arguments queries are set
        for(unsigned short nArg = 0; nArg < _nArgs; ++nArg) {
            assert(_argQueries[nArg] != NULL);
        }
    }
    #endif

    assert(hdql_kGenericFunc != _type);  // TODO: code below is valid only for non-generic functions
    // reserve keys array
    state.keys = reinterpret_cast<hdql_CollectionKey**>(
            hdql_context_alloc(ctx, sizeof(struct hdql_CollectionKey*)*_nArgs));
    if(_type != hdql_kGenericFunc) {
        // reserve arguments data pointers array
        state.argValues = reinterpret_cast<hdql_Datum_t *>(
                hdql_context_alloc(ctx, sizeof(hdql_Datum_t)*_nArgs));
        // Reserve keys and result data destinations, for every argument query
        //const hdql_ValueTypes * types = hdql_context_get_types(ctx);
        // for every query -- retrieve top attribute type, reserve result
        // and keys
        hdql_CollectionKey ** thisQKeys = state.keys;
        hdql_Datum_t * thisQResult = state.argValues;
        for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++thisQKeys, ++thisQResult ) {
            // keys
            hdql_query_reserve_keys_for(*q, thisQKeys, ctx);
            *thisQResult = NULL;
        }
    }
    // Function result alloc
    if(hdql_kArithOp == _type) {
        state.result = hdql_create_value(_f.arithOp.returnType, ctx);
    } else if(hdql_kCartFunc == _type) {
        if(_isStateful)
            state.result = hdql_create_value(_f.sfScalar.returnType, ctx);
        else
            state.result = hdql_create_value(_f.slScalar.returnType, ctx);
    } else if(hdql_kGenericFunc == _type) {
        state.result = hdql_create_value(_f.sfGeneric.returnType, ctx);
    }
    #ifndef NDEBUG
    else { assert(0); }
    #endif
}

void
hdql_Func::_destroy_state( State & state, hdql_Context_t context ) {
    // destroy result
    if(_type == hdql_kArithOp) {
        hdql_destroy_value(_f.arithOp.returnType, state.result, context);
    } else if(_type == hdql_kCartFunc) {
        if(_isStateful) {
             _f.sfScalar.destroy(_fUserdata);
            hdql_destroy_value(_f.sfScalar.returnType, state.result, context);
        } else {
            hdql_destroy_value(_f.slScalar.returnType, state.result, context);
        }
    } else if(_type == hdql_kGenericFunc) {
         _f.sfGeneric.destroy(_fUserdata);
        hdql_destroy_value(_f.sfGeneric.returnType, state.result, context);
    }
    #ifndef NDEBUG
    else { assert(0); }
    #endif
    // destroy args
    if(_type == hdql_kArithOp || _type == hdql_kCartFunc) {
        if(state.argValues)
            hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(state.argValues));
    }
    // free keys
    if(state.keys) {
        hdql_CollectionKey ** thisQKey = state.keys;
        for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++thisQKey/*, ++thisQResult*/ ) {
            hdql_query_destroy_keys_for(*q, *thisQKey, context);
        }
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(state.keys));
    }
}

int
hdql_Func::_eval_arith( hdql_Datum_t root
                      , State & state
                      , hdql_Context_t ctx) {
    assert(state.argValues);
    assert(state.result);
    assert(!state.isExhausted);
    assert(_type == hdql_kArithOp);

    int rc = _f.arithOp.op(state.argValues[0], state.argValues[1], state.result);
    if(rc >= 0) {
        state.isResutValid = 0x1;
    }

    return rc;
}

int
hdql_Func::_eval_sl_scalar( hdql_Datum_t root
                          , State & state
                          , hdql_Context_t ctx) {
    assert(state.argValues);
    assert(state.result);
    assert(!state.isExhausted);
    assert(_type == hdql_kCartFunc);
    assert(!_isStateful);

    int rc = _f.slScalar.call(state.argValues, state.result, ctx);
    if(rc >= 0) state.isResutValid = 0x1;
    return rc;
}

int
hdql_Func::_eval_sf_scalar( hdql_Datum_t root
                          , State & state
                          , hdql_Context_t ctx) {
    assert(state.argValues);
    assert(state.result);
    assert(!state.isExhausted);
    assert(_type == hdql_kCartFunc);
    assert(_isStateful);

    int rc = _f.sfScalar.call(_fUserdata, state.argValues, state.result, ctx);
    if(rc >= 0) state.isResutValid = 0x1;
    return rc;
}

int
hdql_Func::_eval_genf( hdql_Datum_t root
                      , State & state
                      , hdql_Context_t ctx) {
    assert(!state.argValues);
    assert(state.result);
    assert(!state.isExhausted);
    assert(_type == hdql_kGenericFunc);
    assert(_isStateful);

    int rc = _f.sfGeneric.call(_fUserdata, _argQueries, state.result, ctx);
    if(rc >= 0) state.isResutValid = 0x1;
    return rc;
}

//
// hdql_Func external methods implementation

// arithmetic function ctr
hdql_Func::hdql_Func( const struct hdql_OperationEvaluator * op
         , size_t nArgs )
        : _type(hdql_kArithOp), _nArgs(nArgs)
        , _isStateful(false), _fUserdata(NULL)
        {
    if(nArgs > 3) {
        throw std::runtime_error("Max arguments for arithmetic operator exceed");
    }
    bzero(_argQueries, sizeof(_argQueries));
    _f.arithOp = *op;
}

// cart. prod. function ctr
hdql_Func::hdql_Func( hdql_StatelessScalarFunction_t op, hdql_ValueTypeCode_t returnType
         , size_t nArgs )
        : _type(hdql_kCartFunc), _nArgs(nArgs)
        , _isStateful(false), _fUserdata(NULL)
        {
    bzero(_argQueries, sizeof(_argQueries));
    _f.slScalar.call = op;
    _f.slScalar.returnType = returnType;
}

hdql_Func::hdql_Func( const struct hdql_StatefulScalarFunction * op
         , size_t nArgs )
        : _type(hdql_kCartFunc), _nArgs(nArgs)
        , _isStateful(true), _fUserdata(NULL)
        {
    bzero(_argQueries, sizeof(_argQueries));
    _f.sfScalar = *op;
}

// generic function ctr
hdql_Func::hdql_Func( const struct hdql_GenericFunction * op
         , size_t nArgs )
        : _type(hdql_kGenericFunc), _nArgs(nArgs)
        , _isStateful(true), _fUserdata(NULL)
        {
    bzero(_argQueries, sizeof(_argQueries));
    _f.sfGeneric = *op;
}


int
hdql_Func::set_arg(size_t n, hdql_Query * q) {
    if(n >= _nArgs) return HDQL_ERR_FUNC_MAX_NARG;
    if(_argQueries[n] != NULL) return HDQL_ERR_FUNC_ARG_COLLISION;
    _argQueries[n] = q;
    return 0;
}

// TODO: temptating to make this reentrant code for use in generic
//      functions following the same behaviour. As, say, static method or smth
hdql_CollectionKey *
hdql_Func::reserve_keys(hdql_Context_t context) {
    #if 1
    assert(_type != hdql_kGenericFunc);
    #else
    if(_type == hdql_kGenericFunc) {
        if(NULL == _f.sfGeneric.reserve_keys) return NULL;  // no keys
        return _f.sfGeneric.reserve_keys(_fUserdata, _argQueries, context);
    }
    #endif
    // reserve keys for every subquery
    size_t nQueries = 0;
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q) ++nQueries;
    hdql_CollectionKey * keys = reinterpret_cast<hdql_CollectionKey*>(
            hdql_context_alloc(context, sizeof(hdql_CollectionKey)*(nQueries+1))
        );
    // sentinel
    keys[nQueries].code = HDQL_VALUE_TYPE_CODE_MAX;
    keys[nQueries].datum = NULL;
    hdql_CollectionKey * cKey = keys;
    bool failure = false;
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++cKey) {
        cKey->code = 0x0;  // TODO: uncomment
        hdql_CollectionKey * cKeyPtr = NULL;
        int rc = hdql_query_reserve_keys_for(*q, &cKeyPtr, context);
        cKey->datum = reinterpret_cast<hdql_Datum_t>(cKeyPtr);
        if(0 != rc) {
            hdql_CollectionKey * cKey2 = keys;
            for(hdql_Query ** q2 = _argQueries; q2 != q; ++q2, ++cKey2) {
                hdql_query_destroy_keys_for(*q2, cKey2, context);
                cKey2->code = 0x0;
                cKey2->datum = NULL;
            }
            break;
        }
    }
    if(failure) {
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(keys));
        return NULL;
    }
    return keys;
}

int
hdql_Func::copy_keys( hdql_CollectionKey * dest
                    , const hdql_CollectionKey * src 
                    , hdql_Context_t ctx
                    ) {
    assert(_type != hdql_kGenericFunc);
    hdql_CollectionKey * destKey = dest;
    const hdql_CollectionKey * srcKey = src;
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++srcKey, ++destKey) {
        assert(srcKey->code == destKey->code);
        int rc = hdql_query_copy_keys(*q
                , reinterpret_cast<hdql_CollectionKey*>(destKey->datum)
                , reinterpret_cast<hdql_CollectionKey*>(srcKey->datum)
                , ctx);
        if(0 != rc) {
            hdql_context_err_push(ctx, rc
                    , "copying keys for argument query %zu"
                    , _argQueries - q
                    , hdql_err_str(rc)
                    );
            return rc;
        }
    }
    return 0;
}

int
hdql_Func::call_for_each_key( hdql_CollectionKey * keys
                            , hdql_KeyIterationCallback_t callback, void * userdata
                            , size_t cLevel, size_t nKeyAtLevel
                            , hdql_Context_t context
                            ) {
    assert(_type != hdql_kGenericFunc);
    hdql_CollectionKey * k = keys;
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++k) {
        assert(*q);
        assert(k);
        assert(k->datum);
        assert(context);
        int rc = hdql_query_for_every_key( *q
                , reinterpret_cast<hdql_CollectionKey *>(k->datum)
                , context
                , callback, userdata 
                );
        if(0 != rc) {
            hdql_context_err_push(context, rc
                    , "evaluating %p for each key, at argument query %zu: %s"
                    , callback
                    , _argQueries - q
                    , hdql_err_str(rc)
                    );
            return rc;
        }
    }
    return 0;
}

int
hdql_Func::destroy_keys( hdql_CollectionKey * keys
                       , hdql_Context_t ctx
                       ) {
    assert(_type != hdql_kGenericFunc);
    hdql_CollectionKey * k = keys;
    for(hdql_Query ** q = _argQueries; NULL != *q; ++q, ++k) {
        int rc = hdql_query_destroy_keys_for( *q
                , reinterpret_cast<hdql_CollectionKey *>(k->datum)
                , ctx
                );
        if(0 != rc) {
            hdql_context_err_push(ctx, rc
                    , "destroying keys, at argument query %zu: %s"
                    , _argQueries - q
                    , hdql_err_str(rc)
                    );
            return rc;
        }
    }
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(keys));
    return 0;
}

// Create "iterator" instance based on function definition with
// uninitialized state
hdql_It_t
hdql_Func::create( hdql_Datum_t argument, hdql_SelectionArgs_t funcDef_, hdql_Context_t ctx) {
    //StateAndFunc * it = reinterpret_cast<StateAndFunc *>(hdql_context_alloc(ctx, sizeof(StateAndFunc)));
    StateAndFunc * it = hdql_allocate(ctx, StateAndFunc);
    assert(funcDef_);
    it->func = reinterpret_cast<hdql_Func*>(funcDef_);
    it->state = NULL;  // sic!, state is NOT initialized with `create()` interface, see `reset()`
    return reinterpret_cast<hdql_It_t>(it);
}

// Returns previously received result or returns NULL, as defined for
// collection's `dereference()` method
hdql_Datum_t
hdql_Func::dereference_arith( hdql_Datum_t owner
                            , hdql_It_t self_
                            , hdql_Context_t ctx
                            ) {
    assert(self_);
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    assert(NULL != self.state);
    if(self.state->isExhausted) return NULL;
    if(!self.state->isResutValid) {
        assert(self.func->_type == hdql_kArithOp);
        int rc = self.func->_eval_arith(owner, *self.state, ctx);
        if(0 != rc) {
            hdql_context_err_push(ctx, rc
                    , "evaluating arithmetic operation at argument query"
                    );
            return NULL;
        }
    }
    assert(self.state->isResutValid);
    return self.state->result;
}

// receive next value, arithmetic operators
hdql_It_t
hdql_Func::advance_arith( hdql_Datum_t owner
       , hdql_SelectionArgs_t sArgs
       , hdql_It_t self_
       , hdql_Context_t context
       ) {
    assert(self_);
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    self.state->isExhausted
            = self.func->_next_arguments(owner, *self.state, context)
            ? 0x0
            : 0x1
            ;
    return self_;
}

// receive next value, cart arg list funcs
hdql_It_t
hdql_Func::advance_cp( hdql_Datum_t owner
       , hdql_SelectionArgs_t sArgs
       , hdql_It_t self_
       , hdql_Context_t context
       ) {
    #if 1
    assert(0);  // TODO
    #else
    assert(self_);
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    self.state->isExhausted
        = self.func->_eval_cpf(owner, *self.state, context)
        ? 0x0
        : 0x1
        ;
    return self_;
    #endif
}

// receive next value, generic funcs
hdql_It_t
hdql_Func::advance_genf( hdql_Datum_t owner
       , hdql_SelectionArgs_t sArgs
       , hdql_It_t self_
       , hdql_Context_t context
       ) {
    #if 1
    assert(0);
    #else
    assert(self_);
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    self.state->isExhausted
        = self.func->_eval_genf(owner, *self.state, context)
        ? 0x0
        : 0x1
        ;
    return self_;
    #endif
}

// (re-)intitialize "state"
void
hdql_Func::reset( hdql_Datum_t owner
                 , hdql_SelectionArgs_t sArgs_
                 , hdql_It_t self_
                 , hdql_Context_t ctx
                 ) {
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    if(NULL == self.state) {
        // if state is not yet allocated, this is the first invocation
        self.state = reinterpret_cast<State*>(hdql_context_alloc(ctx, sizeof(State)));
        bzero(self.state, sizeof(State));
        // init state based on sub-query types, keys and result type
        self.func->_alloc_state_vars(*self.state, ctx);
        assert(self.state->keys);
        assert(self.state->argValues);
        assert(self.state->result);
    }
    if(self.func->_type != hdql_kGenericFunc) {
        self.func->_init_cart_state(owner, *self.state, ctx);
        // freeing state here for empty arguments seem to clutter other
        // code with redundant conditions...
        #if 0
        if(self.state->isExhausted) {
            // state initialization failed immediately => empty set of
            // arguments, delete allocated memory and set it to NULL preventing
            // further iteration
            hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(self.state));
            self.state = NULL;
            std::cout << " xxx empty args list!" << std::endl;  // XXX
        }
        #endif
    } else {
        //self.state->
        assert(0);  // TODO: generic func
    }
}

void
hdql_Func::destroy(hdql_It_t self_, hdql_Context_t context) {
    assert(self_);
    StateAndFunc * selfPtr = reinterpret_cast<StateAndFunc*>(self_);
    if(selfPtr->state) {
        selfPtr->func->_destroy_state(*selfPtr->state, context);
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(selfPtr->state));
    }
    for(hdql_Query ** q = selfPtr->func->_argQueries; NULL != *q; ++q) {
        hdql_query_destroy(*q, context);
    }
    hdql_func_destroy(selfPtr->func, context);
    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(self_));
}

void
hdql_Func::get_key( hdql_Datum_t item
                  , hdql_It_t self_
                  , struct hdql_CollectionKey * destKeysWrapper
                  , hdql_Context_t ctx
                  ) {
    StateAndFunc & self = *reinterpret_cast<StateAndFunc*>(self_);
    assert(NULL !=  self.state->keys);
    assert(NULL != *self.state->keys);
    hdql_CollectionKey ** srcKeys = self.state->keys;
    struct hdql_CollectionKey * destKeys = reinterpret_cast<hdql_CollectionKey *>(destKeysWrapper->datum);
    for(hdql_Query ** q = self.func->_argQueries; NULL != *q; ++q, ++srcKeys, ++destKeys) {
        int rc = hdql_query_copy_keys( *q
                , reinterpret_cast<hdql_CollectionKey *>(destKeys->datum)
                , *srcKeys
                , ctx
                );
        if(0 != rc) {
            hdql_context_err_push(ctx, rc
                    , "retrieving key, at argument query %zu, key copy returned %d (%s)"
                    , self.func->_argQueries - q
                    , rc
                    , hdql_err_str(rc)
                    );
        }
    }
}

//static hdql_SelectionArgs_t compile_selection( const char * expr
//    , const struct hdql_AttributeDefinition *
//    , hdql_Context_t ctx
//    , char * errBuf, size_t errBufLen) {
//}

void
hdql_Func::free_selection( hdql_Context_t ctx
                         , hdql_SelectionArgs_t fDef_
                         ) {
    //struct hdql_Func * fDef = reinterpret_cast<struct hdql_Func *>(fDef_);
    //assert(0);  // TODO?
}

//
// C API

extern "C" struct hdql_Func *
hdql_func_create_arith( hdql_Context_t ctx
                      , const struct hdql_OperationEvaluator * op
                      , size_t nArgs
                      ) {
    hdql_Datum_t r_ = hdql_context_alloc(ctx, sizeof(hdql_Func));
    return new (r_) hdql_Func(op, nArgs);  // TODO: context-based alloc, placement new
}

extern "C" void
hdql_func_destroy(struct hdql_Func * f, hdql_Context_t ctx) {
    f->~hdql_Func();
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(f));
}

extern "C" int
hdql_func_set_arg( struct hdql_Func * fDef
                 , size_t n
                 , struct hdql_Query * q
                 ) {
    return fDef->set_arg(n, q);
}

extern "C" int
hdql_func_reserve_keys( struct hdql_Func * fDef
                      , hdql_CollectionKey ** keys
                      , hdql_Context_t ctx
                      ) {
    assert(fDef);
    assert(keys);
    assert(ctx);
    *keys = fDef->reserve_keys(ctx);
    return 0;
}

extern "C" int
hdql_func_call_on_keys( struct hdql_Func * fDef
                      , struct hdql_CollectionKey * keys
                      , hdql_KeyIterationCallback_t callback, void * userdata
                      , size_t cLevel, size_t nKeyAtLevel
                      , hdql_Context_t ctx
                      ) {
    return fDef->call_for_each_key(keys, callback, userdata, cLevel, nKeyAtLevel, ctx);
}

extern "C" int
hdql_func_destroy_keys( struct hdql_Func * fDef
                      , struct hdql_CollectionKey * keys
                      , hdql_Context_t ctx
                      ) {
    return fDef->destroy_keys(keys, ctx);
}

extern "C" int
hdql_func_copy_keys( struct hdql_Func * fDef
                   , hdql_CollectionKey * dest
                   , const hdql_CollectionKey * src
                   , hdql_Context_t ctx
                   ) {
    return fDef->copy_keys(dest, src, ctx);
}

//
// Interface implementations

static const hdql_CollectionAttrInterface gArithCollectionInterface = {
      0x0
    , hdql_Func::create
    , hdql_Func::dereference_arith
    , hdql_Func::advance_arith
    , hdql_Func::reset
    , hdql_Func::destroy
    , hdql_Func::get_key
    , NULL  //hdql::OpCollectionInterface::compile_selection
    , hdql_Func::free_selection
};
const hdql_CollectionAttrInterface * hdql_gArithOpIFace = &gArithCollectionInterface;

//
// Implementation of functions dictionary methods

extern "C" hdql_Func *
hdql_func_create( const char * name
        , hdql_Query ** arguments
        , hdql_Context_t context
        ) {
    struct hdql_Functions * fs = hdql_context_get_functions(context);
    assert(fs);
    auto variants = fs->equal_range(name);
    if(variants.first == variants.second) return NULL;
    hdql_Datum_t bf = hdql_context_alloc(context, sizeof(struct hdql_Func));
    for(auto cv = variants.first; cv != variants.second; ++cv) {
        if(cv->second.type == hdql_kCartFunc) {
            if(cv->second.isStateful) {
                assert(0);  // TODO: stateful scalar function, ruled by args
            } else {
                assert(0);  // XXX
                // TODO: check arguments
                //if(nArgs != cv->second.nArgs) continue;
                //return new (bf) hdql_Func(cv->second.ctr.slScalar.call, cv->second.);
            }
            break;
        }
        if(cv->second.type == hdql_kGenericFunc) {
            assert(0);  //TODO: generic function
            break;
        }
        assert(0);  // unknown type
    }
    assert(0);  // TODO
}

//
// Common functions

#if 0
// common function cache for all the aggregate function (like `any()`,
// `all()`, `sum()` etc)
struct AggregateArgs {
    hdql_ValueInterface * vis;
};

// Implements "all" requirement for all the elements in query argument(s).
// Argument queries must be terminated by NULL and provide atomic results
// with to-bool interface.
// One can consider `all()` as concatenation of all args via AND condition.
static int
_f_logic_collection_all_single( const hdql_Datum_t instance
        , struct hdql_Query ** argQueries
        , hdql_Datum_t result
        ) {
    // ...
}
#endif

