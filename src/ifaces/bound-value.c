#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/internal-ifaces.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <alloca.h>
#include <assert.h>
#include <strings.h>

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
    struct BoundValueDefinitionData * d = hdql_alloc(ctx, struct BoundValueDefinitionData);
    d->q = q;
    d->value = NULL;
    return (hdql_Datum_t) d;
}

void
hdql_bound_value_interface_definition_data_destroy(hdql_Datum_t d, hdql_Context_t ctx) {
    hdql_context_free(ctx, d);
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
    assert(*((const struct BoundValueDefinitionData *) (definitionData))->value);
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
    .instantiate = NULL,  //_bound_query_scalar_interface_instantiate,
    .dereference = _bound_query_scalar_interface_dereference,
    .reset = NULL, // _bound_query_scalar_interface_reset,
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
    const struct hdql_ValueInterface * filterVI;
    hdql_Bool_t filterLogicResult;
    hdql_TypeConverter toLogicFilterResultConverter;

    struct hdql_Query ** boundQueries;
    hdql_Datum_t * values;
    /* de-structured list of keys, a cache */
    struct hdql_CollectionKey ** keys;

    hdql_Datum_t owner;
    size_t nBindingQueries;
    hdql_Context_t context;
};

/* callback to count bound attributes, expect ud to be dest counter of size_t */
static int
_count_bound_attributes(const char *attrName
        , size_t nAttr
        , const struct hdql_AttrDef * ad
        , void * ud
) {
    size_t * count = (size_t *) ud;
    if(hdql_attr_def_is_bound(ad)) ++(*count);
    return 0;
}

/* userdata type for _bind_query() callback */
typedef struct {
    size_t nBoundAttr;
    struct QueryProdIterator * it;
} _BindQueryUD_t;

static int
_bind_query(const char *attrName, size_t nAttr, const struct hdql_AttrDef *attrAD, void * ud_) {
    _BindQueryUD_t * ud = (_BindQueryUD_t*) ud_;
    if(!hdql_attr_def_is_bound(attrAD)) return 0;  /* continue */
    const struct hdql_ScalarAttrInterface * boundAttrIFace
            = hdql_attr_def_scalar_iface(attrAD);
    assert(boundAttrIFace->dereference == _hdql_gBoundQueryIFace.dereference);
    /* ^^^ otherwise, how come it became a "bound attribute"? */
    assert(boundAttrIFace->definitionData);
    /* ^^^ otherwise, how this iface was instantiated?
     * hdql_bound_value_interface_definition_data_init() must be used */
    struct BoundValueDefinitionData * bDefData
        = (struct BoundValueDefinitionData*) boundAttrIFace->definitionData;
    ud->it->boundQueries[ud->nBoundAttr] = bDefData->q;
    #ifndef NDEBUG
    int rc =
    #endif
        hdql_query_keys_reserve( bDefData->q
                               , ud->it->keys + ud->nBoundAttr
                               , ud->it->context
                               );
    assert(0 == rc);
    bDefData->value = ud->it->values + ud->nBoundAttr;
    ++(ud->nBoundAttr);
    return 0;
}

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
    it->context = ctx;
    /* set filtering query (or NULL) */
    if(!!(it->filterQuery = dd->filterQuery)) {
        const struct hdql_AttrDef * ad = hdql_query_top_attr(it->filterQuery);
        assert(ad);
        hdql_ValueTypeCode_t valueTCode = hdql_attr_def_get_atomic_value_type_code(ad);
        it->filterVI = hdql_types_get_type( hdql_context_get_types(ctx)
                , valueTCode);
        /* allocate logic result */
        struct hdql_ValueTypes * vts = hdql_context_get_types(ctx);
        hdql_ValueTypeCode_t logicCode = hdql_types_get_type_code(vts, "hdql_Bool_t");
        assert(0x0 != logicCode);
        if(logicCode != valueTCode) {
            /* get converter */
            struct hdql_Converters * cnvs = hdql_context_get_conversions(ctx);
            it->toLogicFilterResultConverter = hdql_converters_get(cnvs, logicCode, valueTCode);
            if(NULL == it->toLogicFilterResultConverter) {
                hdql_context_err_push( ctx, HDQL_ERR_CONVERSION
                    , "Type <%s> can't be converted to boolean value (to be used"
                      " in filter expression).", it->filterVI->name );
                hdql_context_free(ctx, (hdql_Datum_t) it);
                return NULL;
            }
        } else {
            it->toLogicFilterResultConverter = NULL;
        }
    } else {
        it->filterVI = NULL;
    }

    /* populate bound queries refs for the attribute definition of
     * binding compound. Once query for binding compound gets reset
     * or advanced, the values get updated */
    it->nBindingQueries = 0;
    hdql_compound_for_each_own_attribute(dd->vCompound
            , _count_bound_attributes
            , &(it->nBindingQueries));
    assert(it->nBindingQueries > 0);  /* otherwise this compound could not be "bound" */
    /* allocate arrays for queries, value ptrs, keys */
    it->boundQueries = (struct hdql_Query **)
            hdql_context_alloc(ctx
                , it->nBindingQueries*sizeof(struct hdql_Query *));
    /* see comments to issue #14 -- we currently have to allocate and maintain
     * keys unconditionally, but that must be changed in the future */
    it->keys = (struct hdql_CollectionKey **)
            hdql_context_alloc(ctx
                , it->nBindingQueries*sizeof(struct hdql_CollectionKey *));
    it->values = (hdql_Datum_t *) hdql_context_alloc(ctx, sizeof(hdql_Datum_t *)*it->nBindingQueries);
    _BindQueryUD_t bqud = {.it = it, .nBoundAttr = 0};
    hdql_compound_for_each_own_attribute(dd->vCompound, _bind_query, &bqud);
    /* assign definition data to this transient interface */
    return (hdql_It_t) it;
}

