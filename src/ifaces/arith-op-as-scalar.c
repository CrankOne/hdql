#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/operations.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/internal-ifaces.h"

#include <assert.h>

/*
 * Unary or binary arithmetic operation node for scalar arguments resulting a
 * scalar.
 *
 * Definition data brings argument queries (are subject of changes, actually)
 * and arithmetic operation evaluator. Note, that both arguments are
 * considered to be "full scalar" objects meaning that they will yield a key.
 */

struct ArithOpScalarState {
    hdql_Datum_t result;
};

static hdql_Datum_t
_arith_op_scalar_interface_instantiate(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    assert(defData_);
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    struct ArithOpScalarState * state = hdql_alloc(ctx, struct ArithOpScalarState);
    assert(state);
    state->result = hdql_create_value(defData->evaluator->returnType, ctx);
    return (hdql_Datum_t) state;
}

static hdql_Datum_t
_arith_op_scalar_interface_dereference(
          hdql_Datum_t root
        , hdql_Datum_t s_
        , struct hdql_CollectionKey * key  // can be null
        , const hdql_Datum_t defData_
        , hdql_Context_t
        ) {
    assert(s_);
    //printf("XXX dereference()\n");
    // ^^^ TODO: set bp here to debug doubling dereference() -- it seems to be
    // a Query state issue wasting CPU and indicating that dereference/advance
    // sequence is generally invalid at this level...
    return ((struct ArithOpScalarState *) s_)->result;
}

static hdql_Datum_t
_arith_op_scalar_interface_reset(
          hdql_Datum_t newOwner
        , hdql_Datum_t state_
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    assert(state_);
    assert(defData_);
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    struct ArithOpScalarState * state = (struct ArithOpScalarState *) state_;
    hdql_query_reset(defData->args[0], newOwner, ctx);
    if(defData->args[1]) {
        hdql_query_reset(defData->args[1], newOwner, ctx);
    }
    hdql_Datum_t a =                    hdql_query_get(defData->args[0], NULL, ctx)
               , b = defData->args[1] ? hdql_query_get(defData->args[1], NULL, ctx) : NULL
               ;
    int rc = defData->evaluator->op(a, b, state->result);
    if(0 != rc) {
        hdql_context_err_push( ctx, HDQL_ERR_ARITH_OPERATION
                             , "Arithmetic error (%d) on operation %p:%p"
                             , rc, defData->evaluator, defData->evaluator->op
                             );
    }
    return state_;
}

static void
_arith_op_scalar_interface_destroy(
          hdql_Datum_t state_
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    if(NULL == state_) return;
    assert(state_);  /* dereference() without create() (and subsequent reset()) */
    struct ArithOpScalarState * state = (struct ArithOpScalarState *) state_;
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    if(NULL != state->result) {
        hdql_destroy_value(defData->evaluator->returnType, state->result, ctx);
    }
    /* TODO: deleting externally-allocated "definition data" does not seem to
     * be a good solution */
    //if(NULL != defData_) {
    //    if(defData->args[0]) {
    //        hdql_query_destroy(defData->args[0], ctx);
    //    }
    //    if(defData->args[1]) {
    //        hdql_query_destroy(defData->args[1], ctx);
    //    }
    //    hdql_context_free(ctx, defData_);
    //}
    hdql_context_free(ctx, state_);
}

const struct hdql_ScalarAttrInterface _hdql_gScalarArithOpIFace = {
    .definitionData = NULL,
    .instantiate = _arith_op_scalar_interface_instantiate,
    .dereference = _arith_op_scalar_interface_dereference,
    .reset = _arith_op_scalar_interface_reset,
    .destroy = _arith_op_scalar_interface_destroy,
};

