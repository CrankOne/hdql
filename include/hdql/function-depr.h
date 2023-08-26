#ifndef H_HDQL_FUNCTION_H
#define H_HDQL_FUNCTION_H

#include "hdql/compound.h"
#include "hdql/operations.h"
#include "hdql/types.h"
#include "hdql/query.h"

/**\file
 * \brief Functions and function-like objects in HDQL
 *
 * \note HDQL functions are not necessarily pure functions. Hereftafter term
 *       "function" refers to certain mapping which is not necessarily
 *       idempotent (HDQL function may have a state object bound to it).
 *
 * Regarding the way how functions are implemented, functions are:
 *  - arithmetic operations (unary, binary or ternary), including comparison
 *    operations and bitwise arithmetics
 *  - scalar (or "cartesian product arguments list") functions, iterating
 *    vector arguments list as cartesian product
 *  - generic functions
 *
 * We also make distinction between stateful and stateless functions. While
 * arithmetic operations are always stateless, both scalar and generic
 * functions may have state bound to it.
 *
 * From the C/C++ API implementation point of view, function lifecycle refers
 * to following objects:
 *  - a function definition is provided externally and gets represented
 *    as module-private object. Currently, HDQL does not have any means to
 *    define function by its own. Function constructor is
 *    defined using `hdql_func_define_...()` routines and refers to particular
 *    `hdql_Functions` dictionary instance provided by context. Functions can
 *    be overriden meaning that same name may refer to different
 *    implementations.
 *  - a "function definition" is used to instantiate `hdql_Func` object by
 *    combining "function definition" and corresponding set of argument
 *    queries. A name-and-arguments based factory constructor is
 *    `hdql_func_new()`
 *  - arithmetic operations are constructed by `hdql_func_create_arith()`.
 *  - arithmetic operations receive their arguments with `hdql_func_set_arg()`
 *    after being constructed. The reason why it is not deduced automatically
 *    within function defined in this API is that parsers usually perform
 *    argument type deduction on their side to evaluate static expressions
 *    at translation stage.
 *  - usually, function establishes certain mapping between key argument's keys
 *    and result so that result also can be identified by key. Corresponding
 *    reserve/set/destroy procedures are defined here as well.
 * */

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Parameters pack for operation-as-collection interface */
struct hdql_Func;

/**\brief Dictionary of functions
 *
 * Dictionary of function node constructors identified by name.
 * */
struct hdql_Functions;

/**\brief Callback type for stateless (pure) scalar functions */
typedef int (*hdql_StatelessScalarFunction_t)(hdql_Datum_t * args, hdql_Datum_t result, hdql_Context_t ctx);
/**\brief Interface type for stateful scalar function-like objects */
typedef struct hdql_StatefulScalarFunction {
    void (*reset)(hdql_Datum_t);
    /* type code of returned value */
    hdql_ValueTypeCode_t returnType;
    /* gets called on cleanup */
    int (*destroy)(hdql_Datum_t);
    /* evaluates the function */
    int (*call)(hdql_Datum_t state, hdql_Datum_t * args, hdql_Datum_t result, hdql_Context_t ctx);
} * hdql_StatefulScalarFunction_t;

/**\brief Interface type for stateful scalar function-like objects */
typedef struct hdql_GenericFunction {
    /* type code of returned value */
    hdql_ValueTypeCode_t returnType;
    /* gets called on cleanup */
    int (*destroy)(hdql_Datum_t);
    /* evaluates the function */
    int (*call)(hdql_Datum_t state, struct hdql_Query ** args, hdql_Datum_t result, hdql_Context_t ctx);

    struct hdql_CollectionKey * (*reserve_keys)(hdql_Datum_t, struct hdql_Query **, hdql_Context_t);
    // int (*copy_keys)(...), etc
} * hdql_GenericFunction_t;

/**\brief Creates new function node instance identified by name */
struct hdql_Func *
hdql_func_create( const char * name
        , struct hdql_Query ** operands
        , hdql_Context_t context
        );

#if 0
/**\brief Callback type for function receiving arguments as cartesian product
 *        list
 *
 * Pure functions (stateless) handling cartesian product list. Most of the
 * special arithemtic functions should match this callback.
 */
typedef int (*hdql_GenericFunctionCPCallback_t)( const hdql_Datum_t instance
        , hdql_Datum_t * arguments
        , hdql_Datum_t result
        );

