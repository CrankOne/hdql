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

static hdql_Datum_t
_fwd_query_scalar_interface_reset(
          hdql_Datum_t newOwner
        , hdql_Datum_t dd_
        , const struct hdql_Datum *defData
        , struct hdql_Key * key
        , hdql_Context_t ctx
        ) {
    return hdql_query_reset((struct hdql_Query *) defData, newOwner, key, ctx);
}

const struct hdql_ScalarAttrInterface _hdql_gScalarFwdQueryIFace = {
    .definitionData = NULL,  /* changed in copies to keep target forwarding query ptr */
    .new_dyn_data = NULL,
    .reset = _fwd_query_scalar_interface_reset,
    .destroy_dyn_data = NULL
};

