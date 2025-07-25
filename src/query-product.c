#include "hdql/query-product.h"
#include "hdql/attr-def.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include <assert.h>
#include <stdio.h>

int
hdql_query_product_reset( struct hdql_Query ** qs
        , struct hdql_CollectionKey ** keys
        , hdql_Datum_t * values
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t ctx
        ) {
    int rc;
    /* re-set all queries in list,  */
    size_t i = 0;
    for(struct hdql_Query ** q = qs; i < n; ++q, ++i) {
        if(HDQL_ERR_CODE_OK != (rc = hdql_query_reset(*q, d, ctx))) return rc;
    }
    /* get 1st set of values */
    struct hdql_CollectionKey ** k = keys ? keys : NULL;
    hdql_Datum_t * v = values;
    i = 0;
    for(struct hdql_Query ** q = qs; i < n; ++q, ++v, ++i) {
        if(NULL != (*v = hdql_query_get(*q, k ? *k : NULL, ctx))) {
            if(k) ++k;
            continue;  /* query yielded value */
        }
        /* Otherwise at least one query did not provide a value.
         * That results in an empty set */
        return HDQL_ERR_EMPTY_SET;  /* todo: shouldn't we re-set ones already iterated? */
    }
    return HDQL_ERR_CODE_OK;
}

int
hdql_query_product_advance( struct hdql_Query ** qs
        , struct hdql_CollectionKey ** keys
        , hdql_Datum_t * values
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t ctx
        ) {
    int rc;
    const size_t n1 = n-1;
    /* iterate backward */
    size_t i = n;
    assert(i > 0);
    struct hdql_CollectionKey ** k = keys ? keys + n1 : NULL;
    hdql_Datum_t * v = values + n1;
    for(struct hdql_Query ** q = qs + n1; 0 != i; --q, --v) {
        --i;
        if(NULL == (*v = hdql_query_get(*q, k ? *k : NULL, ctx)))
            continue;  /* i-th query did not yield a value */
        /* otherwise, re-set ones to the right (with j > i) */
        for(size_t j = i + 1; j < n; ++j) {
            if(HDQL_ERR_CODE_OK != (rc = hdql_query_reset(qs[j], d, ctx))) {
                assert(0);
                return rc;
            }
            values[j] = hdql_query_get(qs[j], keys ? keys[j] : NULL, ctx);
            assert(NULL != values[j]);  /* how did we passed first reset() then? */
        }
        return HDQL_ERR_CODE_OK;
    }
    /* none of queries in the list yielded values */
    return HDQL_ERR_EMPTY_SET;
}

#if 0
struct hdql_QueryProduct {
    struct hdql_Query ** qs;
    bool exhausted;
};

struct hdql_QueryProduct *
hdql_query_product_create( struct hdql_Query ** qs
                         , hdql_Datum_t ** values_
                         , hdql_Context_t ctx
                         ) {
    struct hdql_QueryProduct * qp = hdql_alloc(ctx, struct hdql_QueryProduct);
    qp->nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++(qp->nQueries)) {}
    if(0 == qp->nQueries) {
        hdql_context_free(ctx, (hdql_Datum_t) qp);
        return NULL;
    }
    /* allocate storages for query pointers, keys, results */
    qp->qs      = (struct hdql_Query **)         hdql_context_alloc(ctx, (qp->nQueries+1)*sizeof(struct hdql_Query*));
    qp->keys    = (struct hdql_CollectionKey **) hdql_context_alloc(ctx,   (qp->nQueries)*sizeof(struct hdql_CollectionKey*));
    qp->values  = (hdql_Datum_t *) hdql_context_alloc(ctx, (qp->nQueries)*sizeof(hdql_Datum_t));
    *values_ = qp->values;
    size_t nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {
        /* copy query */
        qp->qs[nQueries] = qs[nQueries];
        /* copy key */
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

bool
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
        if(keys) ++keys;  /* increment keys ptr */
        if(NULL != *value) continue;  /* ok, go to next query */
        {   /* i-th value is not retrieved, product results in an empty set,
             * we should drop the result */
            hdql_Datum_t * argValue2 = qp->values;
            /* iterate till i-th inclusive, wipe stored datum pointer */
            for(struct hdql_Query ** q2 = qp->qs; q2 <= q; ++q2, ++argValue2 ) {
                hdql_query_reset(*q2, root, context);
                *argValue2 = NULL;
            }
            return false;  /* communicate no product result */
        }
    }
    return true;  /* ok, product at its ground state */
}

struct hdql_CollectionKey **
hdql_query_product_reserve_keys( struct hdql_Query ** qs
        , hdql_Context_t context ) {
    /* count number of queries (null-terminated sequence) */
    size_t nQueries = 0;
    for(struct hdql_Query ** q = qs; NULL != *q; ++q) { ++nQueries; }
    /* allocate root keys for queries */
    struct hdql_CollectionKey ** keys = (struct hdql_CollectionKey**)
        hdql_context_alloc(context, sizeof(struct hdql_CollectionKey *)*nQueries);
    /* reserve keys */
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
#endif
