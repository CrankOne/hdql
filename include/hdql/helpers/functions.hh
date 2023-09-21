#pragma once

#include <stdexcept>
#include <tuple>
#include <cassert>
#include <type_traits>
#include <typeindex>

#include <iostream>  // XXX

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/query.h"
#include "hdql/function.h"
#include "hdql/value.h"

/**\brief C++ template helpers for HDQL API
 * */

namespace hdql {
namespace helpers {
namespace detail {

template<int...> struct index_tuple{}; 
template<int I, typename IndexTuple, typename... Types> 
struct make_indexes_impl; 

template<int I, int... indexes, typename T, typename ... Types> 
struct make_indexes_impl<I, index_tuple<indexes...>, T, Types...> {
    typedef typename make_indexes_impl<I + 1, index_tuple<indexes..., I>, Types...>::type type; 
};

template<int I, int... indexes>
struct make_indexes_impl<I, index_tuple<indexes...> > { 
    typedef index_tuple<indexes...> type; 
};

template<typename ... Types>
struct make_indexes : make_indexes_impl<0, index_tuple<>, Types...> {};

template< typename RetT
        , typename ... ArgsTs
        , int ... indexes > RetT
apply_helper( RetT (*pf)(ArgsTs...)
            , index_tuple< indexes... >
            , struct hdql_Datum ** args
            ) {
    //return pf( std::forward<ArgsTs>( std::get<indexes>(tpl))... );
    return pf( std::forward<ArgsTs>( *reinterpret_cast<typename std::remove_reference<ArgsTs>::type*>(args[indexes]) )... );
}
}  // namespace ::hdql::helpers::detail

template<class RetT, class ... ArgsT> RetT
apply(RetT (*pf)(ArgsT...), struct hdql_Datum ** args) {
    return detail::apply_helper( pf
                       , typename detail::make_indexes<ArgsT...>::type()
                       , args
                       );
}

/**\brief Helper struct automatically instantiating function interfaces
 *
 * Automatic implementation provided by this template is restricted with
 * following rules:
 *  - Number of arguments must be known
 *  - Considered C/C++ function should map N (N > 0) arguments of scalar atomic
 *    or non-virtual scalars into a scalar or non-virtual compound
 * Despite this is rather wide class of C/C++ functions (e.g. all maths,
 * trigonometric, etc), thisimplementation can be somewhat restrictive as it
 * does not support variadic arguments, virtual compounds, etc.
 * */
template<typename ResultT, typename ...ArgsT >
struct AutoFunction {
    typedef ResultT (*FuncPtr)(ArgsT ...);

    /**\brief Function instantiation arguments considered as def.data by iface
     *
     * Caveat: for attribute access functions this instance is considered as
     * const definition data, yet it is in fact a synthetic object provided by
     * function constructor. */
    struct FunctionInstantiationArgs {
        hdql_Query ** args;
        FuncPtr functionPointer;
    };

    /**\brief Function result calculation cache */
    struct ScalarState {
        ResultT result;
        hdql_Datum_t * argValuesPtrs;
        hdql_Datum_t (**converters)(hdql_Datum_t);
        bool isValid, isExhausted;
    };

    static hdql_Datum_t
    instantiate( hdql_Datum_t root
               , const hdql_Datum_t fInstArgs_
               , hdql_Context_t context
               ) {
        ScalarState * state = hdql_alloc(context, ScalarState);
        FunctionInstantiationArgs * fInstArgs = const_cast<FunctionInstantiationArgs*>(
                reinterpret_cast<FunctionInstantiationArgs *>(fInstArgs_));
        /* Allocate array of pointers for argument values */
        size_t nArgs = 0;
        for(hdql_Query ** q = fInstArgs->args; NULL != *q; ++q, ++nArgs) {
            #ifndef NDEBUG
            assert(nArgs <= sizeof...(ArgsT));
            // TODO: check convertible with type nArgs
            #endif
        }
        state->argValuesPtrs = reinterpret_cast<hdql_Datum_t*>(
                hdql_context_alloc(context, sizeof(hdql_Datum_t*)*nArgs));
        bzero(state->argValuesPtrs, sizeof(hdql_Datum_t*)*nArgs);
        return reinterpret_cast<hdql_Datum_t>(state);
    }

