#include "hdql/operations.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/compound.h"
#include "hdql/query.h"

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

#if 1

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

#define _M_for_each_type(m, ...) \
    _M_for_each_numerical_type(m, __VA_ARGS__) \
    m( __VA_ARGS__, hdql_Bool_t )

#define _M_EXPAND(M) M

#define _M_for_each_integer_type_() _M_for_each_integer_type
#define _M_for_each_integer_type__(...) _M_for_each_integer_type_ _M_EXPAND(()) (__VA_ARGS__)

#define _M_for_each_numerical_type_() _M_for_each_numerical_type
#define _M_for_each_numerical_type__(...) _M_for_each_numerical_type_ _M_EXPAND(()) (__VA_ARGS__)

#define _M_for_each_type_() _M_for_each_type
#define _M_for_each_type__(...) _M_for_each_type_ _M_EXPAND(()) (__VA_ARGS__)

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

_M_EXPAND(_M_for_each_numerical_type(_M_for_each_type__, _M_implement_arith_ops))
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

_M_EXPAND(_M_for_each_type(_M_for_each_type__, _M_implement_logic_ops))
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
_M_EXPAND(_M_for_each_numerical_type(_M_for_each_type__, _M_implement_comparison_ops))
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

#else
template<typename T1, typename T2> struct plusOp;
template<typename T1, typename T2> struct minusOp;
template<typename T1, typename T2> struct prodOp;
template<typename T1, typename T2> struct divideOp;

#define _M_for_every_arith_type(m, ...) \
    m( int8_t,  int8_t,     __VA_ARGS__  ) \
    m( int8_t,  uint8_t,    __VA_ARGS__  ) \
    m( uint8_t, int8_t,     __VA_ARGS__  ) \
    m( uint8_t, uint8_t,    __VA_ARGS__  ) \
    m( int8_t,  float,      __VA_ARGS__  ) \
    m( int8_t,  double,     __VA_ARGS__  ) \
    m( uint8_t, float,      __VA_ARGS__  ) \
    m( uint8_t, double,     __VA_ARGS__  ) \
    m( float,   int8_t,     __VA_ARGS__  ) \
    m( double,  int8_t,     __VA_ARGS__  ) \
    m( float,   uint8_t,    __VA_ARGS__  ) \
    m( double,  uint8_t,    __VA_ARGS__  ) \
    \
    m( float,   float,      __VA_ARGS__  ) \
    m( float,   double,     __VA_ARGS__  ) \
    m( double,  float,      __VA_ARGS__  ) \
    m( double,  double,     __VA_ARGS__  ) \

#define _M_implement_operation(OpName, opSign, T1, T2)      \
template<>                                                  \
struct OpName ## Op<T1, T2> {                               \
    typedef ArithmeticOpTraits<T1, T2>::Result_t Result_t;  \
    static int result(const hdql_Datum_t a_, const hdql_Datum_t b_, hdql_Datum_t r_) { \
        *reinterpret_cast<Result_t*>(r_)                    \
            = *reinterpret_cast<const T1 *>(a_)             \
            opSign *reinterpret_cast<const T2 *>(b_);       \
        return 0;                                           \
    }                                                       \
};

#define _M_(T1, T2, m) _M_for_every_binary_arith_op(m, T1, T2)

_M_for_every_arith_type(_M_, _M_implement_operation);
#endif

#if 0

#define _M_for_every_basic_arith_op(m) \
    m( Int, Int, Int ) \
    m( Flt, Int, Flt ) \
    m( Int, Flt, Flt ) \
    m( Flt, Flt, Flt )

#define _M_impl_plus_op( typeA, typeB, typeR ) \
    static int \
    _plus_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           + *reinterpret_cast<const hdql_ ## typeB ## _t *>(b); \
        return 0; \
    }
#define _M_impl_minus_op( typeA, typeB, typeR ) \
    static int \
    _minus_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           - *reinterpret_cast<const hdql_ ## typeB ## _t *>(b); \
        return 0; \
    }
#define _M_impl_product_op( typeA, typeB, typeR ) \
    static int \
    _product_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           * *reinterpret_cast<const hdql_ ## typeB ## _t *>(b); \
        return 0; \
    }
#define _M_impl_divide_op( typeA, typeB, typeR ) \
    static int \
    _divide_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           / *reinterpret_cast<const hdql_ ## typeB ## _t *>(b); \
        return 0; \
    }