static hdql_Datum_t
_hdql_cartesian_product_as_collection_dereference( hdql_It_t it_
                                , struct hdql_CollectionKey * keyPtr
                                ) {
    struct QueryProdIterator * it = (struct QueryProdIterator *) it_;
    if(keyPtr) {
        assert(keyPtr->isList);
        assert(keyPtr->pl.keysList[it->nBindingQueries-1].isTerminal);
        for(size_t nq = 0; nq < it->nBindingQueries; ++nq) {
            //assert(keyPtr->pl.keysList[nq].isList);
            if(!it->keys[nq]) continue;
            hdql_query_keys_copy( keyPtr->pl.keysList[nq].pl.keysList
                                , it->keys[nq]
                                , it->context
                                );
        }
    }
    #ifndef NDEBUG
    if(it->owner) {
        for(size_t i = 0; i < it->nBindingQueries; ++i) {
            assert(it->values[i]);
        }
    }
    #endif
    return it->owner;
}


static hdql_It_t
_hdql_cartesian_product_as_collection_advance( hdql_It_t it_ ) {
    struct QueryProdIterator * it = (struct QueryProdIterator *) it_;
    do {
        int rc = hdql_query_product_advance( it->boundQueries
            , it->keys
            , it->values
            , it->owner
            , it->nBindingQueries
            , it->context
            );
        if(HDQL_ERR_EMPTY_SET == rc) {
            it->owner = NULL;
            break;
        }
        #ifndef NDEBUG
        for(size_t i = 0; i < it->nBindingQueries; ++i) {
            assert(it->values[i]);
        }
        #endif
        if(it->filterQuery) {
            hdql_query_reset(it->filterQuery, it->owner, it->context);
            hdql_Datum_t r = hdql_query_get(it->filterQuery, NULL, it->context);
            if(NULL == r) continue;
            assert(r);
            if(it->toLogicFilterResultConverter) {
                int rc = it->toLogicFilterResultConverter(((hdql_Datum_t) &(it->filterLogicResult)), r);
                assert(0 == rc); /*todo: handle rc != 0*/
            } else {
                it->filterLogicResult = *((hdql_Bool_t *) r);
            }
            if(it->filterLogicResult) break;
        }
    } while(it->filterQuery);
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
    int rc = hdql_query_product_reset( it->boundQueries
        , it->keys
        , it->values
        , it->owner = newOwner
        , it->nBindingQueries
        , it->context = ctx
        );
    if(rc == HDQL_ERR_EMPTY_SET) {
        it->owner = NULL;
        return it_;
    }
    #ifndef NDEBUG
    for(size_t i = 0; i < it->nBindingQueries; ++i) {
        assert(it->values[i]);
    }
    #endif
    while(it->filterQuery && rc != HDQL_ERR_EMPTY_SET) {
        hdql_query_reset(it->filterQuery, it->owner, it->context);
        hdql_Datum_t r = hdql_query_get(it->filterQuery, NULL, it->context);
        if(NULL == r) continue;
        assert(r);
        if(it->toLogicFilterResultConverter) {
            int rc = it->toLogicFilterResultConverter(((hdql_Datum_t) &(it->filterLogicResult)), r);
            assert(0 == rc); /*todo: handle rc != 0*/
        } else {
            it->filterLogicResult = *((hdql_Bool_t *) r);
        }
        if(it->filterLogicResult) break;

        rc = hdql_query_product_advance( it->boundQueries
            , it->keys
            , it->values
            , it->owner
            , it->nBindingQueries
            , it->context
            );
        if(HDQL_ERR_EMPTY_SET == rc) {
            it->owner = NULL;
            break;
        }
    };
    return it_;
}

