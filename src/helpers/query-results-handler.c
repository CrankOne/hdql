#include "hdql/helpers/query-results-handler.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/attr-def.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static int
_visit_ad( const hdql_AttrDef_t ad
         , struct hdql_ADVisitor * v
         , hdql_Context_t ctx
         , const struct hdql_AttrDefStack * prev
         , void * userdata
         ) {
    struct hdql_AttrDefStack s;
    s.attrName = NULL;
    s.ad = ad;
    s.prev = prev;

    void * adUD = NULL;
    hdql_QueryResultHandlerRC_t r = v->visit_attr_def(&s, userdata, &adUD);
    if(0x0 == r) {  /* ignore this attribute */
        assert(NULL == adUD);
        return 0;
    }
    if(0x1 == r) {  /* this is the compound user code would like to expand */
        const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
        size_t na = hdql_compound_get_nattrs_recursive(c);
        const char ** aNames = (const char **) alloca(na*sizeof(char*));
        hdql_compound_get_attr_names_recursive(c, aNames);
        for( size_t i = 0; i < na; ++i ) {
            const struct hdql_AttrDef * subAD = hdql_compound_get_attr(c, aNames[i]);
            _visit_ad(subAD, v, ctx, &s, userdata);
        }
    }
}

void
hdql_attr_def_visit( const hdql_AttrDef_t ad
        , struct hdql_ADVisitor * v
        , hdql_Context_t ctx
        ) {
    // ...
}


/* Allocates structure to track keys with their flattened views
 *
 * Forwards call to interface's `handle_keys()`.
 *
 * \note Entire query object is required since full keys chain allocation
 *       implies recursive traversal.
 * */
static int
_query_result_table_init_keys( struct hdql_QueryResultsWorkspace * ws ) {
    ws->keys = NULL;
    int rc = hdql_query_keys_reserve(ws->q, &ws->keys, ws->ctx);
    assert(rc == 0);
    ws->flatKeyViewLength = hdql_keys_flat_view_size(ws->keys, ws->ctx);
    ws->flatKeyViews = ws->flatKeyViewLength
                      ? (struct hdql_KeyView *) malloc(sizeof(struct hdql_KeyView)*ws->flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(ws->q, ws->keys, ws->flatKeyViews, ws->ctx);
    assert(ws->iqr->handle_keys);
    return ws->iqr->handle_keys(
              ws->keys
            , ws->flatKeyViews
            , ws->flatKeyViewLength
            , ws->iqr->userdata);
}

/*
 * init
 */

static void _handle_collection_as_entry(      struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_AttrHandler * ad);
static void _handle_scalar_compound_as_entry( struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_AttrHandler * ad);
static void _handle_scalar_atomic_as_entry(   struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_AttrHandler * ad);
static void _handle_scalar_atomic_value_as_entry(   struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_AttrHandler * ad);

static int
_results_handler_set_result_type( const struct hdql_AttrDef * ad
            , struct hdql_QueryResultsWorkspace * ws
            ) {

    if(ws->rootObjectHandler.ad != NULL) {
        /* make sure it hasn't being called before */
        return -1;  // TODO: dedicated EC
    }
    ws->rootObjectHandler.ad = ad;
    /* Special case here is when root type is a collection. Contrary to
     * in-depth iteration of the query result (assigned
     * by _assign_value_handler(), we expect query itslef to iterate over this
     * collection, so we must consider datum as a scalar item here. */
    if(hdql_attr_def_is_atomic(ad)) {
        struct hdql_ValueTypes * valTypes = hdql_context_get_types(ws->ctx);
        assert(valTypes);
        const struct hdql_ValueInterface * vi
                = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(ad));

        ws->rootObjectHandler.value_handler = _handle_scalar_atomic_as_entry;
        if(hdql_attr_def_is_scalar(ad)) {
            if(ws->iqr->init_atomic_ad) {
                ws->rootObjectHandler.payload.atomicAttrData = ws->iqr->init_atomic_ad(ad, vi, ws->iqr->userdata);
                /* TODO: check result? */
            }
        } else {
            assert(hdql_attr_def_is_collection(ad));
            /* This is a special (and simplest possible) case when query
             * yields a series of scalars that this handler must not iterate
             * by itself. Just use this datum to stringify as is. */
            if(ws->iqr->init_atomic_ad) {
                ws->rootObjectHandler.payload.atomicAttrData = ws->iqr->init_atomic_ad(ad, vi, ws->iqr->userdata);
                /* TODO: check result? */
            }
            ws->rootObjectHandler.value_handler = _handle_scalar_atomic_value_as_entry;
        }
    } else if(hdql_attr_def_is_compound(ad)) {

    }
    #ifndef DNDEBUG
    else {
        char bf[128];
        hdql_top_attr_str(ad, bf, sizeof(bf), ws->ctx);
        fputs("DEBUG ASSERTION FAILED: logic error: can't handle top level attr def of query result"
                " yielding type `", stderr);
        fputs(bf, stderr);
        fputs("'.\n", stderr);
        assert(0);
    }
    #endif
    return 0;
}

