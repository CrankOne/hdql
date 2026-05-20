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
 * Situation when both arguments are collections is deliberately forbidden
 * for simple arithmetic operations.
 *
 * To accomplish this task an "overloaded" advance operation is implemented
 * here as a choice of two callbacks.
 */

struct ArithOpCollectionState {
    struct hdql_ArithOpDefData * defData;

    bool isExhausted, isValid;
    hdql_Datum_t a, b;
    hdql_Datum_t cResult;
    hdql_Context_t context;

    void (*advance)(struct ArithOpCollectionState *, struct hdql_Key * key);
};

static void
_advance_a(struct ArithOpCollectionState * state, struct hdql_Key * key) {
    state->a = hdql_query_get(state->defData->args[0], key, state->context);
    if(NULL == state->a) state->isExhausted = true;
    state->isValid = false;
}

static void
_advance_b(struct ArithOpCollectionState * state, struct hdql_Key * key) {
    state->b = hdql_query_get(state->defData->args[1], key, state->context);
    if(NULL == state->b) state->isExhausted = true;
    state->isValid = false;
}

static hdql_It_t
_arith_op_collection_create( hdql_Datum_t owner
                           , const hdql_Datum_t defData_
                           , hdql_SelectionArgs_t selArgs_
                           , hdql_Context_t ctx
                           ) {
    struct ArithOpCollectionState * state
        = (struct ArithOpCollectionState *) hdql_context_alloc(ctx, sizeof(struct ArithOpCollectionState));
    state->defData = (struct hdql_ArithOpDefData *) defData_;
    state->isExhausted = state->isValid = false;
    bool aIsFullyScalar = hdql_query_is_fully_scalar(state->defData->args[0]);
    #ifndef NDEBUG
    bool bIsFullyScalar = state->defData->args[1] ? hdql_query_is_fully_scalar(state->defData->args[1]) : true;
    assert(aIsFullyScalar != bIsFullyScalar);  /* both scalars and both collections are prohibited */
    #endif
    if(!aIsFullyScalar) {
        assert(bIsFullyScalar);
        /* a is collection */
        state->advance = _advance_a;
    } else {
        /* b is collection */
        state->advance = _advance_b;
    }
    state->cResult = hdql_create_value( state->defData->evaluator->returnType
                                      , ctx
                                      );
    state->context = ctx;
    return (hdql_It_t) state;
}

static hdql_Datum_t
_arith_op_collection_dereference( hdql_It_t it_ ) {
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    if(state->isExhausted) {
        return NULL;
    }
    if(!state->isValid) {
        int rc = state->defData->evaluator->op(state->a, state->b, state->cResult);
        if(rc != 0) {
            hdql_context_err_push( state->context, HDQL_ERR_ARITH_OPERATION
                             , "Arithmetic error (%d) on operation %p:%p"
                             , rc, state->defData->evaluator, state->defData->evaluator->op
                             );
            return NULL;
        } else {
            state->isValid = true;
        }
    }
    return state->cResult;
}

static hdql_It_t
_arith_op_collection_advance( hdql_It_t it_, struct hdql_Key * keyPtr ) {
    assert(it_);
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    state->advance(state, keyPtr);
    return it_;
}

static hdql_It_t
_arith_op_collection_reset( hdql_It_t it_
                          , hdql_Datum_t newOwner
                          , const hdql_Datum_t defData
                          , hdql_SelectionArgs_t sel
                          , hdql_Context_t ctx
                          ) {
    ((void) sel);
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    assert(state->context == ctx);
    hdql_query_reset(state->defData->args[0], newOwner, ctx);
    if(state->defData->args[1]) {
        hdql_query_reset(state->defData->args[1], newOwner, ctx);
    }
    /* retrieve scalar argument only once */
    if(state->advance == _advance_a) {
        state->b = hdql_query_get( state->defData->args[1]
                                 , NULL
                                 , ctx );
        if(NULL == state->b) {
            state->isExhausted = true;
        } else state->isExhausted = false;
    } else {
        assert(state->advance == _advance_b);
        state->a = hdql_query_get( state->defData->args[0]
                                 , NULL
                                 , ctx );
        if(NULL == state->a) {
            state->isExhausted = true;
        } else state->isExhausted = false;
    }
    return (hdql_It_t) state;
}

static void
_arith_op_collection_destroy( hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    /* TODO: perhaps, redundant checks to handle remergency delete situation... */
    if(NULL == it_) return;
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    assert(state->context == ctx);
    if(NULL != state->cResult && state->defData->evaluator && state->defData->evaluator->returnType) {
        hdql_destroy_value(state->defData->evaluator->returnType, state->cResult, ctx);
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
    hdql_context_free(ctx, (hdql_Datum_t) it_);
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
    , .create = _arith_op_collection_create
    , .dereference = _arith_op_collection_dereference
    , .advance = _arith_op_collection_advance
    , .reset = _arith_op_collection_reset
    , .destroy = _arith_op_collection_destroy
    , .compile_selection = NULL
    , .free_selection = NULL
};