static void
_hdql_cartesian_product_destroy( hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    struct QueryProdIterator * it = hdql_cast(ctx, struct QueryProdIterator, it_);
    for(size_t nq = 0; nq < it->nBindingQueries; ++nq) {
        if(it->keys) {
            hdql_query_keys_destroy(it->keys[nq], ctx);
        }
        hdql_query_destroy(it->boundQueries[nq], ctx);
    }
    if(it->keys) {
        hdql_context_free(ctx, (hdql_Datum_t) it->keys);
    }
    hdql_context_free(ctx, (hdql_Datum_t) it->values);
    hdql_context_free(ctx, (hdql_Datum_t) it->boundQueries);
    hdql_context_free(ctx, (hdql_Datum_t) it_);
}


/* userdata type for _reserve_keys_list_for_binding_query() callback */
typedef struct {
    size_t nBoundAttr;
    struct hdql_CollectionKey * keysLists;
    hdql_Context_t ctx;
} _AllocKeysUD_t;

static int
_reserve_keys_list_for_binding_query(const char * attrName, size_t nAttr
        , const struct hdql_AttrDef * ad, void * ud_) {
    if(!hdql_attr_def_is_bound(ad)) return 0;  /* continue */
    _AllocKeysUD_t * ud = (_AllocKeysUD_t *) ud_;

    const struct hdql_ScalarAttrInterface * boundAttrIFace
            = hdql_attr_def_scalar_iface(ad);

    assert(boundAttrIFace->dereference == _hdql_gBoundQueryIFace.dereference);
    /* ^^^ otherwise, how come it became a "bound attribute"? */
    assert(boundAttrIFace->definitionData);
    /* ^^^ otherwise, how this iface was instantiated?
     * hdql_bound_value_interface_definition_data_init() must be used */
    struct BoundValueDefinitionData * bDefData
        = (struct BoundValueDefinitionData*) boundAttrIFace->definitionData;
    hdql_query_keys_reserve(bDefData->q, &(ud->keysLists[ud->nBoundAttr].pl.keysList), ud->ctx);
    assert(ud->keysLists[ud->nBoundAttr].pl.keysList);
    ud->keysLists[ud->nBoundAttr].isList = 0x1;
    ud->keysLists[ud->nBoundAttr].code = 0x0;
    ++(ud->nBoundAttr);
    return 0;
}

struct hdql_CollectionKey * hdql_bound_compound_key_reserve(
            const hdql_Datum_t defData_, hdql_Context_t ctx) {
    struct hdql_BindingCompoundCollectionDefData * dd
            = hdql_cast(ctx, struct hdql_BindingCompoundCollectionDefData, defData_);
    assert(dd->vCompound);
    size_t nBindingQueries = 0;
    hdql_compound_for_each_own_attribute(dd->vCompound
            , _count_bound_attributes
            , &nBindingQueries);
    assert(nBindingQueries > 0);  /* otherwise this compound could not be "bound" */
    _AllocKeysUD_t ud = {.nBoundAttr = 0, .ctx = ctx};
    /* top-level key (always a body of the list), see implementation
     * of `hdql_attr_def_reserve_keys()` in attr-def.c and issue #14 */
    ud.keysLists = (struct hdql_CollectionKey *) hdql_context_alloc(ctx
            , nBindingQueries*sizeof(struct hdql_CollectionKey));
    bzero(ud.keysLists, nBindingQueries*sizeof(struct hdql_CollectionKey));
    hdql_compound_for_each_own_attribute(dd->vCompound
            , _reserve_keys_list_for_binding_query
            , &ud );
    /* todo: this is required as external code expects us to allocate a list */
    ud.keysLists[nBindingQueries-1].isTerminal = 0x1;
    return ud.keysLists;
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
