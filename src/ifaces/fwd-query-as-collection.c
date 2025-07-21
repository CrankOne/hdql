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

struct FwdQueryIterator {
    struct hdql_Query * subQuery;
    hdql_Datum_t result;
    struct hdql_CollectionKey * keys;
    hdql_Context_t context;
};

static hdql_It_t
_fwd_query_collection_interface_create(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t definitionData
        , hdql_SelectionArgs_t selection
        , hdql_Context_t ctx
        ) {
    struct FwdQueryIterator * it = hdql_alloc(ctx, struct FwdQueryIterator);
    assert(NULL == selection);  /* selection is prohibited at this iface lvl */
    assert(definitionData);  /* required as should bring fwd query target */
    it->subQuery = (struct hdql_Query *) definitionData;
    assert(it->subQuery);
    it->result = NULL;
    it->keys = NULL;
    it->context = ctx;
    int rc = hdql_query_keys_reserve(it->subQuery, &(it->keys), ctx);
    assert(rc == 0);
    assert(it->keys);
    return (hdql_It_t) it;
}

static hdql_Datum_t
_fwd_query_collection_interface_dereference(
          hdql_It_t it_
        , struct hdql_CollectionKey * key  /* NOTE: can be null */
        ) {
    struct FwdQueryIterator * it = (struct FwdQueryIterator *) it_;
    if(key) {
        assert(it->keys);
        assert(key->isList);
        assert(key->code == 0x0);
        hdql_query_keys_copy(key->pl.keysList, it->keys, it->context);
    }
    return it->result;
}

static hdql_It_t
_fwd_query_collection_interface_advance(
          hdql_It_t it_
        ) {
    struct FwdQueryIterator * it = (struct FwdQueryIterator *) it_;
    if(it->result) {
        it->result = hdql_query_get(it->subQuery, it->keys, it->context);
    }
    return it_;
}

static hdql_It_t
_fwd_query_collection_interface_reset(
          hdql_It_t it_
        , hdql_Datum_t newOwner
        , const hdql_Datum_t defData
        , hdql_SelectionArgs_t
        , hdql_Context_t ctx
        ) {
    assert(it_);
    struct FwdQueryIterator * it = (struct FwdQueryIterator *) it_;
    assert(it->context == ctx);  /* fwd q had changed context between create() and reset() */
    #ifndef NDEBUG
    {
        struct hdql_Query * fwdQ = (struct hdql_Query*) defData;
        assert(fwdQ == it->subQuery);  /* fwd q had changed query between create() and reset() */
    }
    #endif
    assert(newOwner);
    hdql_query_reset(it->subQuery, newOwner, ctx);
    it->result = hdql_query_get(it->subQuery, it->keys, ctx);
    return it_;
}

static void
_fwd_query_collection_interface_destroy(
          hdql_It_t it_
        , hdql_Context_t ctx
        ) {
    struct FwdQueryIterator * it = (struct FwdQueryIterator *) it_;
    if(it->keys) {
        hdql_query_keys_destroy(it->keys, ctx);
    }
    // NOTE: sub-queries get destroyed within virtual compound dtrs
    // no need to `if(it->subQuery) hdql_query_destroy(it->subQuery, ctx);`
    // More strictly: scalar interface destroy must not deallocate definition
    // data (it is logically not a runtime information managed by iface's
    // create/destroy); iterator's reference to query is not "owned" by
    // iterator.
    //hdql_query_destroy((struct hdql_Query*) it->subQuery, ctx);  // TODO: doubtful, see scalar iface for counter-example
    hdql_context_free(ctx, (hdql_Datum_t) it_);
}

#if 0
static void
_fwd_query_collection_interface_get_key(
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
    assert(keyPtr->code == 0x0);  // key type code for fwd_query
    assert(keyPtr->pl.datum);
    int rc = hdql_copy_keys( it->subQuery, keyPtr->pl.keysList, it->keys, ctx);
    assert(0 == rc);
}
#endif

const struct hdql_CollectionAttrInterface _hdql_gCollectionFwdQueryIFace = {
    .definitionData = NULL,  /* changed in copies to keep target forwarding query ptr */
    .create = _fwd_query_collection_interface_create,
    .dereference = _fwd_query_collection_interface_dereference,
    .advance = _fwd_query_collection_interface_advance,
    .reset = _fwd_query_collection_interface_reset,
    .destroy = _fwd_query_collection_interface_destroy,
    .compile_selection = NULL,
    .free_selection = NULL,
};
