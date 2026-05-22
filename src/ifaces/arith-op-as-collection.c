#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/internal-ifaces.h"
#include "hdql/operations.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <assert.h>

/*
 * Unary or binary arithmetic operation node for collection arguments resulting
 * a collection.
 *
 * This interface implements the case when:
 *  - one (or single one) of the argument queries is collection
 *  - other is either absent (for unary operator) or is scalar
 * Situation when both arguments are collections is prohibited for simple
 * arithmetic operations.
 *
 * To accomplish this task an "overloaded" advance operation is implemented
 * here as a choice of two callbacks. */

struct ArithOpCollectionState {
    unsigned int nArgToAdvance:1;
    unsigned int exhausted:1;
    hdql_Datum_t other;
    hdql_Datum_t cResult;
};

static hdql_It_t
_arith_op_new_iterator( hdql_Datum_t owner
                      , const hdql_Datum_t defData_
                      , hdql_Context_t ctx
                      ) {
    const struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    struct ArithOpCollectionState * state
        = (struct ArithOpCollectionState *)
            hdql_context_alloc(ctx, sizeof(struct ArithOpCollectionState));
    bool aIsFullyScalar = hdql_query_is_fully_scalar(defData->args[0]);
    state->nArgToAdvance = aIsFullyScalar ? 1 : 0;
    state->exhausted = 0x1;
    state->cResult = hdql_create_value(defData->evaluator->returnType, ctx);
    return (hdql_It_t) state;
}

static hdql_Datum_t
_arith_op_collection_iterator_reset( hdql_It_t it_
                          , hdql_Datum_t newOwner
                          , const struct hdql_Datum *defData_
                          , hdql_SelectionArgs_t sel
                          , struct hdql_Key *key
                          , hdql_Context_t context
                          ) {
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    hdql_Datum_t cr = hdql_query_reset( defData->args[state->nArgToAdvance]
                                      , newOwner
                                      , state->nArgToAdvance ? key : NULL
                                      , context);
    state->other = hdql_query_reset(defData->args[!state->nArgToAdvance]
                    , newOwner, NULL, context);
    if(!cr || (defData->args[1] && !state->other) ) {
        state->exhausted = 0x1;
        return NULL;
    }
    state->exhausted = 0x0;
    if(state->nArgToAdvance) {
        defData->evaluator->op(state->other, cr, state->cResult);
    } else {
        defData->evaluator->op(cr, state->other, state->cResult);
    }
    return state->cResult;
}

static hdql_Datum_t
_arith_op_collection_yield( hdql_It_t it_
        , const struct hdql_Datum * defData_
        , struct hdql_Key * key
        , struct hdql_Context * context
        ) {
    assert(it_);
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    if(state->exhausted) return it_;  /* exhausted, do nothing */
    hdql_query_get(defData->args[0], key, context);
    return it_;
}


static void
_arith_op_collection_destroy_iterator( hdql_It_t it_
        , const struct hdql_Datum * defData_
        , hdql_Context_t context
        ) {
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) defData_;
    /* TODO: perhaps, redundant checks to handle remergency delete situation... */
    if(NULL == it_) return;
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    if(NULL != state->cResult && defData->evaluator && defData->evaluator->returnType) {
        hdql_destroy_value(defData->evaluator->returnType, state->cResult, context);
    }
    //if(NULL != state->defData) {
    //    if(state->defData->args[0]) {
    //        hdql_query_destroy(state->defData->args[0], ctx);
    //    }
    //    if(state->defData->args[1]) {
    //        hdql_query_destroy(state->defData->args[1], ctx);
    //    }
    //}
    /* TODO: deleting externally-allocated "definition data" does not seem to
     * be a good solution */
    //if(NULL != state->defData) {
    //    hdql_context_free(ctx, (hdql_Datum_t) state->defData);
    //}
    hdql_context_free(context, (hdql_Datum_t) it_);
}

int
hdql_reserve_arith_op_collection_key(struct hdql_Key * key
        , const hdql_Datum_t dd_
        , hdql_Context_t context
        ) {
    struct hdql_ArithOpDefData * dd = (struct hdql_ArithOpDefData *) dd_;
    bool aIsFullyScalar = hdql_query_is_fully_scalar(dd->args[0]);
    #ifndef NDEBUG
    bool bIsFullyScalar = dd->args[1] ? hdql_query_is_fully_scalar(dd->args[1]) : true;
    assert(aIsFullyScalar != bIsFullyScalar);  /* both scalars and both collections are prohibited */
    #endif
    int rc;
    if(!aIsFullyScalar) {
        /* a is a collection */
        rc = hdql_key_reserve_for_query( dd->args[0]
                                       , key
                                       , context );
    } else {
        /* b is a collection */
        rc = hdql_key_reserve_for_query( dd->args[1]
                                       , key
                                       , context );
    }
    if(0 != rc) {
        hdql_context_err_push( context, HDQL_ERR_INTERFACE_ERROR
                             , "Key allocation error (%d) on operation %p:%p"
                             , rc, dd->evaluator, dd->evaluator->op
                             );
        return HDQL_ERR_INTERFACE_ERROR;
    }
    return HDQL_ERR_CODE_OK;
}

const struct hdql_CollectionAttrInterface _hdql_gCollectionArithOpIFace = {
      .definitionData = NULL
    , .new_iterator = _arith_op_new_iterator
    , .yield = _arith_op_collection_yield
    , .reset_iterator = _arith_op_collection_iterator_reset
    , .destroy_iterator = _arith_op_collection_destroy_iterator
    , .compile_selection = NULL
    , .free_selection = NULL
};

