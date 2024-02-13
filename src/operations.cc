#include "hdql/operations.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/compound.h"
#include "hdql/query.h"

#include "hdql/helpers/compounds.hh"

#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>

#include <iostream>  // XXX

namespace hdql {

//
// Operator implementations: common binary arithmetic

template<typename T>
struct DynamicTraits {
    static hdql_ValueTypeCode_t tCode;
};

template<typename T> hdql_ValueTypeCode_t DynamicTraits<T>::tCode = 0x0;

template<typename T1, typename T2, typename EnableT=void>
struct ArithmeticOpTraits;

template<typename T1, typename T2>
struct ArithmeticOpTraits< T1, T2
    , typename std::enable_if<  std::is_floating_point<T1>::value
                             || std::is_floating_point<T2>::value
                             >::type > {
    typedef hdql_Flt_t Result_t;
};

template<typename T1, typename T2>
struct ArithmeticOpTraits< T1, T2
    , typename std::enable_if<  std::is_integral<T1>::value
                             && std::is_integral<T2>::value
                             >::type > {
    typedef hdql_Int_t Result_t;
};

#define _M_for_each_integer_type(m, ...) \
    m( __VA_ARGS__, int8_t    ) \
    m( __VA_ARGS__, uint8_t   ) \
    m( __VA_ARGS__, int16_t   ) \
    m( __VA_ARGS__, uint16_t  ) \
    m( __VA_ARGS__, int32_t   ) \
    m( __VA_ARGS__, uint32_t  ) \
    m( __VA_ARGS__, int64_t   ) \
    m( __VA_ARGS__, uint64_t  )

#define _M_for_each_numerical_type(m, ...) \
    _M_for_each_integer_type(m, __VA_ARGS__) \
    m( __VA_ARGS__, float     ) \
    m( __VA_ARGS__, double    )

#define _M_for_each_atomic_type(m, ...) \
    _M_for_each_numerical_type(m, __VA_ARGS__) \
    m( __VA_ARGS__, hdql_Bool_t )

#define _M_for_each_type(m, ...) \
    _M_for_each_atomic_type(m, __VA_ARGS__) \
    m( __VA_ARGS__, hdql_StrPtr_t )

#define _M_EXPAND(M) M

#define _M_for_each_integer_type_() _M_for_each_integer_type
#define _M_for_each_integer_type__(...) _M_for_each_integer_type_ _M_EXPAND(()) (__VA_ARGS__)

#define _M_for_each_numerical_type_() _M_for_each_numerical_type
#define _M_for_each_numerical_type__(...) _M_for_each_numerical_type_ _M_EXPAND(()) (__VA_ARGS__)

#define _M_for_each_atomic_type_() _M_for_each_atomic_type
#define _M_for_each_atomic_type__(...) _M_for_each_atomic_type_ _M_EXPAND(()) (__VA_ARGS__)

#define _M_for_each_type_() _M_for_each_type
#define _M_for_each_type__(...) _M_for_each_type_ _M_EXPAND(()) (__VA_ARGS__)

// -- implement basic conversions ---------------------------------------------

namespace {
template<typename T1, typename T2>
struct Converter {
    static int
    convert( struct hdql_Datum * __restrict__ dest
           , struct hdql_Datum * __restrict__ src ) {
        *reinterpret_cast<T1 * __restrict__>(dest) = *reinterpret_cast<T2 * __restrict__>(src);
        return 0;
    }
    static int
    add(hdql_ValueTypes * vts, hdql_Converters & cnvs) {
        hdql_ValueTypeCode_t toVTC   = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<T1>::name)
                           , fromVTC = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<T2>::name)
                           ;
        return hdql_converters_add( &cnvs
                                  , toVTC
                                  , fromVTC
                                  , convert );
    }
};  // struct Converter

