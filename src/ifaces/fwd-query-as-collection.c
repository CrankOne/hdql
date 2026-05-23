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

static hdql_It_t
_fwd_query_collection_new_iterator(
          hdql_Datum_t ownerDatum
        , const struct hdql_Datum *definitionData
        , hdql_Context_t ctx
        ) {
    assert(definitionData);  /* required as should bring fwd query target */
    return (hdql_It_t) definitionData;
}

static hdql_Datum_t
_fwd_query_collection_reset_iterator(
          hdql_It_t it_
        , struct hdql_Datum *newOwner
        , const struct hdql_Datum *defData
        , hdql_SelectionArgs_t sel
        , struct hdql_Key *key
        , hdql_Context_t context
        ) {
    ((void) sel);
    assert(it_);
    return hdql_query_reset((struct hdql_Query *) it_, newOwner, key, context);
}

static hdql_Datum_t
_fwd_query_collection_yield(
          hdql_It_t it_
        , const struct hdql_Datum *defData
        , struct hdql_Key *key  /* NOTE: can be null */
        , struct hdql_Context *context
        ) {
    return hdql_query_get((struct hdql_Query *) it_, key, context);
}

static void
_fwd_query_collection_interface_destroy(
          hdql_It_t it_
        , const struct hdql_Datum * defData
        , hdql_Context_t ctx
        ) {
    /* stub */
}

const struct hdql_CollectionAttrInterface _hdql_gCollectionFwdQueryIFace = {
    .definitionData = NULL,  /* changed in copies to keep target forwarding query ptr */
    .new_iterator = _fwd_query_collection_new_iterator,
    .yield = _fwd_query_collection_yield,
    .reset_iterator = _fwd_query_collection_reset_iterator,
    .destroy_iterator = _fwd_query_collection_interface_destroy,
    .compile_selection = NULL,
    .free_selection = NULL,
};