    static hdql_Datum_t
    dereference( hdql_Datum_t root
               , hdql_Datum_t state_
               , struct hdql_CollectionKey * key
               , const hdql_Datum_t fInstArgs_
               , hdql_Context_t context
            ) {
        ScalarState * state = reinterpret_cast<ScalarState*>(state_);
        if(state->isExhausted) return NULL;  // TODO: must be prohibited, in principle...
        if(!state->isValid) {
            FunctionInstantiationArgs * fInstArgs = const_cast<FunctionInstantiationArgs*>(
                reinterpret_cast<FunctionInstantiationArgs *>(fInstArgs_));
            size_t nArg = 0;
            for(hdql_Query ** q = fInstArgs->args; NULL != *q; ++q, ++nArg) {
                state->argValuesPtrs[nArg] = hdql_query_get(*q, NULL, context);
                if(NULL == fInstArgs->args[nArg]) {
                    // at least one of the arguments did not provide a value;
                    // we consider result as empty
                    state->isExhausted = true;
                    return NULL;
                }
            }
            state->result = apply(fInstArgs->functionPointer, state->argValuesPtrs);
            state->isValid = true;
        }
        return reinterpret_cast<hdql_Datum_t>(&state->result);
    }

    static hdql_Datum_t
    reset( hdql_Datum_t newOwner
         , hdql_Datum_t state_
         , const hdql_Datum_t fInstArgs_
         , hdql_Context_t ctx
         ) {
        assert(state_);
        FunctionInstantiationArgs * fInstArgs = const_cast<FunctionInstantiationArgs*>(
                reinterpret_cast<FunctionInstantiationArgs *>(fInstArgs_));
        ScalarState * state = reinterpret_cast<ScalarState*>(state_);
        for(hdql_Query ** q = fInstArgs->args; NULL != *q; ++q) {
            hdql_query_reset(*q, newOwner, ctx);
        }
        state->isExhausted = state->isValid = false;
        return state_;
    }

    static void
    destroy(
          hdql_Datum_t state_
        , const hdql_Datum_t fInstArgs_
        , hdql_Context_t ctx
        ) {
        FunctionInstantiationArgs * fInstArgs = const_cast<FunctionInstantiationArgs*>(
                reinterpret_cast<FunctionInstantiationArgs *>(fInstArgs_));
        if(state_) {
            ScalarState * state = reinterpret_cast<ScalarState*>(state_);
            if(state->argValuesPtrs) hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(state->argValuesPtrs));
            hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(state));
        }
        if(fInstArgs_) {
            for(hdql_Query ** q = fInstArgs->args; NULL != *q; ++q) {
                hdql_query_destroy(*q, ctx);
            }
        }
    }

