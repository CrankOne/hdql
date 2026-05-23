#include "hdql/query.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/attr-def.h"
#include "hdql/query-key.h"
#include "hdql/errors.h"

#include "hdql/internal-api.h"

#include <assert.h>

#define HDQL_QUERY_OWNS_SUBJECT 0x1

/* A chain link -- runtime state of the elements selection, a double-linked
 * list of iterators.
 *
 * Double-linked is needed because of iterative DFS. */
struct hdql_Query {
    /* next query in chain (as in `.current.next` expression) */
    struct hdql_Query * next;
    /* prev query in chain (as in `.prev.current` expression) */
    struct hdql_Query * prev;
    /* Gets set by `reset()` */
    hdql_Datum_t owner;
    /* This is immutable pointer to attribute's interface implementing all
     * access interfaces. Note, that subject is not owned by query or query
     * state as this instances are provided by compound definitions (and
     * deleted by them) */
    const struct hdql_AttrDef * ad;
    /* when HDQL_QUERY_OWNS_SUBJECT set, the AD is transient and should be
     * destroyed with query */
    unsigned int flags;  /* currentYielded, ownsSubject */

    /* This is actual selection state with all data mutable during query
     * lifecycle */
    union {
        struct {
            hdql_Datum_t dynamicSuppData;
        } scalar;
        struct {
            // selection iterator at current tier
            hdql_It_t iterator;
            // internal copy of selector expression, used to create new iterators
            hdql_SelectionArgs_t selectionArgs;
        } collection;
    } state;

    /* query label used by external API sometimes */
    char * label;
};


/* module-local API: atomic reset of one query instance within a chain */
static hdql_Datum_t
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
            q->state.collection.iterator = iface->new_iterator( owner /* new owner */
                    , iface->definitionData  /* static attribute's data */
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
        hdql_Datum_t r = iface->reset_iterator(
                  q->state.collection.iterator
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
        return r;
    }
    assert(hdql_attr_def_is_scalar(q->ad));
    const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(q->ad);
    /* scalar attribute "iteration" is slightly different; instead of the
     * iterator we only return value at reset(). Still, dynamic data (cache)
     * can be defined by the scalar interface (allocated
     * with `instantiate()`) providing a place to cache access utility
     * objects between `reset()` calls. Dynamic data is optional */
    if(iface->new_dyn_data) {
        if(NULL == q->state.scalar.dynamicSuppData ) {
            q->state.scalar.dynamicSuppData
                = iface->new_dyn_data( owner
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
    }
    /* any query advance() will raise this flag */
    return iface->reset( owner
            , q->state.scalar.dynamicSuppData
            , iface->definitionData
            , key
            , context
            );
}

static hdql_Datum_t
hdql__query_yield( struct hdql_Query *q
              , struct hdql_Key *key
              , hdql_Context_t context );  /* need by reset() */

/* module-local API: reset till descendants, not the key is list bgn */
static hdql_Datum_t
hdql__query_reset_descendant(struct hdql_Query * q
                , hdql_Datum_t datum
                , hdql_Key_t key
                , hdql_Context_t context
                ) {
    if(!q) return NULL;

    struct hdql_Query *cq = q;
    hdql_Key_t ckey = key;
    hdql_Datum_t r = datum;

    for(;;) {
        /* Descend as far as possible. */
        while(cq) {
            r = hdql__query_reset(cq, r, ckey, context);
            if(!r) break;
            if(!cq->next) return r;  /* full chain initialized */
            cq = cq->next;
            if(ckey) ckey = hdql__keys_next(ckey);
        }
        /* Reset failed at cq.  Climb to parent and try to advance it. */
        while(cq) {
            if(!cq->prev) return NULL;  /* root exhausted or initially empty */
            cq = cq->prev;
            if(ckey) ckey = hdql__keys_prev(ckey);
            r = hdql__query_yield(cq, ckey, context);
            if(r)
                break;
        }
        /* Parent yielded a new datum.  Try descendants again. */
        if(!cq->next) return r;
        cq = cq->next;
        if(ckey) ckey = hdql__keys_next(ckey);
    }
}

/* public API: chained reset of the query */
hdql_Datum_t
hdql_query_reset( struct hdql_Query * q
                , hdql_Datum_t datum
                , hdql_Key_t key
                , hdql_Context_t context
                ) {
    /* get pointer to key list begin */
    if(key) {
        assert(hdql_key_is_list(key));  /* query key must always be a list */
        key = hdql__key_get_list_bgn(key);
    }
    return hdql__query_reset_descendant(q, datum, key, context);
}


/* module-local API: tries to advance iterator
 * Advances collection iterator and returns the datum of advanced element of
 * the sequence.
 * Returns NULL when collection depleted, or when called on a scalar.
 *  */
static hdql_Datum_t
hdql__query_yield( struct hdql_Query *q
              , struct hdql_Key *key
              , hdql_Context_t context ) {
    if(hdql_attr_def_is_scalar(q->ad))
        return NULL;  /* scalars never yield, anly reset() can access its value */
    const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(q->ad);
    return iface->yield(q->state.collection.iterator, iface->definitionData, key, context);
    /* ^^^ note: yield should tolerate advancing when depletion */
}

/* exported API: advance and get */
hdql_Datum_t
hdql_query_get( struct hdql_Query *q
              , struct hdql_Key *key_
              , hdql_Context_t context
              ) {
    assert(q);
    struct hdql_Key * key = key_ ? hdql__key_get_list_bgn(key_) : NULL;
    struct hdql_Query * cq = q;
    /* Start from terminal query and terminal key. */
    while(cq->next) {
        cq = cq->next;
        if(key) key = hdql__keys_next(key);
    }
    /* ^^^ TODO: this fragment is repeated each call; consider caching it */
    for(;;) {
        hdql_Datum_t r = NULL;
        /* Backtrack until some iterator yields a value. */
        while(NULL == (r = hdql__query_yield(cq, key, context))) {
            if(NULL == cq->prev) {
                return NULL;  /* whole query chain depleted */
            }
            cq = cq->prev;
            if(key) key = hdql__keys_prev(key);
        }
        /* Descend again: reset each child iterator on the value
         * yielded by its parent. */
        while(cq->next) {
            cq = cq->next;
            if(key) key = hdql__keys_next(key);
            r = hdql__query_reset(cq, r, key, context);
            /* Empty child collection. Need to backtrack from this child on the
             * next outer iteration. */
            if(NULL == r) break;
        }
        /* Reached terminal query successfully. */
        if(NULL != r && NULL == cq->next)
            return r;
        /* Otherwise child reset failed. Continue from current cq,
         * so the next iteration will try to yield it; if depleted,
         * it will backtrack to its parent. */
    }
}

/* module-local API: intializes query data with attribute definition and
 * null-like data attributes */
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
        q->state.scalar.dynamicSuppData = NULL;
        assert(!selexpr);  /* otherwise, selection expr was provided to scalar */
    }
    q->next = NULL;
    q->prev = NULL;
    q->flags = 0x0;
    q->label = NULL;
}

