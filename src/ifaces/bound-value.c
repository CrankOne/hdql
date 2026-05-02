#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/internal-ifaces.h"
#include "hdql/types.h"

#include <alloca.h>
#include <assert.h>

/* Bound value implements forwarded access interface over the object managed
 * externally. Perculiar feature is that these iface has two stages in its
 * lifecycle.
 *
 * - pointer to a query is stored in definition data, but it is not controlled
 *   by this interface. It is stored *only* to be picked up at the end of
 *   encompassing virtual compound creation (TODO: but gets deleted here?)
 * - refers to a collection (sub-queries resulting in non-scalar item);
 * - represents themself as a scalar during query state evaluation at own
 *   level;
 * - yet, the state is controlled by some other query implementing collection
 *   interface.
 *
 * Example (lexic can differ):
 *      .root.{one := .a, two := *.b.c}
 *                               ^^^ this
 * here `two' is the bound query and the encompassing `{}` virtual compound
 * is a collection, steering iteration over bound query.
 */

/* NOTE: this is definition data, NOT AN ITERATOR! */
struct BoundValueDefinitionData {
    /* query that should provide the value. Set upon construction */
    struct hdql_Query * q;
    /* value pointer */
    hdql_Datum_t * value;
    /* ... key?*/
};

hdql_Datum_t
hdql_bound_value_interface_definition_data_init(struct hdql_Query * q, hdql_Context_t ctx) {
    struct BoundValueDefinitionData * d = (struct BoundValueDefinitionData *)
            hdql_context_alloc(ctx, sizeof(struct BoundValueDefinitionData));
    d->q = q;
    d->value = NULL;
    return (hdql_Datum_t) d;
}

void
hdql_bound_value_interface_definition_data_destroy(hdql_Datum_t d, hdql_Context_t ctx) {
    hdql_context_free(ctx, d);
}

/* Query is provided as `definitionData` */
static hdql_Datum_t
_bound_query_scalar_interface_instantiate(
          hdql_Datum_t ownerDatum
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    assert(definitionData);  /* otherwise, bound to what query/value? */
    assert(((const struct BoundValueDefinitionData *) (definitionData))->value);
    return *((const struct BoundValueDefinitionData *) (definitionData))->value;
}

static hdql_Datum_t
_bound_query_scalar_interface_dereference(
          hdql_Datum_t root
        , hdql_Datum_t dd_
        , struct hdql_CollectionKey * key  // can be null
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    assert(definitionData);  /* otherwise, bound to what query/value? */
    return *((const struct BoundValueDefinitionData *) (definitionData))->value;
}

static hdql_Datum_t
_bound_query_scalar_interface_reset(
          hdql_Datum_t newOwner
        , hdql_Datum_t dd_
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    assert(definitionData);  /* otherwise, bound to what query/value? */
    return *((const struct BoundValueDefinitionData *) (definitionData))->value;
}

static void
_bound_query_scalar_interface_destroy(
          hdql_Datum_t dd_
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    /* do nothing */
}

const struct hdql_ScalarAttrInterface _hdql_gBoundQueryIFace = {
    .definitionData = NULL,  /* : changed in copies keep target forwarding query ptr */
    .instantiate = _bound_query_scalar_interface_instantiate,
    .dereference = _bound_query_scalar_interface_dereference,
    .reset = _bound_query_scalar_interface_reset,
    .destroy = _bound_query_scalar_interface_destroy
};

/* Bound compound collection (possibly filtered) manages Cartesian product
 * internally to iterate over set of (binding) queries. 
 *
 * Upon creation the iterator gets initialized with compound containing (up to
 * this point incomplete) bound attributes that are used to assemble data
 * necessary for a Cartesian product.
 * */

struct QueryProdIterator {
    struct hdql_Query * filterQuery;

    struct hdql_Query ** boundQueries;
    hdql_Datum_t * values;
    struct hdql_CollectionKey ** keys;

    hdql_Datum_t owner;
    size_t nBindingQueries;
    hdql_Context_t context;
};

