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

/* Init workspace with query result attribs
 *
 * With given query's top attribute definition, retrieves the resulting type
 * and for every selected attribute forwards execution to
 * interface's `handle_attr_def()`.
 *
 * If \p columns is null, then either single column titled as NULL string will
 * be expected (when query result is scalar value) or all scalar columns of a
 * resulting compound will be obtained.
 * */
static int
_query_result_table_init_attrs( const struct hdql_AttrDef * ad
        , const char ** attrs
        , struct hdql_iQueryResultsHandler * iqr
        , struct hdql_Context * ctx
        ) {
    int rc;
    if(hdql_attr_def_is_compound(ad)) {
        /* iterate over (selected) compound attributes and let the object
         * behind the interface know, in which order we are going to provide
         * values and of which type */
        const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
        /* Loop over compound keys */
        size_t nAttrs = hdql_compound_get_nattrs(c);
        assert(nAttrs > 0);
        const char ** attrNames = (const char **) alloca(sizeof(char*)*(nAttrs+1));
        hdql_compound_get_attr_names(c, attrNames);
        attrNames[nAttrs] = NULL;
        const char ** selectedColumns = attrs ? attrs : attrNames;
        for( const char ** colNamePtr = selectedColumns; *colNamePtr; ++colNamePtr ) {
            const struct hdql_AttrDef * cAD = hdql_compound_get_attr(c, *colNamePtr);
            rc = iqr->handle_attr_def(*colNamePtr, cAD, iqr->userdata);
            if(rc < 0) {
                /* TODO: communicate error other way */
                fprintf(stderr, "Error while communicating attribute"
                        " definition of a query result \"%s\": %d\n"
                        , *colNamePtr, rc);
                return rc;
            }
        }
    } else {
        /* query results in scalar value */
        assert(hdql_attr_def_is_atomic(ad));
        const char * singularColName;
        if(attrs && attrs[0] && attrs[0][0] != '\0') {
            if(attrs[1] && attrs[1][0] != '\0') {
                /* This means, that more than one column name is provided,
                 * while query results in a singular value. Most probably
                 * indicates error. */
                /* TODO: communicate error other way */
                fprintf(stderr, "Query results in atomic value, while multiple"
                        " attributes are expected by selection.\n");
                return HDQL_ERR_GENERIC;
            }
        } else {
            singularColName = "";
        }
        rc = iqr->handle_attr_def(singularColName, ad, iqr->userdata);
        if(rc < 0) return rc;
    }
    return 0;
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
    ws->kv = ws->flatKeyViewLength
                      ? (struct hdql_KeyView *) malloc(sizeof(struct hdql_KeyView)*ws->flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(ws->q, ws->keys, ws->kv, ws->ctx);
    assert(ws->iqr->handle_keys);
    return ws->iqr->handle_keys(ws->keys, ws->kv, ws->flatKeyViewLength, ws->iqr->userdata);
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
    if(iqr->handle_attr_def) {
        const struct hdql_AttrDef * ad = hdql_query_top_attr(q);
        assert(ad);
        if(0 != (rc = _query_result_table_init_attrs(ad, attrs, iqr, ctx))) {
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
hdql_query_results_destroy( struct hdql_QueryResultsWorkspace * ) {
    return -1;  // TODO
}

