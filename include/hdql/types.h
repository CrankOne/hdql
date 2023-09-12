#ifndef H_NA64DP_MATH_DSL_TYPES_H
#define H_NA64DP_MATH_DSL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int hdql_Err_t;

#ifndef HDQL_ARITH_BOOL_TYPE
#   define HDQL_ARITH_BOOL_TYPE bool
#endif
/** default type for arithmetic operations with integer numbers */
typedef HDQL_ARITH_BOOL_TYPE hdql_Bool_t;

#ifndef HDQL_ARITH_INT_TYPE
#   define HDQL_ARITH_INT_TYPE int64_t
#endif
/** default type for arithmetic operations with integer numbers */
typedef HDQL_ARITH_INT_TYPE hdql_Int_t;


#ifndef HDQL_ARITH_FLT_TYPE
#   define HDQL_ARITH_FLT_TYPE double
#endif
/** default type for arithmetic operations with floating point numbers */
typedef HDQL_ARITH_FLT_TYPE hdql_Flt_t;


#ifndef HDQL_ATTR_INDEX_TYPE
#   define HDQL_ATTR_INDEX_TYPE uint32_t
#endif
/** attribute index field type */
typedef HDQL_ATTR_INDEX_TYPE hdql_AttrIdx_t;


#ifndef HDQL_SELEXPR_ERROR_BUFFER_SIZE
/** static size of buffer for third-party error messages */
#   define HDQL_SELEXPR_ERROR_BUFFER_SIZE 256
#endif


/**\def HDQL_VALUE_TYPEDEF_CODE_BITSIZE
 * \brief Defines number of bits used in bitfield to identify value type
 *
 * Affects maximum number of permitted type definitions and structure packing.
 * */
#ifndef HDQL_VALUE_TYPEDEF_CODE_BITSIZE
#   define HDQL_VALUE_TYPEDEF_CODE_BITSIZE 10
#endif

#define HDQL_VALUE_TYPE_CODE_MAX ((0x1 << HDQL_VALUE_TYPEDEF_CODE_BITSIZE)-1)

#ifndef HDQL_SUBQUERY_DEPTH_BITSIZE
#   define HDQL_SUBQUERY_DEPTH_BITSIZE 6
#endif


#ifndef HDQL_COMPOUNDS_STACK_MAX_DEPTH
#   define HDQL_COMPOUNDS_STACK_MAX_DEPTH 64
#endif

/* Note: these types are never defined as real structs; their purpose is to
 * exploit basic C compiler type constrol system as much as possible.
 * Internally these pointers are reinterpret_cast'ed to particular types. */

/* Versatile state (single sub-query iterator) type */
struct hdql_It;
typedef struct hdql_It * hdql_It_t;

/* Versatile pointer to any data */
struct hdql_Datum;
typedef struct hdql_Datum * hdql_Datum_t;

/* By-index selection arguments */
struct hdql_SelectionArgs;
typedef struct hdql_SelectionArgs * hdql_SelectionArgs_t;

/* Context type */
struct hdql_Context;
typedef struct hdql_Context* hdql_Context_t;

const char *
hdql_cxx_demangle( const char * mangled
                 , char * buf, size_t buflen
                 );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /*H_NA64DP_MATH_DSL_TYPES_H*/