static hdql_It_t
_hdql_cartesian_product_as_collection_create( hdql_Datum_t owner
                           , const hdql_Datum_t defData_
                           , hdql_SelectionArgs_t selArgs_
                           , hdql_Context_t ctx
                           ) {
    struct hdql_BindingCompoundCollectionDefData * dd
            = hdql_cast(ctx, struct hdql_BindingCompoundCollectionDefData, defData_);
    /* allocate definition data for transient interface */
    struct QueryProdIterator * it = hdql_alloc(ctx, struct QueryProdIterator);

    /*
     * Init value interface and filtering
     */



    /*
     * init Cartesian product
     */

    /* set filtering query (or NULL) */
    it->filterQuery = dd->filterQuery;
    /* populate bound queries refs for the attribute definition of
     * binding compound. Once query for binding compound gets reset
     * or advanced, the values get updated */
    it->nBindingQueries = 0;
    const size_t nAttrsOverall = hdql_compound_get_nattrs(dd->vCompound);
    const char ** names = alloca(sizeof(char*)*nAttrsOverall);
    hdql_compound_get_attr_names(dd->vCompound, names);
    for(size_t nAttr = 0; nAttr < nAttrsOverall; ++nAttr) {
        const struct hdql_AttrDef * attrAD = hdql_compound_get_attr(dd->vCompound, names[nAttr]);
        if(hdql_attr_def_is_bound(attrAD)) ++(it->nBindingQueries);
    }
    assert(it->nBindingQueries > 0);  /* otherwise this compound could not be "bound" */

    /* allocate arrays for queries, value ptrs, keys */
    it->boundQueries = (struct hdql_Query **)
            hdql_context_alloc(ctx
                , it->nBindingQueries * sizeof(struct hdql_Query *));
    it->values = (hdql_Datum_t *) hdql_context_alloc(ctx, sizeof(hdql_Datum_t *)*it->nBindingQueries);

    size_t nBoundAttr = 0;
    for(size_t nAttr = 0; nAttr < nAttrsOverall; ++nAttr) {
        const struct hdql_AttrDef * attrAD = hdql_compound_get_attr(dd->vCompound, names[nAttr]);
        if(!hdql_attr_def_is_bound(attrAD)) continue;
        const struct hdql_ScalarAttrInterface * boundAttrIFace
                = hdql_attr_def_scalar_iface(attrAD);
        assert(boundAttrIFace->dereference == _hdql_gBoundQueryIFace.dereference);
        /* ^^^ otherwise, how come it became a "bound attribute"? */
        assert(boundAttrIFace->definitionData);
        /* ^^^ otherwise, how this iface was instantiated?
         * hdql_bound_value_interface_definition_data_init() must be used */
        struct BoundValueDefinitionData * bDefData
            = (struct BoundValueDefinitionData*) boundAttrIFace->definitionData;
        it->boundQueries[nBoundAttr] = bDefData->q;
        bDefData->value = it->values + nBoundAttr;
        ++nBoundAttr;
    }
    printf( "XXX Cartesian product initialized to handle %zu bound attributes\n"
          , it->nBindingQueries );
    /* assign definition data to this transient interface */
    return (hdql_It_t) it;
}

static hdql_Datum_t
_hdql_cartesian_product_as_collection_dereference( hdql_It_t it_
                                , struct hdql_CollectionKey * keyPtr
                                ) {
    struct QueryProdIterator * it = (struct QueryProdIterator *) it_;
    return it->owner;
}

static hdql_It_t
_hdql_cartesian_product_as_collection_advance( hdql_It_t it_ ) {
    struct QueryProdIterator * it = (struct QueryProdIterator *) it_;
    int rc = hdql_query_product_advance( it->boundQueries
        , it->keys
        , it->values
        , it->owner
        , it->nBindingQueries
        , it->context
        );
    if(HDQL_ERR_CODE_OK != rc)
        it->owner = NULL;
    return it_;
}

static hdql_It_t
_hdql_cartesian_product_as_collection_reset( hdql_It_t it_
                          , hdql_Datum_t newOwner
                          , const hdql_Datum_t defData
                          , hdql_SelectionArgs_t selArgs
                          , hdql_Context_t ctx
                          ) {
    assert(it_);
    struct QueryProdIterator * it = (struct QueryProdIterator *) it_;
    #ifndef NDEBUG
    int rc =
    #endif
        hdql_query_product_reset( it->boundQueries
        , it->keys
        , it->values
        , it->owner = newOwner
        , it->nBindingQueries
        , it->context = ctx
        );
    assert(0 == rc);  /* TODO we have no means to forward errors here */
    return it_;
}

static void
_hdql_cartesian_product_destroy( hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    // ...
}


const struct hdql_CollectionAttrInterface _hdql_gBindingCompoundCollectionIFace = {
    .definitionData = NULL,  /* set by caller to the instance of `hdql_BoundCompoundDefinitionData` struct */
    .create = _hdql_cartesian_product_as_collection_create,
    .dereference =_hdql_cartesian_product_as_collection_dereference,
    .advance = _hdql_cartesian_product_as_collection_advance,
    .reset = _hdql_cartesian_product_as_collection_reset,
    .destroy = _hdql_cartesian_product_destroy,
    .compile_selection = NULL,  /* TODO? */
    .free_selection = NULL  /*TODO?*/
};