template<typename T2>
struct Converter<hdql_StrPtr_t, T2> {
    static int
    convert( struct hdql_Datum * __restrict__ dest
           , struct hdql_Datum * __restrict__ src ) {
        // TODO: this is rather draft implementation, potentially dangerous
        // because of leaks, inefficient, lack of formatting manipulators, etc
        std::string strrepr;
        {
            std::ostringstream oss;
            oss << *reinterpret_cast<T2 * __restrict__>(src);
            strrepr = oss.str();
        }
        *reinterpret_cast<hdql_StrPtr_t * __restrict__>(dest) = strdup(strrepr.c_str());
        return 0;
    }
    static int
    add(hdql_ValueTypes * vts, hdql_Converters & cnvs) {
        hdql_ValueTypeCode_t toVTC   = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<hdql_StrPtr_t>::name)
                           , fromVTC = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<T2>::name)
                           ;
        return hdql_converters_add( &cnvs
                                  , toVTC
                                  , fromVTC
                                  , convert );
    }
};  // struct Converter<hdql_StrPtr_t, ...>

template<>
struct Converter<hdql_StrPtr_t, hdql_Bool_t> {
    static int
    convert( struct hdql_Datum * __restrict__ dest
           , struct hdql_Datum * __restrict__ src ) {
        *reinterpret_cast<hdql_StrPtr_t * __restrict__>(dest)
                = (*reinterpret_cast<hdql_Bool_t * __restrict__>(src))
                ? strdup("true")
                : strdup("false");
        return 0;
    }
    static int
    add(hdql_ValueTypes * vts, hdql_Converters & cnvs) {
        hdql_ValueTypeCode_t toVTC   = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<hdql_StrPtr_t>::name)
                           , fromVTC = hdql_types_get_type_code(vts, hdql::helpers::detail::ArithTypeNames<hdql_Bool_t>::name)
                           ;
        return hdql_converters_add( &cnvs
                                  , toVTC
                                  , fromVTC
                                  , convert );
    }
};  // struct Converter<hdql_StrPtr_t, bool>

}  // empty ns

extern "C" void
hdql_converters_add_std(hdql_Converters *cnvs, hdql_ValueTypes * vts) {
    #define _M_add_conversion_implem(t1, t2) Converter<t1, t2>::add(vts, *cnvs);
    // NOTE: we do not add here bool -> int conversion to prevent unintended
    // mistakes when boolean result is provided to a numerical function.
    _M_EXPAND(_M_for_each_numerical_type(_M_for_each_numerical_type__, _M_add_conversion_implem));
    // ...while inverse situation, when numerical result should be considered
    // as logical one seem to be natural
    #define _M_add_num_to_bool_conversion(_, t1) \
        _M_add_conversion_implem(bool, t1)
    _M_for_each_numerical_type(_M_add_num_to_bool_conversion)
    #undef _M_add_num_to_bool_conversion

    // to-string conversions
    #define _M_add_atomic_to_str_conversion(_, t1) \
        _M_add_conversion_implem(hdql_StrPtr_t, t1)
    _M_for_each_atomic_type(_M_add_atomic_to_str_conversion)
    #undef _M_add_atomic_to_str_conversion

    #undef _M_add_conversion_implem
}

// -- implement binary arithmetic ---------------------------------------------
#define _M_for_every_binary_arith_op(m, ...) \
    m( Sum,     +, __VA_ARGS__ ) \
    m( Minus,   -, __VA_ARGS__ ) \
    m( Product, *, __VA_ARGS__ ) \
    m( Divide,  /, __VA_ARGS__ )

#define _M_implement_arith_op(op, sign, t1, t2) \
    static int _ ## t1 ## _ ## op ## _ ## t2 (const hdql_Datum_t a_, const hdql_Datum_t b_, hdql_Datum_t r_) { \
        assert(a_); \
        assert(b_); \
        assert(r_); \
        *reinterpret_cast<ArithmeticOpTraits<t1, t2>::Result_t *>(r_) \
            = *reinterpret_cast<const t1 *>(a_) \
            sign *reinterpret_cast<const t2 *>(b_); \
        return 0; \
    }

#define _M_implement_arith_ops(t1, t2) \
    _M_for_every_binary_arith_op(_M_implement_arith_op, t1, t2)

_M_EXPAND(_M_for_each_numerical_type(_M_for_each_atomic_type__, _M_implement_arith_ops))
#undef _M_implement_arith_ops
#undef _M_implement_arith_op

// -- implement binary logic --------------------------------------------------
#define _M_for_every_binary_logic_op(m, ...) \
    m( Or,    ||, __VA_ARGS__ ) \
    m( And,   &&, __VA_ARGS__ ) \
    m( XOr,   !=, __VA_ARGS__ )

