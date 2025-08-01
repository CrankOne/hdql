#ifndef H_NA64DP_MATH_DSL_TYPES_H
#define H_NA64DP_MATH_DSL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*                                                             _______________
 * __________________________________________________________/ Main API types
 */

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

#ifndef HDQL_STRING_TYPE
#   define HDQL_STRING_TYPE char*
#endif
typedef HDQL_STRING_TYPE hdql_StrPtr_t;

/*                                                           _________________
 * ________________________________________________________/ Sizes (settings)
 */

#ifndef HDQL_SELEXPR_ERROR_BUFFER_SIZE
/** static size of buffer for third-party error messages */
#   define HDQL_SELEXPR_ERROR_BUFFER_SIZE 256
#endif

#ifndef HDQL_VALUE_TYPEDEF_CODE_BITSIZE
/**\def HDQL_VALUE_TYPEDEF_CODE_BITSIZE
 * \brief Defines number of bits used in bitfield to identify value type
 *
 * Affects maximum number of permitted type definitions and structure packing.
 * Important notes:
 *  - to simplify indexing of the converter functions it is better to keep
 *    the size of containing type at least twice lesser than largest
 *    unsigned int number.
 *  - to keep tiers in the containing type for type table, it is better to
 *    foresee some bits at the beginning of containing type
 *  - reserve at least one bit in the containing type to keep scalar features
 *    flag (`hdql_AtomicTypeFeatures`) packed at the same area or at least two
 *    bits for `hdql_CollectionKey` pack
 * So for instance:
 *  - containing type is unsigned short of length 16
 *  - then conversion key is unsigned int of 2x16=32 bits
 *  - within containing type:
 *      - 2 bit is reserved for key's flags
 *      - 4 bits is reserved for tier (letting up to 16 inherited type tables)
 *      - 10 bits offers 1024 types per table of single tier
 * */
#   define HDQL_VALUE_TYPEDEF_CODE_BITSIZE 14
#endif

/**\brief Max value of type code, derived from max bitlen */
#define HDQL_VALUE_TYPE_CODE_MAX ((0x1 << HDQL_VALUE_TYPEDEF_CODE_BITSIZE)-1)

/**\brief Numeric type used to identify value type 
 *
 * Note that upper limit for type code is defined not by this type size, but
 * rather by macro `HDQL_VALUE_TYPEDEF_CODE_BITSIZE` */
typedef uint16_t hdql_ValueTypeCode_t;


#ifndef HDQL_COMPOUNDS_STACK_MAX_DEPTH
/**\brief Controls max depth of subquery stack
 *
 * In practice, the depth will doubtly exceed 2-3. */
#   define HDQL_COMPOUNDS_STACK_MAX_DEPTH 8
#endif

/*                                              ______________________________
 * ___________________________________________/ Common declaration-only types
 * Note: these types are never defined as real structs; their purpose is to
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
