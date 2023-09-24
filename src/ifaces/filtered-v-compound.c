#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/internal-ifaces.h"

#include <assert.h>

struct FilteredVCompoundState {
    /* Ptr to query that should control the instances */
    struct hdql_Query * filterQuery;
    const struct hdql_ValueInterface * vi;
    hdql_Bool_t logicResult;
    hdql_TypeConverter toLogicConverter;
    // ...
};

static hdql_Datum_t
_filtered_compound_scalar_interface_instantiate(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    assert(defData_);
    struct FilteredVCompoundState * state = hdql_alloc(ctx, struct FilteredVCompoundState);
    assert(state);
    state->filterQuery = (struct hdql_Query *) defData_;
    const struct hdql_AttrDef * ad = hdql_query_top_attr(state->filterQuery);
    assert(ad);
    hdql_ValueTypeCode_t valueTCode = hdql_attr_def_get_atomic_value_type_code(ad);
    state->vi = hdql_types_get_type( hdql_context_get_types(ctx)
            , valueTCode);
    /* allocate logic result */
    struct hdql_ValueTypes * vts = hdql_context_get_types(ctx);
    hdql_ValueTypeCode_t logicCode = hdql_types_get_type_code(vts, "hdql_Bool_t");
    assert(0x0 != logicCode);
    if(logicCode != valueTCode) {
        /* get converter */
        struct hdql_Converters * cnvs = hdql_context_get_conversions(ctx);
        state->toLogicConverter = hdql_converters_get(cnvs, logicCode, valueTCode);
        if(NULL == state->toLogicConverter) {
            hdql_context_err_push( ctx, HDQL_ERR_CONVERSION
                , "Type <%s> can't be converted to boolean value (to be used"
                  " in filter expression).", state->vi->name );
            hdql_context_free(ctx, (hdql_Datum_t) state);
            return NULL;
        }
    } else {
        state->toLogicConverter = NULL;
    }
    return (hdql_Datum_t) state;
}

static hdql_Datum_t
_filtered_compound_scalar_interface_dereference(
          hdql_Datum_t root
        , hdql_Datum_t s_
        , struct hdql_CollectionKey * key  // can be null
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    struct FilteredVCompoundState * s = hdql_cast(ctx, struct FilteredVCompoundState, s_);
    hdql_Datum_t r = hdql_query_get(s->filterQuery, NULL, ctx);
    if(NULL == r) return NULL;
    hdql_query_reset(s->filterQuery, root, ctx);  // TODO: check why do we need this
    assert(r);
    if(s->toLogicConverter) {
        int rc = s->toLogicConverter(((hdql_Datum_t) &(s->logicResult)), r);
        assert(0 == rc); /*todo: handle rc != 0*/
    } else {
        s->logicResult = *((hdql_Bool_t *) r);
    }
    
    if(s->logicResult) return root;
    return NULL;
}

static hdql_Datum_t
_filtered_compound_scalar_interface_reset(
          hdql_Datum_t newOwner
        , hdql_Datum_t s_
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    struct FilteredVCompoundState * s = hdql_cast(ctx, struct FilteredVCompoundState, s_);
    hdql_query_reset(s->filterQuery, newOwner, ctx);
    return s_;
}

static void
_filtered_compound_scalar_interface_destroy(
          hdql_Datum_t s_
        , const hdql_Datum_t defData_
        , hdql_Context_t ctx
        ) {
    struct FilteredVCompoundState * s = hdql_cast(ctx, struct FilteredVCompoundState, s_);
    hdql_query_destroy(s->filterQuery, ctx);
    hdql_context_free(ctx, s_);
}

const struct hdql_ScalarAttrInterface _hdql_gFilteredCompoundIFace = {
    .definitionData = NULL,
    .instantiate = _filtered_compound_scalar_interface_instantiate,
    .dereference = _filtered_compound_scalar_interface_dereference,
    .reset = _filtered_compound_scalar_interface_reset,
    .destroy = _filtered_compound_scalar_interface_destroy,
};