#define _M_implement_logic_op(op, sign, t1, t2) \
    static int _ ## t1 ## _L ## op ## _ ## t2 (const hdql_Datum_t a_, const hdql_Datum_t b_, hdql_Datum_t r_) { \
        assert(a_); \
        assert(b_); \
        assert(r_); \
        *reinterpret_cast<hdql_Bool_t *>(r_) \
            =   (((bool) *reinterpret_cast<const t1 *>(a_)) \
            sign ((bool) *reinterpret_cast<const t2 *>(b_))); \
        return 0; \
    }

#define _M_implement_logic_ops(t1, t2) \
    _M_for_every_binary_logic_op(_M_implement_logic_op, t1, t2)

_M_EXPAND(_M_for_each_atomic_type(_M_for_each_atomic_type__, _M_implement_logic_ops))
#undef _M_implement_logic_ops
#undef _M_implement_logic_op

// --- implement binary comparison --------------------------------------------

#define _M_for_every_binary_comparison_op(m, ...) \
    m( GT,    >,  __VA_ARGS__ ) \
    m( GTE,   >=, __VA_ARGS__ ) \
    m( LT,    <,  __VA_ARGS__ ) \
    m( LTE,   <=, __VA_ARGS__ )

#define _M_implement_comparison_op(op, sign, t1, t2) \
    static int _ ## t1 ## _ ## op ## _ ## t2 (const hdql_Datum_t a_, const hdql_Datum_t b_, hdql_Datum_t r_) { \
        *reinterpret_cast<hdql_Bool_t *>(r_) \
            = *reinterpret_cast<const t1 *>(a_) \
            sign *reinterpret_cast<const t2 *>(b_); \
        return 0; \
    }

#define _M_implement_comparison_ops(t1, t2) \
    _M_for_every_binary_comparison_op(_M_implement_comparison_op, t1, t2)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
_M_EXPAND(_M_for_each_numerical_type(_M_for_each_atomic_type__, _M_implement_comparison_ops))
#pragma GCC diagnostic pop

#undef _M_implement_comparison_ops
#undef _M_implement_comparison_op

// --- implement integer arithmetics ------------------------------------------

#define _M_for_every_integer_arithmetics_op(m, ...) \
    m( Modulo,    %,   __VA_ARGS__ ) \
    m( RBShift,   >>,  __VA_ARGS__ ) \
    m( LBShift,   <<,  __VA_ARGS__ ) \
    m( BAnd,      &,   __VA_ARGS__ ) \
    m( BXOr,      ^,   __VA_ARGS__ ) \
    m( BOr,       |,   __VA_ARGS__ ) \

#define _M_implement_integer_arithmetic_op(op, sign, t1, t2) \
    static int _ ## t1 ## _ ## op ## _ ## t2 (const hdql_Datum_t a_, const hdql_Datum_t b_, hdql_Datum_t r_) { \
        *reinterpret_cast<hdql_Int_t *>(r_) \
            = *reinterpret_cast<const t1 *>(a_) \
            sign *reinterpret_cast<const t2 *>(b_); \
        return 0; \
    }

#define _M_implement_integer_arithmetic_ops(t1, t2) \
    _M_for_every_integer_arithmetics_op(_M_implement_integer_arithmetic_op, t1, t2)

_M_EXPAND(_M_for_each_integer_type(_M_for_each_integer_type__, _M_implement_integer_arithmetic_ops))
#undef _M_implement_integer_arithmetic_ops
#undef _M_implement_integer_arithmetic_op

// --- unary operations implementation ----------------------------------------

template<typename T, typename enable=void> struct UnaryMinusSpec;

template<typename T>
struct UnaryMinusSpec<T, typename std::enable_if<std::is_signed<T>::value>::type> {
    typedef T Result_t;
    static int op(const hdql_Datum_t a_, const hdql_Datum_t unused_, hdql_Datum_t r_) {
        assert(NULL == unused_);
        *reinterpret_cast<Result_t*>(r_) = - *reinterpret_cast<T*>(a_);
        return 0;
    }
};

