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
 * Situation when both arguments are collections is deliberately must not be
 * permitted for simple arithmetic operations.
 *
 * To accomplish this task an "overloaded" advance operation is implemented
 * here as a choice of two callbacks.
 */

struct ArithOpCollectionState {
    struct hdql_ArithOpDefData * defData;

    bool isExhausted, isValid;
    hdql_Datum_t a, b;
    struct hdql_CollectionKey * cRKey;
    hdql_Datum_t cResult;
    hdql_Context_t context;

    void (*advance)(struct ArithOpCollectionState *);
};

static void
_advance_a(struct ArithOpCollectionState * state) {
    state->a = hdql_query_get(state->defData->args[0], state->cRKey, state->context);
    if(NULL == state->a) state->isExhausted = true;
    state->isValid = false;
}

static void
_advance_b(struct ArithOpCollectionState * state) {
    state->b = hdql_query_get(state->defData->args[1], state->cRKey, state->context);
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
        /* first arg is collection */
        state->advance = _advance_a;
        hdql_query_keys_reserve( state->defData->args[0]
                               , &(state->cRKey)
                               , ctx ); 
    } else {
        /* second arg is collection */
        state->advance = _advance_b;
        hdql_query_keys_reserve( state->defData->args[1]
                               , &(state->cRKey)
                               , ctx );
    }
    state->cResult = hdql_create_value( state->defData->evaluator->returnType
                                      , ctx
                                      );
    state->context = ctx;
    return (hdql_It_t) state;
}

static hdql_Datum_t
_arith_op_collection_dereference( hdql_It_t it_
                                , struct hdql_CollectionKey * keyPtr
                                ) {
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    if(state->isExhausted) return NULL;
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
    if(keyPtr) {
        assert(keyPtr->isList);
        hdql_query_keys_copy(keyPtr->pl.keysList, state->cRKey, state->context);
    }
    return state->cResult;
}

static hdql_It_t
_arith_op_collection_advance( hdql_It_t it_ ) {
    assert(it_);
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    state->advance(state);
    return it_;
}

static hdql_It_t
_arith_op_collection_reset( hdql_It_t it_
                          , hdql_Datum_t newOwner
                          , const hdql_Datum_t defData
                          , hdql_SelectionArgs_t
                          , hdql_Context_t ctx
                          ) {
    struct ArithOpCollectionState * state = (struct ArithOpCollectionState *) it_;
    assert(state->context == ctx);
    hdql_query_reset(state->defData->args[0], newOwner, ctx);
    if(state->defData->args[1]) {
        hdql_query_reset(state->defData->args[1], newOwner, ctx);
    }
    /* retrieve scalar argument only once */
    if(state->advance == _advance_a) {
        if(state->defData->args[1]) {
            state->b = hdql_query_get( state->defData->args[1]
                                     , NULL
                                     , ctx );
            if(NULL == state->b) state->isExhausted = true;
        }
    } else {
        assert(state->advance == _advance_b);
        state->a = hdql_query_get( state->defData->args[0]
                                 , NULL
                                 , ctx );
        if(NULL == state->a) state->isExhausted = true;
    }
    state->advance(state);
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
    if(NULL != state->cRKey) {
        hdql_query_keys_destroy(state->cRKey, ctx);
    }
    /* TODO: deleting externally-allocated "definition data" does not seem to
     * be a good solution */
    //if(NULL != state->defData) {
    //    hdql_context_free(ctx, (hdql_Datum_t) state->defData);
    //}
    hdql_context_free(ctx, (hdql_Datum_t) it_);
}

struct hdql_CollectionKey *
hdql_reserve_arith_op_collection_key(
          const hdql_Datum_t dd_
        , hdql_Context_t context
        ) {
    struct hdql_ArithOpDefData * dd = (struct hdql_ArithOpDefData *) dd_;
    bool aIsFullyScalar = hdql_query_is_fully_scalar(dd->args[0]);
    #ifndef NDEBUG
    bool bIsFullyScalar = dd->args[1] ? hdql_query_is_fully_scalar(dd->args[1]) : true;
    assert(aIsFullyScalar != bIsFullyScalar);  /* both scalars and both collections are prohibited */
    #endif
    struct hdql_CollectionKey * key;
    int rc;
    if(!aIsFullyScalar) {
        rc = hdql_query_keys_reserve( dd->args[0]
                                    , &key
                                    , context );
    } else {
        rc = hdql_query_keys_reserve( dd->args[1]
                                    , &key
                                    , context );
    }
    if(0 != rc) {
        hdql_context_err_push( context, HDQL_ERR_INTERFACE_ERROR
                             , "Key allocation error (%d) on operation %p:%p"
                             , rc, dd->evaluator, dd->evaluator->op
                             );
        return NULL;
    }
    return key;
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

