#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/internal-ifaces.h"
#include "hdql/types.h"

#include <assert.h>

/*
 * Sub-query as-collection interface
 *
 * Implements collection interface for forwarding query
 */

struct FwdQueryScalarState {
    struct hdql_Query * subQuery;
    hdql_Datum_t result;
    struct hdql_Key * key;
    hdql_Context_t context;
};

static hdql_Datum_t
_fwd_query_scalar_interface_instantiate(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    struct FwdQueryScalarState * it = hdql_alloc(ctx, struct FwdQueryScalarState);
    assert(definitionData);  /* required as should bring fwd query target */
    it->subQuery = (struct hdql_Query *) definitionData;
    assert(it->subQuery);
    it->result = NULL;
    it->key = hdql_key_new(ctx);
    it->context = ctx;
    int rc = hdql_key_reserve_for_query(it->subQuery, it->key, ctx);
    assert(rc == 0);
    assert(it->key);
    return (hdql_Datum_t) it;
}

static hdql_Datum_t
_fwd_query_scalar_interface_dereference(
          hdql_Datum_t root
        , hdql_Datum_t dd_
        , struct hdql_Key * key  // can be null
        , const hdql_Datum_t defData
        , hdql_Context_t
        ) {
    assert(dd_);  /* dereference() without create() (and subsequent reset()) */
    struct FwdQueryScalarState * dd = (struct FwdQueryScalarState *) dd_;
    if(key) {
        assert(dd->key);
        assert(hdql_key_is_list(key));
        hdql_key_copy_value(key, dd->key, dd->context);
    }
    return dd->result;
}

static hdql_Datum_t
_fwd_query_scalar_interface_reset(
          hdql_Datum_t newOwner
        , hdql_Datum_t dd_
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    assert(dd_);  /* reset() without create() */
    struct FwdQueryScalarState * dd = (struct FwdQueryScalarState *) dd_;
    assert(dd->context == ctx);  /* fwd q had changed context between create() and reset() */
    #ifndef NDEBUG
    {
        struct hdql_Query * fwdQ = (struct hdql_Query*) defData;
        assert(fwdQ == dd->subQuery);  /* fwd q had changed query between create() and reset() */
    }
    #endif
    hdql_query_reset(dd->subQuery, newOwner, ctx);
    dd->result = hdql_query_get(dd->subQuery, dd->key, ctx);
    return dd_;
}

static void
_fwd_query_scalar_interface_destroy(
          hdql_Datum_t dd_
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    struct FwdQueryScalarState * dd = (struct FwdQueryScalarState *) dd_;
    if(dd && dd->key) {
        hdql_key_destroy(dd->key, ctx);
    }
    // NOTE: sub-queries get destroyed within virtual compound dtrs
    // no need to `if(it->subQuery) hdql_query_destroy(it->subQuery, ctx);` XXX
    // More strictly: scalar interface destroy must not deallocate definition
    // data (it is logically not a runtime information managed by iface's
    // create/destroy)
    //hdql_query_destroy((struct hdql_Query*) defData, ctx);
    hdql_context_free(ctx, (hdql_Datum_t) dd_);
}

const struct hdql_ScalarAttrInterface _hdql_gScalarFwdQueryIFace = {
    .definitionData = NULL,  /* changed in copies to keep target forwarding query ptr */
    .instantiate = _fwd_query_scalar_interface_instantiate,
    .dereference = _fwd_query_scalar_interface_dereference,
    .reset = _fwd_query_scalar_interface_reset,
    .destroy = _fwd_query_scalar_interface_destroy
};