_M_for_every_basic_arith_op( _M_impl_plus_op    );
_M_for_every_basic_arith_op( _M_impl_minus_op   );
_M_for_every_basic_arith_op( _M_impl_product_op );
_M_for_every_basic_arith_op( _M_impl_divide_op  );

#undef _M_impl_plus_op
#undef _M_impl_minus_op
#undef _M_impl_product_op
#undef _M_impl_divide_op

#define _M_for_every_arith_comparison_op(m) \
    m( Int, Int, Int ) \
    m( Flt, Int, Int ) \
    m( Int, Flt, Int ) \
    m( Flt, Flt, Int )

#define _M_impl_gt_op( typeA, typeB, typeR ) \
    static int \
    _gt_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           > *reinterpret_cast<const hdql_ ## typeB ## _t *>(b) ? HDQL_TRUE : HDQL_FALSE; \
        return 0; \
    }
#define _M_impl_gte_op( typeA, typeB, typeR ) \
    static int \
    _gte_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           =  *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           >= *reinterpret_cast<const hdql_ ## typeB ## _t *>(b) ? HDQL_TRUE : HDQL_FALSE; \
        return 0; \
    }
#define _M_impl_lt_op( typeA, typeB, typeR ) \
    static int \
    _lt_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           = *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           < *reinterpret_cast<const hdql_ ## typeB ## _t *>(b) ? HDQL_TRUE : HDQL_FALSE; \
        return 0; \
    }
#define _M_impl_lte_op( typeA, typeB, typeR ) \
    static int \
    _lte_ ## typeA ## _ ## typeB( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) \
    {   *reinterpret_cast<hdql_ ## typeR ## _t *>(r) \
           =  *reinterpret_cast<const hdql_ ## typeA ## _t *>(a)  \
           <= *reinterpret_cast<const hdql_ ## typeB ## _t *>(b) ? HDQL_TRUE : HDQL_FALSE; \
        return 0; \
    }

_M_for_every_arith_comparison_op( _M_impl_gt_op  );
_M_for_every_arith_comparison_op( _M_impl_gte_op );
_M_for_every_arith_comparison_op( _M_impl_lt_op  );
_M_for_every_arith_comparison_op( _M_impl_lte_op );

#undef _M_impl_plus_op
#undef _M_impl_minus_op
#undef _M_impl_product_op
#undef _M_impl_divide_op

//
// Integer only binary operators

static int
_modulo( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) { 
    hdql_Int_t bVal = *reinterpret_cast<const hdql_Int_t *>(b);
    if(0 == bVal)
        return HDQL_ARITH_ERR_ZERO_DIVISION;
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       % bVal;
    return 0;
}

static int
_rshift( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) { 
    hdql_Int_t bVal = *reinterpret_cast<const hdql_Int_t *>(b);
    if(bVal < 0)
        return HDQL_ARITH_ERR_NEGATIVE_SHIFT;
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       >> bVal;
    return 0;
}

static int
_lshift( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) { 
    hdql_Int_t bVal = *reinterpret_cast<const hdql_Int_t *>(b);
    if(bVal < 0)
        return HDQL_ARITH_ERR_NEGATIVE_SHIFT;
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       << bVal;
    return 0;
}

static int
_b_and( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) {
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       & *reinterpret_cast<const hdql_Int_t *>(b);
    return 0;
}

static int
_b_or( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) {
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       | *reinterpret_cast<const hdql_Int_t *>(b);
    return 0;
}

static int
_b_xor( const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t r ) {
    *reinterpret_cast<hdql_Int_t *>(r)
       = *reinterpret_cast<const hdql_Int_t *>(a)
       ^ *reinterpret_cast<const hdql_Int_t *>(b);
    return 0;
}