template<typename T>
struct UnaryMinusSpec<T, typename std::enable_if<(!std::is_signed<T>::value) && std::is_integral<T>::value>::type> {
    typedef hdql_Int_t Result_t;
    static int op(const hdql_Datum_t a_, const hdql_Datum_t unused_, hdql_Datum_t r_) {
        assert(NULL == unused_);
        *reinterpret_cast<hdql_Int_t*>(r_) = - static_cast<hdql_Int_t>(*reinterpret_cast<T*>(a_));
        return 0;
    }
};

template<typename T>
struct UnaryMinusSpec<T, typename std::enable_if<(!std::is_signed<T>::value) && std::is_floating_point<T>::value>::type> {
    typedef hdql_Flt_t Result_t;
    static int op(const hdql_Datum_t a_, const hdql_Datum_t unused_, hdql_Datum_t r_) {
        assert(NULL == unused_);
        *reinterpret_cast<hdql_Flt_t*>(r_) = - static_cast<hdql_Flt_t>(*reinterpret_cast<T*>(a_));
        return 0;
    }
};



template<typename T> int
_T_unary_b_not(const hdql_Datum_t a_, const hdql_Datum_t unused_, hdql_Datum_t r_) {
    static_assert(std::is_integral<T>::value);
    assert(NULL == unused_);
    *reinterpret_cast<T*>(r_) = ~ *reinterpret_cast<T*>(a_);
    return 0;
}

template<typename T> static int
_T_unary_l_not(const hdql_Datum_t a_, const hdql_Datum_t unused_, hdql_Datum_t r_) {
    assert(NULL == unused_);
    *reinterpret_cast<hdql_Bool_t*>(r_) = ! *reinterpret_cast<T*>(a_);
    return 0;
}

//
// (typed) operation lookup key

struct OpKey {
    hdql_ValueTypeCode_t t1, t2;
    hdql_OperationCode_t opCode;

    bool operator==(const OpKey & other) const {
        return t1 == other.t1 && t2 == other.t2 && opCode == other.opCode;
    }
};

}  // namespace hdql

template <>
struct std::hash<hdql::OpKey> {
  std::size_t operator()(const hdql::OpKey & k) const
  {
    using std::size_t;
    using std::hash;

    return (k.opCode << 2*HDQL_VALUE_TYPEDEF_CODE_BITSIZE)
         | (k.t1     <<   HDQL_VALUE_TYPEDEF_CODE_BITSIZE)
         |  k.t2
         ;
  }
};

struct hdql_Operations : public std::unordered_map<hdql::OpKey, hdql_OperationEvaluator> {
    hdql_Operations * parent;
    hdql_Operations(hdql_Operations * parent_) : parent(parent_) {}
};