/* public API */
struct hdql_Query *
hdql_query_create(
          const struct hdql_AttrDef * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t context
        ) {
    struct hdql_Query * q = (struct hdql_Query *) hdql_context_alloc(context, sizeof(struct hdql_Query));
    hdql__init_query(q, attrDef, selArgs);
    return q;
}

/* public API */
void
hdql_query_set_transient_subject_ownership(struct hdql_Query * q) {
    q->flags |= HDQL_QUERY_OWNS_SUBJECT;
}


/* private module API */
static void
hdql__query_destroy(struct hdql_Query * q, struct hdql_Context * context) {
    if(q->next)
        hdql_query_destroy(q->next, context);
    /* NOTE: we do not require here that iterator/suppData state values to be
     * created to call destroy() method of corresponding (scalar or collection)
     * interface. This is to facilitate cases when (static) definition data has
     * to be finalized in some way which is can be accomplished by iface's
     * destruction. That means, destroy() methods must consider
     * NULL state (collection iterator or scalar's suppData) as a valid
     * argument.
     * TODO: violates single responsibility principle, consider getting rid of it... */
    if(hdql_attr_def_is_collection(q->ad)) {
        const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(q->ad);
        if(q->state.collection.iterator) {
            iface->destroy_iterator( q->state.collection.iterator
                    , iface->definitionData, context );
            q->state.collection.iterator = NULL;
        }
        if(q->state.collection.selectionArgs) {
            if(!hdql__attr_def_is_fwd_query(q->ad)) {
                assert(iface->free_selection);
                iface->free_selection( iface->definitionData
                                     , q->state.collection.selectionArgs, context);
                q->state.collection.selectionArgs = NULL;
            }
        }
    } else {
        const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(q->ad);
        if(iface->destroy_dyn_data)
            iface->destroy_dyn_data( q->state.scalar.dynamicSuppData
                          , iface->definitionData
                          , context);
    }

    /* Transient attribute definitions are created dynamically by parsing
     * procedure. They are not managed as part of context definitions, so
     * they must be deleted when owning query gets destroyed. */
    if( hdql_attr_def_is_transient(q->ad) && (q->flags & HDQL_QUERY_OWNS_SUBJECT)) {
        hdql_attr_def_destroy(q->ad, context);
    }
}

/* public API */
void
hdql_query_destroy(struct hdql_Query * q, hdql_Context_t context) {
    assert(q);
    if(q->label)
        hdql_context_free(context, (hdql_Datum_t)q->label);
    hdql__query_destroy(q, context);
    hdql_context_free(context, (hdql_Datum_t)(q));
}

/* public API */
struct hdql_Query *
hdql_query_append( 
          struct hdql_Query * current
        , struct hdql_Query * next
        ) {
    struct hdql_Query * root = current;
    while(NULL != current->next) current = (struct hdql_Query*) current->next;
    current->next = next;
    next->prev = current;
    return root;
}

/* public API */
size_t
hdql_query_depth(struct hdql_Query * q) {
    size_t depth = 1;
    while(q->next) {
        q = q->next;
        ++depth;
    }
    return depth;
}

