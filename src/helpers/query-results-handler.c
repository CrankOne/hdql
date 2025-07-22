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

struct hdql_QueryResultsWorkspace {
    struct hdql_Query * q;
    struct hdql_Context * ctx;
    struct hdql_iQueryResultsHandler * iqr;

    /* Keys pointer */
    struct hdql_CollectionKey * keys;
    /* Number of key flat views */
    size_t flatKeyViewLength;
    /* Keys flat views array */
    struct hdql_KeyView * kv;
};

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
    ws->kv = ws->flatKeyViewLength
                      ? (struct hdql_KeyView *) malloc(sizeof(struct hdql_KeyView)*ws->flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(ws->q, ws->keys, ws->kv, ws->ctx);
    assert(ws->iqr->handle_keys);
    return ws->iqr->handle_keys( ws->keys, ws->kv, ws->flatKeyViewLength
            , ws->iqr->userdata);
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
    if(iqr->handle_result_type) {
        const struct hdql_AttrDef * ad = hdql_query_top_attr(q);
        assert(ad);
        if(0 != (rc = iqr->handle_result_type(ad, iqr->userdata))) {
            /* TODO communicate error other way */
            fprintf(stderr, "Can't process attributes of query result: %d\n", rc);
            return NULL;
        }
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
        ws->kv = NULL;
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
    if(ws->kv) free(ws->kv);
    if(ws->keys) hdql_query_keys_destroy(ws->keys, ws->ctx);
    hdql_context_free(ws->ctx, (hdql_Datum_t) ws);
    return 0;
}