struct hdql_QueryResultsWorkspace *
hdql_query_results_init(
          struct hdql_Query * q
        , const char ** attrs
        , struct hdql_iQueryResultsHandler * iqr
        , struct hdql_Context * ctx
        ) {
    if(NULL == iqr->handle_record) {
        /* TODO: communicate otherwise */
        fprintf(stderr, "Query results handler interface does not provide"
                " implementation function to handle record.\n");
        return NULL;
    }
    int rc;
    /* allocate workspace */
    struct hdql_QueryResultsWorkspace * ws = (struct hdql_QueryResultsWorkspace *)
        hdql_context_alloc(ctx, sizeof(struct hdql_QueryResultsWorkspace));

    /* set query instance, context and ptr to the interface in use */
    ws->q   = q;
    ws->ctx = ctx;
    ws->iqr = iqr;

    /* if attribute handling is enabled in iface implem, process attributes */
    const struct hdql_AttrDef * ad = hdql_query_top_attr(q);
    assert(ad);
    if(0 != (rc = _results_handler_set_result_type(ad, ws))) {
        /* TODO communicate error other way */
        fprintf(stderr, "Can't process attributes of query result: %d\n", rc);
        return NULL;
    }

    if(iqr->handle_keys) {
        if(0 != (rc = _query_result_table_init_keys(ws))) {
            /* TODO communicate error other way */
            fprintf(stderr, "Can't process keys of query result: %d\n", rc);
            return NULL;
        }
    } else {
        ws->flatKeyViewLength = 0;
        ws->keys = NULL;
        ws->flatKeyViews = NULL;
    }
    if(iqr->finalize_schema) iqr->finalize_schema(iqr->userdata);

    return ws;
}

int
hdql_query_results_process_records_from( struct hdql_Datum * d
        , struct hdql_QueryResultsWorkspace * ws ) {
    int rc = hdql_query_reset(ws->q, d, ws->ctx);
    if(rc != HDQL_ERR_CODE_OK) {
        /* TODO communicate error other way */
        fprintf(stderr, "Can't set query subject: %d\n", rc);
        return rc;
    }
    hdql_Datum_t r;
    while(NULL != (r = hdql_query_get(ws->q, ws->keys, ws->ctx))) {
        ws->iqr->handle_record(r, ws->iqr->userdata);
    }
    return HDQL_ERR_CODE_OK;
}

int
hdql_query_results_destroy( struct hdql_QueryResultsWorkspace * ws ) {
    if(ws->flatKeyViews) free(ws->flatKeyViews);
    if(ws->keys) hdql_query_keys_destroy(ws->keys, ws->ctx);
    hdql_context_free(ws->ctx, (hdql_Datum_t) ws);
    return 0;
}

