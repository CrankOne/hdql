#include "hdql/query-product.h"

#include <assert.h>

struct hdql_QueryProduct {
    struct hdql_Query ** qs;

    struct hdql_CollectionKey ** keys;
    hdql_Datum_t * values;
};

struct hdql_QueryProduct *
hdql_query_product_create( struct hdql_Query ** qs
                         , hdql_Datum_t ** values_
                         , hdql_Context_t ctx
                         ) {
    struct hdql_QueryProduct * qp = hdql_alloc(ctx, struct hdql_QueryProduct);
    size_t nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {}
    if(0 == nQueries) {
        hdql_context_free(ctx, (hdql_Datum_t) qp);
        return NULL;
    }
    qp->qs      = (struct hdql_Query **) hdql_context_alloc(ctx, (nQueries+1)*sizeof(struct hdql_Query*));
    qp->keys    = (struct hdql_CollectionKey **) hdql_context_alloc(ctx, (nQueries)*sizeof(struct hdql_CollectionKey*));
    qp->values  = (hdql_Datum_t *) hdql_context_alloc(ctx, (nQueries)*sizeof(hdql_Datum_t));
    *values_ = qp->values;
    nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {
        qp->qs[nQueries] = qs[nQueries];
        hdql_query_keys_reserve(qp->qs[nQueries], qp->keys + nQueries, ctx);
        qp->values[nQueries] = NULL;
    }
    qp->qs[nQueries++] = NULL;
    return qp;
}

bool
hdql_query_product_advance(
          hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context
        ) {
    // keys iterator (can be null)
    struct hdql_CollectionKey ** keys = qp->keys;
    // datum instance pointer currently set
    hdql_Datum_t * value = qp->values;

    for(struct hdql_Query ** q = qp->qs; NULL != *q; ++q ) {
        hdql_query_reset(*q, root, context);  // TODO: check rc
    }
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(struct hdql_Query ** q = qp->qs; NULL != *q; ++q, ++value ) {
        assert(*q);
        // retrieve next item from i-th list and consider it as an argument
        *value = hdql_query_get(*q, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        // if i-th value is retrieved, nothing else should be done with the
        // argument list
        if(NULL != *value) {
            return true;
        }
        // i-th generator depleted
        // - if it is the last query list, we have exhausted argument list
        if(NULL == *(q+1)) {
            return false;
        }
        // - otherwise, reset it and all previous and proceed with next
        {
            // supplementary iteration pointers to use at re-set
            struct hdql_CollectionKey ** keys2 = qp->keys;
            hdql_Datum_t * argValue2 = qp->values;
            // iterate till i-th inclusive
            for(struct hdql_Query ** q2 = qp->qs; q2 <= q; ++q2, ++argValue2 ) {
                assert(*q2);
                // re-set the query
                hdql_query_reset(*q2, root, context);
                // get 1st value of every query till i-th, inclusive
                *argValue2 = hdql_query_get(*q2, *keys2, context);
                // since we've iterated it already, a 1st lements of j-th
                // query should always exist, otherwise one of the argument
                // queris is not idempotent and that's an error
                assert(*argValue2);
                if(keys2) ++keys2;
            }
        }
    }
    assert(false);  // unforeseen loop exit, algorithm error
}

void
hdql_query_product_reset( hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context ) {
    struct hdql_CollectionKey ** keys = qp->keys;
    hdql_Datum_t * value = qp->values;

    for(struct hdql_Query ** q = qp->qs; NULL != *q; ++q ) {
        hdql_query_reset(*q, root, context);  // TODO: check rc
    }
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(struct hdql_Query ** q = qp->qs; NULL != *q; ++q, ++value ) {
        assert(*q);
        hdql_query_reset(*q, root, context);
        *value = hdql_query_get(*q, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        if(NULL != *value) {
            continue;
        }
        {   // i-th value is not retrieved, product results in an empty set,
            // we should drop the result
            hdql_Datum_t * argValue2 = qp->values;
            // iterate till i-th inclusive
            for(struct hdql_Query ** q2 = qp->qs; q2 <= q; ++q2, ++argValue2 ) {
                hdql_query_reset(*q2, root, context);
                *argValue2 = NULL;
            }
            return;
        }
    }
    assert(false);  // unforeseen loop exit, algorithm error
}

struct hdql_CollectionKey **
hdql_query_product_reserve_keys( struct hdql_Query ** qs
        , hdql_Context_t context ) {
    size_t nQueries = 0;
    for(struct hdql_Query ** q = qs; NULL != *q; ++q) { ++nQueries; }
    struct hdql_CollectionKey ** keys = (struct hdql_CollectionKey**)
        hdql_context_alloc(context, sizeof(struct hdql_CollectionKey *)*nQueries);
    struct hdql_CollectionKey ** cKey = keys;
    for(struct hdql_Query ** q = qs; NULL != *q; ++q, ++cKey) {
        hdql_query_keys_reserve(*q, cKey, context);
    }
    return keys;
}

struct hdql_Query **
hdql_query_product_get_query(struct hdql_QueryProduct * qp) {
    return qp->qs;
}

void
hdql_query_product_destroy(
            struct hdql_QueryProduct *qp
          , hdql_Context_t context
          ) {
    struct hdql_CollectionKey ** cKey = qp->keys;
    for(struct hdql_Query ** cq = qp->qs; NULL != *cq; ++cq, ++cKey) {
        hdql_query_keys_destroy(*cKey, context);
        *cKey = NULL;
    }

    hdql_context_free(context, (hdql_Datum_t) qp->values);
    hdql_context_free(context, (hdql_Datum_t) qp->keys);
    hdql_context_free(context, (hdql_Datum_t) qp->qs);

    hdql_context_free(context, (hdql_Datum_t) qp);
}