extern "C" {

int
hdql_op_define( struct hdql_Operations * ops
        , hdql_ValueTypeCode_t t1
        , hdql_OperationCode_t opCode
        , hdql_ValueTypeCode_t t2
        , hdql_OperationEvaluator_t evaluator
        ) {
    hdql::OpKey opKey{t1, t2, opCode};
    auto ir = ops->emplace(opKey, *evaluator);
    if(!ir.second) return -1;
    return 0;
}

const hdql_OperationEvaluator *
hdql_op_get( const struct hdql_Operations * ops
        , hdql_ValueTypeCode_t t1
        , hdql_OperationCode_t opCode
        , hdql_ValueTypeCode_t t2
        ) {
    hdql::OpKey opKey{t1, t2, opCode};
    auto it = ops->find(opKey);
    if(ops->end() != it) return &(it->second);
    if(ops->parent) return hdql_op_get(ops->parent, t1, opCode, t2);
    return NULL;
}

int
hdql_op_define_std_arith( struct hdql_Operations * operations
                        , struct hdql_ValueTypes * types
                        ) {

    #define _M_get_type_code(unused, typeName) \
        hdql::DynamicTraits<typeName>::tCode = hdql_types_get_type_code(types, #typeName ); \
        assert(hdql::DynamicTraits<typeName>::tCode);
    _M_for_each_atomic_type(_M_get_type_code);
    #undef _M_get_type_code

    hdql::DynamicTraits<hdql_Bool_t>::tCode = hdql_types_get_type_code(types, "hdql_Bool_t");
    assert(hdql::DynamicTraits<hdql_Bool_t>::tCode != 0x0);

    std::pair<hdql_Operations::iterator, bool> ir;

    //
    // Add standard arithmetics
    #define _M_impose_std_arith_op_of_types(op, sign, t1, t2)                   \
        ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                             , hdql::DynamicTraits<t2>::tCode   \
                                             , hdql_kOp ## op}                  \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql::ArithmeticOpTraits<t1, t2>::Result_t>::tCode \
                                    , hdql:: _ ## t1 ## _ ## op ## _ ## t2 });  \
        assert(ir.second);
    
    #define _M_impose_std_arith_ops_of_types(t1, t2) \
        _M_for_every_binary_arith_op(_M_impose_std_arith_op_of_types, t1, t2)

    _M_EXPAND(_M_for_each_numerical_type(_M_for_each_atomic_type__, _M_impose_std_arith_ops_of_types));
    #undef _M_impose_std_arith_ops_of_types
    #undef _M_impose_std_arith_op_of_types

    //
    // Add binary logic operations
    #define _M_impose_std_logic_of_types(op, sign, t1, t2)                      \
        ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                             , hdql::DynamicTraits<t2>::tCode   \
                                             , hdql_kOpL ## op}                  \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql_Bool_t>::tCode \
                                    , hdql:: _ ## t1 ## _L ## op ## _ ## t2 });  \
        assert(ir.second);
    
    #define _M_impose_std_logic_ops_of_types(t1, t2) \
        _M_for_every_binary_logic_op(_M_impose_std_logic_of_types, t1, t2)

    _M_EXPAND(_M_for_each_atomic_type(_M_for_each_atomic_type__, _M_impose_std_logic_ops_of_types));
    #undef _M_impose_std_logic_ops_of_types
    #undef _M_impose_std_logic_of_types

    //
    // Add standard comparison
    #define _M_impose_std_cmp_op_of_types(op, sign, t1, t2)                     \
        ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                             , hdql::DynamicTraits<t2>::tCode   \
                                             , hdql_kOp ## op}                  \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql_Bool_t>::tCode \
                                    , hdql:: _ ## t1 ## _ ## op ## _ ## t2 });  \
        assert(ir.second);
    
    #define _M_impose_std_cmp_ops_of_types(t1, t2) \
        _M_for_every_binary_comparison_op(_M_impose_std_cmp_op_of_types, t1, t2)

    _M_EXPAND(_M_for_each_numerical_type(_M_for_each_atomic_type__, _M_impose_std_cmp_ops_of_types));
    #undef _M_impose_std_cmp_ops_of_types
    #undef _M_impose_std_cmp_op_of_types

    //
    // Add integer arithmetics
    #define _M_impose_int_arith_op_of_types(op, sign, t1, t2)                   \
        ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                             , hdql::DynamicTraits<t2>::tCode   \
                                             , hdql_kOp ## op}                  \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql_Int_t>::tCode \
                                    , hdql:: _ ## t1 ## _ ## op ## _ ## t2 });  \
        assert(ir.second);
    
    #define _M_impose_int_arith_ops_of_types(t1, t2) \
        _M_for_every_integer_arithmetics_op(_M_impose_int_arith_op_of_types, t1, t2)

    _M_EXPAND(_M_for_each_integer_type(_M_for_each_integer_type__, _M_impose_int_arith_ops_of_types));
    #undef _M_impose_int_arith_ops_of_types
    #undef _M_impose_int_arith_op_of_types

    //
    // unary operations
    #define _M_impose_unary_minus(unused, t1)                               \
    ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                         , 0x0                              \
                                         , hdql_kUOpMinus }                 \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql::UnaryMinusSpec<t1>::Result_t>::tCode \
                                    , hdql::UnaryMinusSpec<t1>::op });  \
        assert(ir.second);
    _M_for_each_numerical_type(_M_impose_unary_minus);
    #undef _M_impose_unary_minus

    #define _M_impose_unary_bnot(unused, t1)                               \
    ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode  \
                                         , 0x0                             \
                                         , hdql_kUOpBNot }                 \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<t1>::tCode \
                                    , hdql::_T_unary_b_not<t1> });  \
        assert(ir.second);
    _M_for_each_integer_type(_M_impose_unary_bnot);
    #undef _M_impose_unary_bnot

    #define _M_impose_logic_unary_not(unused, t1)                           \
    ir = operations->emplace( hdql::OpKey{ hdql::DynamicTraits<t1>::tCode   \
                                         , 0x0                              \
                                         , hdql_kUOpNot }                   \
                                , hdql_OperationEvaluator{ hdql::DynamicTraits<hdql_Bool_t>::tCode \
                                    , hdql::_T_unary_l_not<t1> });  \
        assert(ir.second);
    _M_for_each_atomic_type(_M_impose_logic_unary_not);
    #undef _M_impose_logic_unary_not

    return ir.second ? 0 : -1;
}