/* public API */
bool
hdql_query_is_fully_scalar(struct hdql_Query * q) {
    do {
        if(hdql_attr_def_is_collection(q->ad)) return false;
        if(hdql__attr_def_is_fwd_query(q->ad)) {
            if(!hdql_query_is_fully_scalar(hdql__attr_def_fwd_query(q->ad)))
                return false;
        }
    } while(q->next && (q = (struct hdql_Query *)(q->next)));
    return true;
}

/* public API */
struct hdql_Query *
hdql_query_next_query(struct hdql_Query * q) {
    assert(q);
    if(!q->next) return NULL;
    return (struct hdql_Query *)(q->next);
}

/* public API */
hdql_SelectionArgs_t
hdql_query_get_collection_selection(struct hdql_Query * q) {
    assert(q->ad);
    assert(hdql_attr_def_is_collection(q->ad));
    return q->state.collection.selectionArgs;
}

/* public API */
const struct hdql_AttrDef *
hdql_query_get_subject( const struct hdql_Query * q ) {
    return q->ad;
}

/* public API */
void
hdql_query_dump( FILE * outf
               , struct hdql_Query * q
               , hdql_Context_t context
               ) {
    int rc;
    char buf[1024];
    int nQ = 1;
    for(; q; q = (struct hdql_Query *)(q->next), ++nQ) {
        rc = hdql_top_attr_str(q->ad, buf, sizeof(buf), context);
        if(0 != rc) return;
        fprintf(outf, " %d) ", nQ);
        fputs(buf, outf);
        fputc('\n', outf);
    }
}

/* public API */
const struct hdql_AttrDef *
hdql_query_top_attr(const struct hdql_Query * q) {
    while(q->next) { q = q->next; }
    return hdql_attr_def_top_attr(hdql_query_get_subject(q));
}


/*
 * Query product
 */

int
hdql_query_product_reserve_key(struct hdql_Query ** qs
        , struct hdql_Key *key
        , size_t n
        , struct hdql_Context *context
        ) {
    hdql_key_mark_as_list(key, n, context);
    for(size_t i = 0; i < n; ++i) {
        int rc = hdql_key_reserve_for_query( qs[i]
                , hdql_key_get_list_item(key, i)
                , context );
        if(HDQL_ERR_CODE_OK != rc) {
            hdql_context_err_push(context, rc, "failed to reserve key for"
                    " query product itme #%zu, abrupt product key allocation", i);
            return rc;
        }
    }
    return HDQL_ERR_CODE_OK;
}

/* module API */
static int
hdql__query_product_reset( struct hdql_Query **qs
        , struct hdql_Key *locKey
        , hdql_Datum_t *v
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t context
        ) {
    /* re-set all queries in a list */
    size_t i = 0;
    const size_t iLast = n - 1;
    for(struct hdql_Query **q = qs; i < n; ++q, ++i) {
        if(!(*v = hdql_query_reset(*q, d, locKey, context)))
            return HDQL_ERR_EMPTY_SET;
        ++v;
        if(locKey && i != iLast) locKey = hdql__keys_next(locKey);
    }
    return HDQL_ERR_CODE_OK;
}

/* public API */
int
hdql_query_product_reset( struct hdql_Query ** qs
        , struct hdql_Key *key
        , hdql_Datum_t * v
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t context
        ) {
    struct hdql_Key *locKey = key ? hdql__key_get_list_bgn(key) : NULL;
    return hdql__query_product_reset(qs, locKey, v, d, n, context);
}

/* public API */
int
hdql_query_product_advance( struct hdql_Query **qs
        , struct hdql_Key *key
        , hdql_Datum_t *values
        , hdql_Datum_t d
        , size_t n
        , hdql_Context_t context
        ) {
    assert(qs);
    assert(values);
    if(!n) return HDQL_ERR_EMPTY_SET;
    size_t i = n - 1;
    struct hdql_Key *locKey = key ? hdql__key_get_list_at(key, i) : NULL;
    for(;;) {
        hdql_Datum_t r = hdql_query_get(qs[i], locKey, context);
        if(r) { /* q[i] advanced, reset all to the right: q[i+1] ... q[n-1]. */
            values[i] = r;
            if(i + 1 < n) {
                struct hdql_Key *suffixKey = locKey
                        ? hdql__keys_next(locKey)
                        : NULL;
                int rc = hdql__query_product_reset( qs + i + 1
                                                  , suffixKey
                                                  , values + i + 1
                                                  , d
                                                  , n - i - 1
                                                  , context );
                if(rc != HDQL_ERR_CODE_OK) return rc;
            }
            return HDQL_ERR_CODE_OK;
        }
        /* Current digit depleted. Move left. */
        if(0 == i) return HDQL_ERR_EMPTY_SET;
        --i;
        if(locKey) locKey = hdql__keys_prev(locKey);
    }
}

/* public API */
void
hdql_query_assign_label(struct hdql_Query * q, char * label) {
    assert(!q->label);
    q->label = label;
}

/* public API */
bool
hdql_query_is_labeled(const struct hdql_Query * q) { return q->label; }

/* public API */
const char *
hdql_query_get_label(const struct hdql_Query * q) { return q->label; }
