#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query.h"

#include <assert.h>

/*
 * Sub-query as-collection interface
 *
 * Implements collection interface for forwarding query
 */

struct SubQueryIterator {
    hdql_Datum_t owner;
    hdql_Query * subQuery;
    hdql_Datum_t result;
    hdql_CollectionKey * keys;
};

static hdql_It_t
_subquery_collection_interface_create(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t definitionData
        , hdql_SelectionArgs_t selection
        , hdql_Context_t ctx
        ) {
    SubQueryIterator * it = hdql_alloc(ctx, struct SubQueryIterator);
    it->owner = ownerDatum;
    it->subQuery = reinterpret_cast<hdql_Query *>(selection);
    assert(it->subQuery);
    it->result = NULL;
    it->keys = NULL;
    int rc = hdql_query_keys_destroy(it->keys, ctx);
    assert(rc == 0);
    assert(it->keys);
    return reinterpret_cast<hdql_It_t>(it);
}

static hdql_Datum_t
_subquery_collection_interface_dereference(
          hdql_It_t it_
        , hdql_CollectionKey * key  // can be null
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    #if 1
    //SubQueryIterator * it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    const struct hdql_AttrDef * topAttrDef
        = hdql_query_top_attr(it->subQuery);
    hdql_Datum_t d = it->result;
    if(hdql_attr_def_is_direct_query(topAttrDef)) return d;
    assert(0);
    #else
    return reinterpret_cast<SubQueryIterator *>(it_)->result;
    #endif
}

static hdql_It_t
_subquery_collection_interface_advance(
          hdql_It_t it_
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    it->result = hdql_query_get(it->subQuery, it->owner, it->keys, ctx);
    return it_;
}

static hdql_It_t
_subquery_collection_interface_reset( 
          hdql_Datum_t newOwner
        , hdql_SelectionArgs_t
        , hdql_It_t it_
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    assert(it_);
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    hdql_query_reset(it->subQuery, newOwner, ctx);
    it->result = hdql_query_get(it->subQuery, newOwner, it->keys, ctx);
    return it_;
}

static void
_subquery_collection_interface_destroy(
          hdql_It_t it_
        , const hdql_Datum_t defData
        , hdql_Context_t ctx
        ) {
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    if(it->keys) {
        hdql_query_keys_destroy(it->keys, ctx);
    }
    // NOTE: sub-queries get destroyed within virtual compound dtrs
    // no need to `if(it->subQuery) hdql_query_destroy(it->subQuery, ctx);`
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(it_));
}

#if 0
static void
_subquery_collection_interface_get_key(
          hdql_Datum_t unused
        , hdql_It_t it_
        , struct hdql_CollectionKey * keyPtr
        , hdql_Context_t ctx
        ) {
    assert(it_);
    assert(keyPtr);
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    assert(it->keys);
    assert(keyPtr->code == 0x0);  // key type code for subquery
    assert(keyPtr->pl.datum);
    int rc = hdql_copy_keys( it->subQuery, keyPtr->pl.keysList, it->keys, ctx);
    assert(0 == rc);
}
#endif

const struct hdql_CollectionAttrInterface gSubQueryInterface = {
    .create = _subquery_collection_interface_create,
    .dereference = _subquery_collection_interface_dereference,
    .advance = _subquery_collection_interface_advance,
    .reset = _subquery_collection_interface_reset,
    .destroy = _subquery_collection_interface_destroy,
    .compile_selection = NULL,
    .free_selection = NULL,  // TODO: free subquery here?
};
#endif
