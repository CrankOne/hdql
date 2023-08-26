#ifndef H_HDQL_OPERATIONS_H
#define H_HDQL_OPERATIONS_H

#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HDQL_FUNC_MAX_NARGS_2POW
#define     HDQL_FUNC_MAX_NARGS_2POW 5
#endif

struct hdql_CollectionAttrInterface;  /* fwd */
struct hdql_Query;  /* fwd */

/**\brief Index type for table of defined arithmetic operations */
struct hdql_Operations;

typedef int (*hdql_ArithOpCallback_t)(const hdql_Datum_t a, const hdql_Datum_t b, hdql_Datum_t result);

/**\brief Callback type for operation */
typedef const struct hdql_OperationEvaluator {
    hdql_ValueTypeCode_t returnType;
    hdql_ArithOpCallback_t op;
} * hdql_OperationEvaluator_t;

/**\brief Standard arithmetic operator codes */
typedef enum hdql_OperationCode {
    hdql_kOpUndefined,
    hdql_kOpSum,
    hdql_kOpMinus,
    hdql_kOpProduct,
    hdql_kOpDivide,
    hdql_kOpModulo,
    hdql_kOpRBShift,
    hdql_kOpLBShift,
    hdql_kOpBAnd,
    hdql_kOpBXOr,
    hdql_kOpBOr,
    hdql_kOpLOr,
    hdql_kOpLAnd,
    hdql_kOpLXOr,
    hdql_kOpLT,
    hdql_kOpLTE,
    hdql_kOpGT,
    hdql_kOpGTE,
    hdql_kUOpNot,
    hdql_kUOpBNot,  // todo: rename to b-invert
    hdql_kUOpMinus
} hdql_OperationCode_t;

/**\brief Defines new operation in operations table for given type(s)
 *
 * This function defines operation for unary or binary arithmetic operator
 * provided as `opCode` arg between one or two types. If `opCode` is for
 * unary operator, `t2` is unused, otherwise `t1` is for left operand, `t2` is
 * for right. */
int
hdql_op_define( struct hdql_Operations *
        , hdql_ValueTypeCode_t t1
        , hdql_OperationCode_t opCode
        , hdql_ValueTypeCode_t t2
        , hdql_OperationEvaluator_t
        );

/**\brief Returns previously defined operation callback
 *
 * \return NULL if callback is not found */
hdql_OperationEvaluator_t
hdql_op_get( const struct hdql_Operations * ops
        , hdql_ValueTypeCode_t t1
        , hdql_OperationCode_t
        , hdql_ValueTypeCode_t t2
        );

#if 0
/**\brief Executes binary or unary operation on given values
 *
 * Helper function wrapping procedure of invoking operation callback. Forwards
 *
 * `result` must be pre-allocated.
 */
int
hdql_op_eval( hdql_Datum_t valueA
            , hdql_OperationEvaluator_t opEvaluator
            , hdql_Datum_t valueB
            , hdql_Datum_t result
            );
#endif

/**\brief Fills index of operations with standard numerical arithmetics */
int hdql_op_define_std_arith(struct hdql_Operations *, struct hdql_ValueTypes *);

/*                                                          ___________________
 * _______________________________________________________/ Scalar arithmetics
 */

/**\brief ... */
hdql_Datum_t hdql_scalar_arith_op_create( struct hdql_Query * a
                           , struct hdql_Query * b
                           , const struct hdql_OperationEvaluator * evaluator
                           , hdql_Context_t ctx
                           );

/**\brief Implements scalar attribute dereference method for scalar arithmetic
 *
 * Used for query attribute definition. */
hdql_Datum_t hdql_scalar_arith_op_dereference( hdql_Datum_t root
        , hdql_Context_t ctx
        , hdql_Datum_t scalarOperation );

/**\brief Implements scalar attribute instance delete method for scalar
 *        arithmetic
 *
 * Used for query attribute definition. */
void hdql_scalar_arith_op_free( hdql_Datum_t scalarOp_, hdql_Context_t ctx );

/*                                                      _______________________
 * ___________________________________________________/ Collection arithmetics
 */

/**\brief Temporary instance used as selection arguments for arithmetics on
 *        collections */
struct hdql_ArithCollectionArgs {
    struct hdql_Query * a, * b;
    const struct hdql_OperationEvaluator * evaluator;
};

/**\brief Collection interface implementation for arithmetic functions */
extern const struct hdql_CollectionAttrInterface hdql_gArithOpIFace;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_OPERATIONS_H */