    /**\brief Instantiates attribute definition corresponding to a particular
     *        function
     *
     * Instantiation failures must not be considered as critical error as this
     * "constructor" is a part of "overloading" function selection mechanism.
     * */
    static hdql_AttrDef *
    construct( struct hdql_Query ** argQs
             , void * func_
             , char * failureBuffer
             , size_t failureBufferSize
             , hdql_Context_t context
             ) {
        hdql_ValueTypes * vts = hdql_context_get_types(context);
        assert(vts);
        //
        // iterate over queries, checking that input signature match
        size_t nArg = 0;
        const std::type_info * argTypes[sizeof...(ArgsT)] = {
            &typeid(typename std::remove_reference<typename std::remove_const<ArgsT>::type>::type)... };
        for(hdql_Query ** q = argQs; NULL != *q; ++q, ++nArg) {
            // basic checks for is scalar / argument number match
            if(nArg >= sizeof...(ArgsT)) {
                if(failureBuffer) {
                    snprintf( failureBuffer, failureBufferSize
                            , "Expected %lu arguments"
                            , sizeof...(ArgsT)
                            );
                }
                return NULL;
            }
            if(!hdql_query_is_fully_scalar(*q)) {
                if(failureBuffer) {
                    snprintf( failureBuffer, failureBufferSize
                            , "Argument #%lu is a collection while scalar is expected"
                            , nArg + 1
                            );
                }
                return NULL;  // not a scalar arg
            }

            const hdql_AttrDef * argTQDef = hdql_query_top_attr(*q);
            if(hdql_attr_def_is_compound(argTQDef)) {
                // TODO: support for compound arguments?
                if(failureBuffer) {
                    const hdql_Compound * ct = hdql_attr_def_compound_type_info(argTQDef);
                    if(hdql_compound_is_virtual(ct)) {
                        ct = hdql_virtual_compound_get_parent(ct);
                        snprintf( failureBuffer, failureBufferSize
                            , "Argument #%lu is of virtual compound type based on %s, "
                              " atomic type expected."
                            , nArg + 1, hdql_compound_get_name(ct)
                            );
                    } else {
                        snprintf( failureBuffer, failureBufferSize
                            , "Argument #%lu is of compound type %s, "
                              " atomic type expected."
                            , nArg + 1, hdql_compound_get_name(ct)
                            );
                    }
                }
                return NULL;
            } else {
                char typeBf[64];
                hdql_cxx_demangle(argTypes[nArg]->name(), typeBf, sizeof(typeBf)); 
                // check N argument type provided by template pack
                hdql_ValueTypeCode_t cfArgTCode
                        = hdql_types_get_type_code(vts, typeBf)
                    , qArgTCode = hdql_attr_def_get_atomic_value_type_code(argTQDef);
                // Assertion below must be prevented by the language itself --
                // non-compound (atomic) type which can not be resolved
                assert(qArgTCode != 0x0);
                if(cfArgTCode == 0x0) {
                    if(failureBuffer) {
                        snprintf( failureBuffer, failureBufferSize
                                , "Function candidate is not available as"
                                  " argument #%lu of type `%s' can not be"
                                  " resolved to any of HDQL types"
                                , nArg + 1
                                , typeBf 
                                );
                    }
                    return NULL;
                }
                if(cfArgTCode != qArgTCode) {

                    // TODO: support atomic type conversion
                    if(failureBuffer) {
                        snprintf( failureBuffer, failureBufferSize
                                , "Can not convert argument #%lu of type <%s> to expected type <%s>"
                                  " (C/C++ type `%s')"
                                , nArg + 1
                                , qArgTCode ? hdql_types_get_type(vts, qArgTCode)->name : "?"
                                , cfArgTCode ? hdql_types_get_type(vts, cfArgTCode)->name : "?"
                                , typeBf
                                );
                    }
                    return NULL;
                } else {
                    continue;  // atomic arg must be just forwarded to function
                }
            }
            assert(0);  // unreachable code
        }
        assert(0 != nArg);
        //
        // Return type
        hdql_AtomicTypeFeatures retFts; {
            char typeBf[64];
            const std::type_index & retTI
                = typeid(typename std::remove_reference<typename std::remove_const<ResultT>::type>::type);
            hdql_cxx_demangle(retTI.name(), typeBf, sizeof(typeBf)); 
            retFts.isReadOnly = 0x1;
            retFts.arithTypeCode = hdql_types_get_type_code(vts, typeBf);
            if(0x0 == retFts.arithTypeCode) {
                if(failureBuffer) {
                    snprintf( failureBuffer, failureBufferSize
                            , "Function candidate is not available as"
                              " return value of type `%s' can not be"
                              " resolved to any of HDQL types"
                            , typeBf 
                            );
                    return NULL;
                }
            }
        }

        FunctionInstantiationArgs * fArgs = hdql_alloc(context, FunctionInstantiationArgs);
        fArgs->args = reinterpret_cast<hdql_Query **>(
                hdql_context_alloc(context, sizeof(struct hdql_Query *)*(nArg+1)) );
        memcpy(fArgs->args, argQs, sizeof(struct hdql_Query *)*(nArg +1));
        fArgs->functionPointer=reinterpret_cast<FuncPtr>(func_);

        // instantiate scalar interface
        hdql_ScalarAttrInterface iface{
                .definitionData = reinterpret_cast<hdql_Datum_t>(fArgs),
                .instantiate = instantiate,
                .dereference = dereference,
                .reset = reset,
                .destroy = destroy,
            };
        return hdql_attr_def_create_atomic_scalar(
                  &retFts
                , &iface
                , 0x0  // keyTypeCode
                , NULL  // hdql_ReserveKeysListCallback_t keyIFace
                , context
                );
    }
};  // struct AutoFunction

template<typename ResultT, typename ...ArgsT> hdql_FunctionConstructor_t
math_f_construct(ResultT (*f)(ArgsT...)) {
    return AutoFunction<ResultT, ArgsT...>::construct;
}

}  // namespace ::hdql::helpers
}  // namespace hdql