/**\brief Callback type for a generic function */
typedef int (*hdql_GenericFunctionCallback_t)( const hdql_Datum_t instance
        , struct hdql_Query ** argQueries
        , hdql_Datum_t localState
        , hdql_Datum_t result
        );
/**\brief Generic stateful function definition
 *
 * By "generic function" we assume procedures evaluating certain result on the
 * query as a whole. It may be stateful functions (functors) like summators,
 * user-defined forwarding procedures, aggregate methods and so on. Such
 * functions are defined by primary callback (which is of type defined below)
 * and auxiliary state-mangement state constructor, destructor and re-set.
 *
 * Generic functions are capable to operate with queries in somewhat opaque
 * mode. It's a bit more tedious than cartesian-product, but useful to
 * facilitate aggregate functions.
 *
 * \todo Not yet fully implemented, just anticipated by some calls
 * */
struct hdql_GenericFunction {
    /**\brief Function result definition */
    const struct hdql_AttributeDefinition * resultDefinition;
    /**\brief Shall allocate new generic function's local context instance */
    hdql_Datum_t (*init_local_context)();
    /**\brief Evaluates function on given queries, mutating local context */
    hdql_GenericFunctionCallback_t eval;
    /**\brief Re-sets local context instance */
    void (*reset)(hdql_Datum_t);
    /**\brief Destroys local context*/
    int (*destroy)(hdql_Datum_t);
};

typedef struct hdql_GenericFunction * hdql_GenericFunction_t;

/**\brief Creates new generic function definition for N arguments and given
 *        callback */
//struct hdql_Func * hdql_func_create_generic(hdql_Context_t ctx);
/**\brief Creates new generic function definition for N arguments and given
 *        callback */
//struct hdql_Func * hdql_func_create_generic(hdql_Context_t ctx);
#endif

/**\brief Creates new arithmetic operation instance
 *
 * \note contrary to `hdql_func_new()` creating scalar/generic functions, this
 *       ctr does not resolve functions overloading since provided evaluator is
 *       typed already
 * \todo parameterise with argument queries instead of `hdql_func_set_arg()`
 */
struct hdql_Func *
hdql_func_create_arith( hdql_Context_t ctx
                      , const struct hdql_OperationEvaluator *
                      , size_t nArgs
                      );
/**\brief Adds argument to a function instance
 *
 * Returns 0 on success, -1 if max number of arguments defined on creation
 * exceeded.
 *
 * \warning to be removed
 * */
int hdql_func_set_arg(struct hdql_Func *, size_t n, struct hdql_Query *);

/**\brief Reserves keys sub-list
 *
 * Given pointer will be set to an array of keys containing sub-arrays.
 * Terminated with an item of type code `HDQL_VALUE_TYPE_CODE_MAX`. */
int hdql_func_reserve_keys( struct hdql_Func *
                          , struct hdql_CollectionKey ** dest
                          , hdql_Context_t );

/**\brief Copies argument keys */
int hdql_func_copy_keys( struct hdql_Func * fDef
                       , struct hdql_CollectionKey * dest
                       , const struct hdql_CollectionKey * src
                       , hdql_Context_t ctx
                       );
/**\brief Invoces callback on every key respecting query topology */
int hdql_func_call_on_keys( struct hdql_Func * fDef
                          , struct hdql_CollectionKey * keys
                          , hdql_KeyIterationCallback_t callback, void * userdata
                          , size_t cLevel, size_t nKeyAtLevel
                          , hdql_Context_t context
                          );
/**\brief Destroys keys respecting query topology */
int hdql_func_destroy_keys( struct hdql_Func * fDef
                          , struct hdql_CollectionKey * keys
                          , hdql_Context_t
                          );

/**\brief Destroys function node instance */
void hdql_func_destroy(struct hdql_Func *, hdql_Context_t ctx);

/**\brief Collection interface implementation for arithmetic op function */
extern const struct hdql_CollectionAttrInterface * hdql_gArithOpIFace;
/**\brief Collection interface implementation for cartesian product arguments function */
//extern const struct hdql_CollectionAttrInterface * hdql_gCartProdArgsFuncIFace;
/**\brief Collection interface implementation for generic function */
//extern const struct hdql_CollectionAttrInterface * hdql_gGenericFuncIFace;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_FUNCTION_H */
