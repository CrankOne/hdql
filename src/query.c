#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/attr-def.h"
#include "hdql/query-key.h"
#include "hdql/errors.h"

#include "hdql/internal-api.h"

#include <assert.h>

struct hdql_Query {
    /* next query in chain (as in `.current.next` expression) */
    struct hdql_Query * next;
    /* current query's result */
    hdql_Datum_t result;
    /* Gets set by `reset()` */
    hdql_Datum_t owner;
    /* This is immutable pointer to attribute's interface implementing all
     * access interfaces. Note, that subject is not owned by query or query
     * state as this instances are provided by compound definitions (and
     * deleted by them) */
    const struct hdql_AttrDef * ad;
    unsigned int flags;  /* currentYielded, ownsSubject */

    /* This is actual selection state with all data mutable during query
     * lifecycle */
    union {
        struct {
            bool isVisited;
            hdql_Datum_t dynamicSuppData;
        } scalar;
        struct {
            // selection iterator at current tier
            hdql_It_t iterator;
            // internal copy of selector expression, used to create new iterators
            hdql_SelectionArgs_t selectionArgs;
        } collection;
    } state;
};

/* reset {{{ */

/* Internal API: atomic reset of one query instance within a chain */
hdql_Datum_t
hdql__query_reset( struct hdql_Query * q
                 , hdql_Datum_t owner
                 , hdql_Key_t key
                 , hdql_Context_t context
                 ) {
    q->owner = owner;
    if(hdql_attr_def_is_collection(q->ad)) {
        const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(q->ad);
        /* create iterator if it was not previously created */
        if(NULL == q->state.collection.iterator) {
            /* create new iterator for the collection dereferencing subject
             * attribute on owning datum */
            q->state.collection.iterator = iface->create( owner /* new owner */
                    , iface->definitionData  /* static attribute's data */
                    , q->state.collection.selectionArgs  /* key filtering (selection) */
                    , context  /* context for allocations of iterator's dyn data */
                    );
            if(NULL == q->state.collection.iterator) {  /* iterator allocation failure */
                hdql_context_err_push(context, HDQL_ERR_INTERFACE_ERROR
                        , "collection interface %p could not"
                          " create iterator on object on owner %p", iface, owner );
                return NULL;
            }
        }
        /* (re)initialize iterator */
        q->state.collection.iterator = iface->reset( q->state.collection.iterator
                , owner
                , iface->definitionData
                , q->state.collection.selectionArgs
                , key
                , context
                );
        if(NULL == q->state.collection.iterator) {  /* iterator reset failure */
            hdql_context_err_push(context, HDQL_ERR_INTERFACE_ERROR
                    , "collection interface's %p reset() returned NULL"
                      " indicating unrecoverable failure for %p"
                    , iface, owner );
            return NULL;
        }
        /* note, that result can be NULL, corresponding to empty collection */
        return q->result = iface->dereference(q->state.collection.iterator);
    }
    assert(hdql_attr_def_is_scalar(q->ad));
    const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(q->ad);
    /* scalar attribute "iteration" is slightly different; instead of the
     * iterator we maintain `isVisited` flag. Still, dynamic data (cache)
     * can be defined by the scalar interface (allocated
     * with `instantiate()`) providing a place to cache access utility
     * objects between `reset()` calls. Dynamic data is optional */
    if(iface->instantiate) {
        if(NULL == q->state.scalar.dynamicSuppData ) {
            q->state.scalar.dynamicSuppData
                = iface->instantiate( owner
                    , iface->definitionData
                    , context
                    );
            if(NULL == q->state.scalar.dynamicSuppData) {
                hdql_context_err_push(context, HDQL_ERR_INTERFACE_ERROR
                    , "scalar interface %p could not"
                      " initialize access state object on owner %p", iface
                    , owner );
                return NULL;
            }
        }
        q->state.scalar.dynamicSuppData = iface->reset( owner
                    , q->state.scalar.dynamicSuppData
                    , iface->definitionData
                    , key
                    , context
                    );
    }
    /* any query advance() will raise this flag */
    q->state.scalar.isVisited = false;
    return q->result = iface->dereference( owner
            , q->state.scalar.dynamicSuppData
            , iface->definitionData
            , context
            );
}

/* exported API: chained reset of the query */
hdql_Datum_t
hdql_query_reset( struct hdql_Query * q
                , hdql_Datum_t datum
                , hdql_Key_t key
                , hdql_Context_t context
                ) {
    /* get pointer to key list begin */
    if(key) {
        assert(hdql_key_is_list(key));  /* query key is always a list */
        key = hdql__key_get_list_bgn(key);
    }
    while(q) {
        /* reset and dereference 1st in collection */
        datum = hdql__query_reset(q, datum, key, context);
        if(!datum) { return NULL; }
        /* advance key and ptr to current query */
        if(key) hdql__keys_next(key);
        q = q->next;
    }
    assert((!key) || hdql__key_is_terminal(key));  // TODO: internal api hdql__key_is_terminal(key)
    return datum;
}

/* }}} */

/* get/advance */
hdql_Datum_t
hdql__query_advance( struct hdql_Query *q
              , struct hdql_Key *key
              , hdql_Context_t context ) {
    if(hdql_attr_def_is_collection(q->ad)) {
        const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(q->ad);
        iface->advance(q->state.collection.iterator, key);
    } else {
        //const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(q->ad);
        
    }
}

/* exported API: advance and get */
hdql_Datum_t
hdql_query_get( struct hdql_Query *q
              , struct hdql_Key *key
              , hdql_Context_t context
              ) {
    /* A variant of DFS algorithm: go to the latest query in chain, try
     * to:
     *  a) advance and get element if it is a collection
     *  b) raise `isVisited` flag for scalar
     * If collection element was resolved to non-NULL from the iterator at a),
     * return the dereferenced element datum.
     *
     * Otherwise, repeat the attempt on the previous query, and then forward
     * execution to reset() to get new datum
     * */
}

static void hdql__init_query(struct hdql_Query * q, const struct hdql_AttrDef * ad
        , hdql_SelectionArgs_t selexpr) {
    q->ad = ad;
    q->owner = NULL;

    if(hdql_attr_def_is_collection(q->ad)) {
        q->state.collection.iterator = NULL;
        if(selexpr)
            q->state.collection.selectionArgs = selexpr;
        else
            q->state.collection.selectionArgs = NULL;
    } else {
        q->state.scalar.isVisited = false;
        q->state.scalar.dynamicSuppData = NULL;
        assert(!selexpr);  /* otherwise, selection expr was provided to scalar */
    }

    q->next = NULL;
    q->result = NULL;
    q->flags = 0x0;
}
