#ifndef H_HDQL_FUNCTION_H
#define H_HDQL_FUNCTION_H

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/operations.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\file
 * \brief HDQL function definition
 *
 * Programmatically, a "function" in HDQL query machine is just another kind of
 * "virtual" attribute associated with consodered object. An object is
 * considered as the source of argument's queries.
 *
 * A "function definition" must define a rule of production for this virtual
 * attribute definition. By refering a particular name HDQL script picks up
 * certain "function definition" that is then used by a parser to build up a
 * particular attribute that is then placed to a "query" object ready for
 * evaluation.
 * */

/**\brief HDQL functions dictionary */
struct hdql_Functions;

/**\brief Constructor instantiating function object for given queries 
 *
 * Shall return either a pointer to attribute definition performing certain
 * function call, or a null pointer. In the latter case, may set a message
 * in the given buffer explaining why failure reason. */
typedef struct hdql_AttrDef * (*hdql_FunctionConstructor_t)(
          struct hdql_Query ** args, void * userdata
        , char * failureBuffer
        , size_t failureBufferSize
        , hdql_Context_t context);

/**\brief Creates new function definition in a dictionary */
int
hdql_functions_define( struct hdql_Functions *
                     , const char * name
                     , hdql_FunctionConstructor_t
                     , void * userdata
                     );

/**\brief Instantiates function object based on name and argument queries */
int
hdql_functions_resolve( struct hdql_Functions * funcDict
                      , const char * name
                      , struct hdql_Query ** args
                      , struct hdql_AttrDef ** r
                      , hdql_Context_t context
                      );

int
hdql_functions_add_standard_math(struct hdql_Functions * functions);

#if 0

/*
 * Some common implementations
 */

/**\brief Common constructor for single-argument math functions
 *
 * Constructor for simple 1-argument functions on real number like
 * exp(), sin(), abs(), etc. Argument query must be "scalar atomic" (results in
 * scalar) or "collection of atomics" type (results in collection with
 * forwarded keys). Created `hdql_FuncDef` must have function of the signature
 * `double (*)(double)`. Examples:
 *  - trigonometric functions
 *  - exponentiation, logarithms
 * */
struct hdql_AttrDef *
hdql_func_helper__try_instantiate_unary_math(struct hdql_Query **, void *, hdql_Context_t);

/**\brief Common constructor for logic functions
 *
 * Constructor for functions taking one argument of "collection atomic"
 * type and returning one (scalar) value typed as logic, without key. Examples:
 *  - `any()` returns `true` if at least one of the collection elements
 *      evaluated to `true`, similar to OR concatenation
 *  - `all()` returns `true` only if all the collection elements evaluated to
 *      `true`, similar to AND concatenation
 *  - `none()` returns `true` only if all the collection elements evaluated to
 *      `false`, similar to inversion of OR concatenation
 * */
struct hdql_AttrDef *
hdql_func_helper__try_instantiate_logic(struct hdql_Query **, void *, hdql_Context_t);

/**\brief Common constructor for aggregate functions
 *
 * Constructor for aggregate functions taking one argument of "collection atomic"
 * type and returning one (scalar) value typed the same, without key. Examples:
 *  - `sum()` returns numerical sum of collection elements
 *  - `average()` returns mean value of collection elements
 *  - `median()` returns median value of collection elements
 *  - `variance()` returns square of the standard deviation
 *  - `stddev()` returns standard deviation
 * */
struct hdql_AttrDef *
hdql_func_helper__try_instantiate_aggregate(struct hdql_Query **, void *, hdql_Context_t);

/**\brief Common constructor for aggregate functions returning result with key
 *
 * Constructor for aggregate functions taking one argument of "collection atomic"
 * type and returning one value typed the same with key. Examples:
 *  - `arb()` returns arbitrary element (note: iterates over collection first)
 *  - `minimum()` returns minimum element from a collection
 *  - `maximum()` returns maximum element from a collection
 * */
struct hdql_AttrDef *
hdql_func_helper__try_instantiate_aggregate_w_key(struct hdql_Query **, void *, hdql_Context_t);

/**\brief Common constructor for aggregate functions returning collection
 *
 * Constructor for aggregate functions taking one argument of "collection atomic"
 * type and returning "collection atomic". Examples:
 *  - `unique()`
 * */
struct hdql_AttrDef *
hdql_func_helper__try_instantiate_aggregate_w_key(struct hdql_Query **, void *, hdql_Context_t);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_FUNCTION_H */