int
hdql_op_define_std_arith( struct hdql_Operations * operations
                        , struct hdql_ValueTypes * types
                        ) {
    hdql_ValueTypeCode_t IntCode = hdql_types_get_type_code(types, "hdql_Int_t")
                       , FltCode = hdql_types_get_type_code(types, "hdql_Flt_t")
                       ;
    assert(IntCode);
    assert(FltCode);
    std::pair<hdql_Operations::iterator, bool> ir;

    #define _M_impose_std_arith_of_types(typeA, typeB, typeR) \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpSum} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_plus_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpMinus} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_minus_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpProduct} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_product_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpDivide} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_divide_ ## typeA ## _ ## typeB }); \
        assert(ir.second);
    
    _M_for_every_basic_arith_op(_M_impose_std_arith_of_types);
    #undef _M_impose_std_arith_of_types

    #define _M_impose_std_comparison_of_types(typeA, typeB, typeR) \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpGT} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_gt_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpGTE} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_gte_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpLT} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_lt_ ## typeA ## _ ## typeB }); \
        assert(ir.second); \
        ir = operations->emplace( hdql::OpKey{ typeA ## Code, typeB ## Code, hdql_kOpLTE} \
                                , hdql_OperationEvaluator{ typeR ## Code, hdql::_lte_ ## typeA ## _ ## typeB }); \
        assert(ir.second);
    
    _M_for_every_arith_comparison_op(_M_impose_std_comparison_of_types);
    #undef _M_impose_std_comparison_of_types

    #define _M_impose_int_only_op( opName, fName )                          \
        ir = operations                                                     \
            ->emplace( hdql::OpKey{ IntCode, IntCode, hdql_kOp ## opName}   \
                     , hdql_OperationEvaluator{ IntCode, hdql::_ ## fName });   \

    _M_impose_int_only_op( Modulo,  modulo );
    _M_impose_int_only_op( RBShift, rshift );
    _M_impose_int_only_op( LBShift, lshift );
    _M_impose_int_only_op( BAnd,    b_and  );
    _M_impose_int_only_op( BOr,     b_or   );
    _M_impose_int_only_op( BXOr,    b_xor  );

    #undef _M_impose_int_only_op

    return ir.second ? 0 : -1;
}
#endif

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
    if(ops->end() == it) return NULL;
    return &(it->second);
}

#if 0
int
hdql_op_eval( hdql_Datum_t valueA
            , hdql_OperationEvaluator_t opEvaluator
            , hdql_Datum_t valueB
            //, const struct hdql_ValueTypes * types
            //, char * errBuffer, size_t errBufferSize
            , hdql_Datum_t result
            ) {
    assert(valueA);
    assert(opEvaluator);
    assert(result);

    return opEvaluator->op(valueA, valueB, result); 

    #if 0
    if(0 == rc) return rc;

    /* otherwise an error has occured */
    const struct hdql_ValueInterface
                      * viA = hdql_types_get_type(types, t1)
                    , * viB = valueB ? hdql_types_get_type(types, t2) : NULL
                    ;

    char strBfA[64];
    int rc2 = -1;
    if(viA && viA->get_as_string) {
        rc2 = viA->get_as_string(valueA, strBfA, sizeof(strBfA));
    }
    if(0 != rc2)
        snprintf(strBfA, sizeof(strBfA), "(data at %p)", valueA);
    if(valueB) {
        char strBfB[64];
        rc2 = -1;
        if(viB) {
            rc2 = viB->get_as_string(valueB, strBfB, sizeof(strBfB));
        }
        if(0 != rc2) {
            snprintf(strBfB, sizeof(strBfB), "(data at %p)", valueB);
        }
        if(0 != rc2) strncpy(strBfB, "(no str value)", sizeof(strBfB));
        snprintf( errBuffer, errBufferSize
                  , "operator evaluation returned code %d with"
                    " operands %s of type %s and %s of type %s"
                  , rc
                  , strBfA, viA->name
                  , strBfB, viB->name
                  );
    } else {
        snprintf( errBuffer, errBufferSize
                  , "operator evaluation returned code %d with"
                    " value %s (of type %s)"
                  , rc, strBfA, viA->name
                  );
    }
    return rc;
    #endif
}
#endif

int
hdql_op_define_std_arith( struct hdql_Operations * operations
                        , struct hdql_ValueTypes * types
                        ) {

    #define _M_get_type_code(unused, typeName) \
        hdql::DynamicTraits<typeName>::tCode = hdql_types_get_type_code(types, #typeName ); \
        assert(hdql::DynamicTraits<typeName>::tCode);
    _M_for_each_type(_M_get_type_code);
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

    _M_EXPAND(_M_for_each_numerical_type(_M_for_each_type__, _M_impose_std_arith_ops_of_types));
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

    _M_EXPAND(_M_for_each_type(_M_for_each_type__, _M_impose_std_logic_ops_of_types));
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

    _M_EXPAND(_M_for_each_numerical_type(_M_for_each_type__, _M_impose_std_cmp_ops_of_types));
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
    _M_for_each_type(_M_impose_logic_unary_not);
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
        int rc = hdql_query_reserve_keys_for(scalarOp->argQueries[i]
                , &k, ctx);
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
    hdql_Datum_t a = hdql_query_get(op->argQueries[0], root, op->keys[0], ctx);
    hdql_Datum_t b = op->argQueries[1]
                   ? hdql_query_get(op->argQueries[1], root, op->keys[1], ctx)
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
            hdql_query_destroy_keys_for(scalarOp->argQueries[i]
                    , scalarOp->keys[i], ctx);
            hdql_query_destroy(scalarOp->argQueries[i], ctx);
        }
    }
    if(scalarOp->result) {
        hdql_destroy_value(scalarOp->evaluator.returnType, scalarOp->result, ctx);
    }
    hdql_context_free(ctx, scalarOp_);
}

//                                                     ________________________
// __________________________________________________/ Collection arithmetics

struct ArithCollection {
    struct hdql_ArithCollectionArgs * args;
    bool isResultValid;
    hdql_Datum_t valueA, valueB;
    // hdql_CollectionKey * keys;  // TODO
    hdql_Datum_t result;
};

hdql_It_t
_arith_collection_op_create( hdql_Datum_t root
                           , hdql_SelectionArgs_t selArgs_
                           , hdql_Context_t ctx
                           ) {
    struct hdql_ArithCollectionArgs * aca
        = hdql_cast(ctx, struct hdql_ArithCollectionArgs, selArgs_);
    struct ArithCollection * r = hdql_alloc(ctx, struct ArithCollection);
    r->args = aca;
    r->result = hdql_create_value(r->args->evaluator->returnType, ctx);
    r->valueA = r->valueB = NULL;
    r->isResultValid = false;
    return reinterpret_cast<hdql_It_t>(r);
}

hdql_Datum_t
_arith_collection_op_dereference( hdql_Datum_t root
                                , hdql_It_t it_
                                , hdql_Context_t ctx
                                ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    if(it->isResultValid) return it->result;
    int rc = it->args->evaluator->op(it->valueA, it->valueB, it->result);
    if(0 != rc) {
        hdql_context_err_push(ctx, rc, "Arithmetic operation on"
                " collection returned %d", rc);  // TODO: elaborate
    }
    it->isResultValid = true;
    return it->result;
}

hdql_It_t
_arith_collection_op_advance( hdql_Datum_t root
                            , hdql_SelectionArgs_t selArgs
                            , hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    assert(reinterpret_cast<hdql_ArithCollectionArgs *>(selArgs) == it->args );
    assert(0);  // TODO
    // ...
    it->isResultValid = false;
    return it_;
}

void
_arith_collection_op_reset( hdql_Datum_t root
                          , hdql_SelectionArgs_t selArgs
                          , hdql_It_t it_
                          , hdql_Context_t ctx
                          ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    assert(reinterpret_cast<hdql_ArithCollectionArgs *>(selArgs) == it->args );
    hdql_query_reset(it->args->a, root, ctx);
    if(it->args->b)
        hdql_query_reset(it->args->a, root, ctx);
}

void
_arith_collection_op_destroy( hdql_It_t
                            , hdql_Context_t ctx
                            ) {
}

void
_arith_collection_op_get_key( hdql_Datum_t root
                            , hdql_It_t
                            , struct hdql_CollectionKey *
                            , hdql_Context_t ctx
                            ) {
}

void
_arith_collection_op_free_selection(hdql_Context_t ctx, hdql_SelectionArgs_t) {

}

const struct hdql_CollectionAttrInterface hdql_gArithOpIFace = {
    .keyTypeCode = 0x0,
    .create = _arith_collection_op_create,
    .dereference = _arith_collection_op_dereference,
    .advance = _arith_collection_op_advance,
    .reset = _arith_collection_op_reset,
    .destroy = _arith_collection_op_destroy,
    .get_key = _arith_collection_op_get_key,
    .compile_selection = NULL,
    .free_selection = _arith_collection_op_free_selection
};

//                                 ____________________________________________
// ______________________________/ Context-private arithmetic operations mgmnt

// NOT exposed to public header
struct hdql_Operations *
_hdql_operations_create(hdql_Context_t ctx) {
    // TODO: use context-based allocations
    return new hdql_Operations;
}

// NOT exposed to public header
void
_hdql_operations_destroy(struct hdql_Operations * instance, hdql_Context_t ctx) {
    // TODO: use context-based allocations
    delete instance;
}

}  // extern "C"