//                                                          __________________
// _______________________________________________________/ Scalar arithmetics

// internal struct, represents arithmetic operation
struct ScalarOperation {
    struct hdql_Query * argQueries[2];
    struct hdql_CollectionKey * keys[2];
    hdql_OperationEvaluator evaluator;
    hdql_Datum_t result;
};

extern "C" hdql_Datum_t
hdql_scalar_arith_op_create( hdql_Query * a
                           , hdql_Query * b
                           , const struct hdql_OperationEvaluator * evaluator
                           , hdql_Context_t ctx
                           ) {
    struct ScalarOperation * scalarOp = hdql_alloc(ctx, struct ScalarOperation);
    scalarOp->argQueries[0] = a;
    scalarOp->argQueries[1] = b;
    scalarOp->evaluator = *evaluator;
    scalarOp->result = hdql_create_value(evaluator->returnType, ctx);

    for(int i = 0; i < 2; ++i) {
        if(NULL == scalarOp->argQueries[i]) {
            scalarOp->keys[i] = NULL;
            continue;
        }
        hdql_CollectionKey * k = NULL;
        int rc = hdql_query_keys_destroy(k, ctx);
        assert(0 == rc);  // TODO: handle key allocation error
        scalarOp->keys[i] = k;
    }

    return reinterpret_cast<hdql_Datum_t>(scalarOp);
}

extern "C" hdql_Datum_t
hdql_scalar_arith_op_dereference( hdql_Datum_t root
                                , hdql_Context_t ctx
                                , hdql_Datum_t scalarOperation
                                ) {
    struct ScalarOperation * op = hdql_cast(ctx, struct ScalarOperation, scalarOperation);
    assert(op->argQueries[0]);
    hdql_query_reset(op->argQueries[0], root, ctx);
    if(op->argQueries[1])
        hdql_query_reset(op->argQueries[1], root, ctx);
    hdql_Datum_t a = hdql_query_get(op->argQueries[0], op->keys[0], ctx);
    hdql_Datum_t b = op->argQueries[1]
                   ? hdql_query_get(op->argQueries[1], op->keys[1], ctx)
                   : NULL
               //, c = op->argQueries[2]  // TODO
               //    ? hdql_query_get(op->argQueries[2], root, op->keys[2], ctx)
               //    : NULL
               ;
    assert(a);
    int rc = op->evaluator.op(a, b, op->result);
    if(0 != rc) {
        hdql_context_err_push(ctx, rc, "arithmetic operation error %d", rc);  // TODO: elaborate
    }
    return op->result;
}

extern "C" void
hdql_scalar_arith_op_free( hdql_Datum_t scalarOp_, hdql_Context_t ctx ) {
    struct ScalarOperation * scalarOp
        = hdql_cast(ctx, struct ScalarOperation, scalarOp_);
    for(int i = 0; i < 2; ++i) {
        if(scalarOp->argQueries[i]) {
            assert(scalarOp->keys[i]);
            hdql_query_keys_destroy(scalarOp->keys[i], ctx);
            hdql_query_destroy(scalarOp->argQueries[i], ctx);
        }
    }
    if(scalarOp->result) {
        hdql_destroy_value(scalarOp->evaluator.returnType, scalarOp->result, ctx);
    }
    hdql_context_free(ctx, scalarOp_);
}

//                                 ____________________________________________
// ______________________________/ Context-private arithmetic operations mgmnt

// NOT exposed to public header
struct hdql_Operations *
_hdql_operations_create(struct hdql_Operations * parent, hdql_Context_t ctx) {
    return new hdql_Operations(parent);
}

// NOT exposed to public header
void
_hdql_operations_destroy(struct hdql_Operations * instance, hdql_Context_t ctx) {
    delete instance;
}

}  // extern "C"

