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
 * function call, or a null pointer when this is not possible. In the latter
 * case, may set a message in the given buffer explaining failure
 * reason (legitimate way to deny function instantiation for overridden
 * functions). */
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

/**\brief Common constructor for single-argument math functions
 *
 * Constructor for simple 1-argument functions on real number like
 * exp(), sin(), abs(), etc. Argument query must be "scalar atomic" (results in
 * scalar) or "collection of atomics" type (results in collection with
 * forwarded keys). Created `hdql_FuncDef` must have function of the signature
 * `double (*)(double)`. Examples:
 *  - trigonometric functions: sin, asin, sinh, cos, acos, cosh, tan, tanh,
 *    atan, atan2.
 *  - exponentiation, logarithms: sqrt, pow, floor, ceil, fabs, fmod, log, exp,
 *    log2, log10.
 * */
int
hdql_functions_add_standard_math(struct hdql_Functions * functions);

/**\briefe Adds foldable monoid functions
 *
 * Simple foldable monoid
 * ----------------------
 *
 * Aggreage functions featuring following traits:
 * - receives one or more queries resulting in atomic arithmetic type;
 * - results in a scalar real-valued convolution of all the query results;
 * - Either have fixed result type, or some kind of type promotion is applied
 *   for result type. I.e. sum of integers results in the larger integer,
 *   presense of float makes the sum result to be of floating point, etc.
 * - are not annotated with keys.
 *
 * Last feature makes this class of functions different from classic monoid
 * definitions, which can result in element selection -- min(), max(),
 * arbitrary() etc. Currently, SMA are:
 *
 *   Func. name  | Operation    | Neutral el. | Types           | Result type
 * --------------+--------------+-------------+-----------------+-------------
 *     sum       | a += b       | 0           | all numeric     | promoted
 *     product   | a *= b       | 1           | all numeric     | promoted
 *     min       | a= a<b ? a:b | max of type | all numeric     | promoted
 *     max       | a= a>b ? a:b | min of type | all numeric     | promoted
 *     bAND      | a & b        | ~0x0        | integer only    | promoted int
 *     bOR       | a | b        | 0x0         | integer only    | promoted int
 *     bXOR      | a ^ b        | 0x0         | integer only    | promoted int
 *     all       | a && b       | true        | all             | bool
 *     any       | a || b       | false       | all             | bool
 *     count     | a += b? 0:1  | 0           | all             | uin64_t
 *
 * The usefulness of XOR-based boolean monoid ("all odd are true") is doubtful,
 * yet one may imagine some practical applications still.
 */
int
hdql_functions_add_monoids(struct hdql_Functions * functions);

/* Statistics
 * ----------
 *
 * During the lifecycle, these monoids maintains more complex objects than
 * simple atomic value.
 * Although, they can be expressed by simpler monoids (and often algebraically
 * used to), the underlying implementation allows to gain more precise results.
 *
 *      Func. name  | Types           | Result type
 * -----------------+-----------------+-----------------
 *     arb          | all*            | inherited / promoted
 *     mean         | all numeric     | promoted
 *     var          | all numeric     | promoted
 *
 * *) arb() is applicable if either all queries result in the same type, or
 *    all are numeric. In first case, the resulting type matches the arguments,
 *    at the second the type is promoted.
 */
//int
//hdql_functions_add_basic_statistical(struct hdql_Functions * functions);

#if 0

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
