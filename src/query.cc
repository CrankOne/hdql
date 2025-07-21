#include <cstdio>
#include <cassert>
#include <stdexcept>

#include <cstring>

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
//#include "hdql/function.h"
#include "hdql/query.h"
#include "hdql/query-key.h"
#include "hdql/types.h"
#include "hdql/context.h"
#include "hdql/value.h"

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST
#   include <gtest/gtest.h>
#endif

namespace hdql {

// A runtime state of elementary query tier object
//
// attrDef + owner -> runtimeState
struct QueryState {
    // Subject attribute definition
    //
    // This is immutable pointer to attribute's interface implementing all
    // access interfaces. Note, that subject is not owned by query or query
    // state as this instances are provided by compound definitions (and
    // deleted by them)
    const hdql_AttrDef * subject;
    bool ownsSubject;

    // This is actual selection state with all data mutable during query
    // lifecycle
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

    // Gets set by `reset()`
    hdql_Datum_t owner;

    QueryState( const hdql_AttrDef * subject_
             , hdql_SelectionArgs_t selexpr
             );

    void finalize_tier(hdql_Context_t ctx);

    ~QueryState();
    // if state is not initialized, creates iterator and set it to first
    // available item, if possible, otherwise, sets it to end
    bool get_value( hdql_Datum_t & value
                  , hdql_CollectionKey * keyPtr
                  , hdql_Context_t ctx
                  );
    // sets iterator to next available item, if possible, or sets it to end
    void advance( hdql_Context_t ctx );
    // sets iterator to first available item
    void reset( hdql_Datum_t newOwner
              , hdql_Context_t ctx
              );
};

/**\brief Tree-item wrapper around single selection item
 *
 * \note currently implemented as linked list, in future can be extended to
 *       a tree (if we will introduce a destructuring querying syntax).
 * */
template<typename SelectionItemT>
struct Query : public SelectionItemT {
    Query * next;
    hdql_Datum_t result;

    Query( const hdql_AttrDef * subject_
         , const hdql_SelectionArgs_t selexpr_
         ) : SelectionItemT( subject_
                           , selexpr_
                           )
           , next(NULL)
           , result(NULL)
           {}

    bool get( struct hdql_CollectionKey * keys
            , hdql_Context_t ctx
            ) {
        // try to get value using current selection
        if(!SelectionItemT::get_value( result  // destination
                                     , keys
                                     , ctx
                                     ) ) {
            return false;  // not available
        }
        if(!next) {
            SelectionItemT::advance(ctx);
            return true;  // got current and this is terminal
        }
        // this is not a terminal query in the chain 
        while(!next->get(keys ? keys + 1 : NULL, ctx)) {  // try to get next and if failed, keep:
            SelectionItemT::advance(ctx);  // advance next one
            if(!SelectionItemT::get_value( result  // destination
                                         , keys
                                         , ctx
                                         ) ) {
                return false;
            }
            next->reset(result, ctx);  // reset next one
        }
        return true;
    }

    void destroy(hdql_Context_t ctx) {
        // NOTE: special case are forwarded queries for virtual compounds. We
        // do destroy them here as this is the simplest way to avoid dependency
        // calculus for virtual compound being deleted
        //if(hdql_attr_def_is_fwd_query(SelectionItemT::subject)) {
        //    hdql_query_destroy(hdql_attr_def_fwd_query(SelectionItemT::subject), ctx);
        //}
        if(next) {
            hdql_query_destroy(static_cast<hdql_Query *>(next), ctx);
        }
        SelectionItemT::finalize_tier(ctx);
    }

    void reset( hdql_Datum_t owner, hdql_Context_t ctx ) {
        SelectionItemT::reset(owner, ctx);
        if(!SelectionItemT::get_value(result, NULL, ctx))
            return;
        if(!next) return;
        next->reset(result, ctx);
    }

    std::size_t depth() const {
        if(!next) return 1;
        return next->depth() + 1;
    }
};  // struct Query<>

}  // namespace hdql

struct hdql_Query : public hdql::Query<hdql::QueryState> {
    char * label;
    hdql_Query( const hdql_AttrDef * subject_
              , const hdql_SelectionArgs_t selexpr_
              ) : hdql::Query<hdql::QueryState>( subject_
                                              , selexpr_
                                              )
                , label(nullptr)
                {}
};

namespace hdql {

QueryState::QueryState( const hdql_AttrDef * subject_
                    , hdql_SelectionArgs_t selexpr
                    ) : subject(subject_)
                      , ownsSubject(false)
                      , owner(nullptr)
                      {
    if(hdql_attr_def_is_collection(subject)) {
        state.collection.iterator = NULL;
        if(selexpr)
            state.collection.selectionArgs = selexpr;
        else
            state.collection.selectionArgs = NULL;
    } else {
        state.scalar.isVisited = false;
        state.scalar.dynamicSuppData = NULL;
        assert(!selexpr);
    }
}

void
QueryState::finalize_tier(hdql_Context_t ctx) {
    // NOTE: we do not require here that iterator/suppData state values to be
    // created to call destroy() method of corresponding (scalar or collection)
    // interface. This is to facilitate cases when definition data has to be
    // finalized in some way... That means, destroy() methods must consider
    // NULL state (collection iterator or scalar's suppData) as a valid
    // argument.
    if(hdql_attr_def_is_collection(subject)) {
        const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        if(state.collection.iterator) {
            iface->destroy( state.collection.iterator, ctx );
            state.collection.iterator = nullptr;
        }
        if(state.collection.selectionArgs) {
            if(hdql_attr_def_is_direct_query(subject)) {
                assert(iface->free_selection);
                iface->free_selection( iface->definitionData
                                     , state.collection.selectionArgs, ctx);
                state.collection.selectionArgs = nullptr;
            }
        }
        // check, if forwarding query. It was created during interpretation
        // stage and is not managed by iface although brought by it as supp
        // data.
        //if(hdql_attr_def_is_fwd_query(subject)
        //&& !hdql_attr_def_is_transient(subject)) {
        //    hdql_query_destroy((hdql_Query *) iface->definitionData, ctx);
        //}
    } else {
        const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
        if(iface->destroy)
            iface->destroy( state.scalar.dynamicSuppData
                          , iface->definitionData
                          , ctx);
        //if( hdql_attr_def_is_fwd_query(subject)
        // && !hdql_attr_def_is_transient(subject)
        // ) {
        //    hdql_query_destroy((hdql_Query *) iface->definitionData, ctx);
        //}
    }

    // Transient attribute definitions are created dynamically by parsing
    // procedure. They are not managed as part of context definitions, so
    // they must be deleted when owning query gets destroyed.
    if( hdql_attr_def_is_transient(subject) && ownsSubject) {
        hdql_attr_def_destroy(subject, ctx);
        //hdql_context_free(ctx, (hdql_Datum_t) subject);
    }
}

QueryState::~QueryState() {
}

// if state is not initialized, creates iterator and set it to first
// available item, if possible, otherwise, sets it to end
bool
QueryState::get_value( hdql_Datum_t & value
              , hdql_CollectionKey * keyPtr
              , hdql_Context_t ctx
              ) {
    if(hdql_attr_def_is_static_value(subject)) {
        owner = hdql_attr_def_get_static_value(subject);
    }
    assert(owner);  // otherwise missing `reset()`
    if(hdql_attr_def_is_collection(subject)) {
        const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        assert(state.collection.iterator);
        value = iface->dereference(state.collection.iterator, keyPtr);
    } else {
        if(state.scalar.isVisited)
            return false;
        const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
        assert(iface->dereference);
        value = iface->dereference( owner
                , state.scalar.dynamicSuppData
                , keyPtr
                , iface->definitionData
                , ctx
                );
    }
    return NULL != value;
}

// sets iterator to next available item, if possible, or sets it to end
void
QueryState::advance( hdql_Context_t ctx ) {
    assert(subject);
    assert(owner);  // otherwise missing reset()
    if(hdql_attr_def_is_collection(subject)) {
        if(!state.collection.iterator)
            return;  // permitted state for empty collections
        const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        state.collection.iterator = iface->advance( state.collection.iterator );
    } else {
        state.scalar.isVisited = true;
    }
}

void
QueryState::reset( hdql_Datum_t newOwner
                , hdql_Context_t ctx
                ) {
    assert(newOwner);
    if(hdql_attr_def_is_collection(subject)) {
        const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        if(NULL == state.collection.iterator) {
            assert(iface->create);
            state.collection.iterator =
                iface->create( newOwner
                    , iface->definitionData
                    , state.collection.selectionArgs
                    , ctx
                    );
        }   
        state.collection.iterator =
                iface->reset(state.collection.iterator
                    , newOwner
                    , iface->definitionData
                    , state.collection.selectionArgs
                    , ctx
                    );
    } else {
        const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
        if(iface->instantiate) {
            if(NULL == state.scalar.dynamicSuppData ) {
                state.scalar.dynamicSuppData
                    = iface->instantiate( newOwner
                        , iface->definitionData
                        , ctx
                        );
                if(NULL == state.scalar.dynamicSuppData) {
                    throw std::runtime_error("Failed to instantiate"
                            " access state object for scalar attribute");
                }
            }
            assert(iface->reset);  // provided `instantiate()`, but no `reset()`?
            state.scalar.dynamicSuppData = iface->reset( newOwner
                        , state.scalar.dynamicSuppData
                        , iface->definitionData
                        , ctx
                        );
        }
        state.scalar.isVisited = false;
    }
    owner = newOwner;
}

}  // namespace hdql


extern "C" hdql_Query *
hdql_query_create(
          const struct hdql_AttrDef * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t ctx
        ) {
    hdql_Datum_t bf = hdql_context_alloc(ctx, sizeof(hdql_Query));
    return new (bf) hdql_Query( attrDef, selArgs );
}

extern "C" hdql_Datum_t
hdql_query_get( struct hdql_Query * query
              , struct hdql_CollectionKey * keys
              , hdql_Context_t ctx
              ) {
    // evaluate query on data, return NULL if evaluation failed
    // Query::get() changes states of query chain and returns `false' if
    // full chain can not be evaluated
    if(!query->get(keys, ctx)) return NULL;
    // for successfully evaluated chain, dereference results to topmost query
    // and return its "current" value as a query result
    struct hdql_Query * current = query;
    while(NULL != current->next) current = static_cast<hdql_Query*>(current->next);
    return current->result;
}

extern "C" int
hdql_query_reset( struct hdql_Query * query
                , hdql_Datum_t owner
                , hdql_Context_t ctx
                ) {
    try {
        query->reset(owner, ctx);
    } catch(std::runtime_error & e) {
        hdql_context_err_push(ctx, HDQL_ERR_BAD_QUERY_STATE
                , "Can't re-set the query: %s", e.what());
        return HDQL_ERR_BAD_QUERY_STATE;
    }
    return HDQL_ERR_CODE_OK;
}

extern "C" void
hdql_query_set_transient_subject_ownership(struct hdql_Query * q) {
    q->ownsSubject = true;
}

extern "C" void
hdql_query_destroy(struct hdql_Query * q, hdql_Context_t ctx) {
    assert(q);
    if(q->label)
        hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(q->label));
    q->destroy(ctx);
    q->~hdql_Query();
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(q));
}

extern "C" hdql_Query *
hdql_query_append( 
          struct hdql_Query * current
        , struct hdql_Query * next
        ) {
    struct hdql_Query * root = current;
    while(NULL != current->next) current = static_cast<hdql_Query*>(current->next);
    current->next = next;
    return root;
}

extern "C" size_t
hdql_query_depth(struct hdql_Query * q) {
    assert(q);
    return q->depth();
}

extern "C" bool
hdql_query_is_fully_scalar(struct hdql_Query * q) {
    do {
        if(hdql_attr_def_is_collection(q->subject)) return false;
        if(hdql_attr_def_is_fwd_query(q->subject)) {
            if(!hdql_query_is_fully_scalar(hdql_attr_def_fwd_query(q->subject)))
                return false;
        }
    } while(q->next && (q = static_cast<hdql_Query *>(q->next)));
    return true;
}

extern "C" struct hdql_Query *
hdql_query_next_query(struct hdql_Query * q) {
    assert(q);
    if(!q->next) return NULL;
    return static_cast<hdql_Query *>(q->next);
}

extern "C" hdql_SelectionArgs_t
hdql_query_get_collection_selection(struct hdql_Query * q) {
    assert(q->subject);
    assert(hdql_attr_def_is_collection(q->subject));
    return q->state.collection.selectionArgs;
}

extern "C" void
hdql_query_assign_label(struct hdql_Query * q, char * l) {
    assert(q);
    q->label = l;
}

extern "C" bool
hdql_query_is_labeled(const struct hdql_Query * q) {
    return q->label;
}

extern "C" const char *
hdql_query_get_label(const struct hdql_Query * q) {
    return q->label;
}


extern "C" const struct hdql_AttrDef *
hdql_query_get_subject( const struct hdql_Query * q ) {
    return q->subject;
}

extern "C" void
hdql_query_dump( FILE * outf
               , struct hdql_Query * q
               , hdql_Context_t context
               ) {
    int rc;
    char buf[1024];
    for(; q; q = static_cast<hdql_Query *>(q->next)) {
        rc = hdql_top_attr_str(q->subject, buf, sizeof(buf), context);
        if(0 != rc) return;
        fputs(" * ", outf);
        fputs(buf, outf);
        fputc('\n', outf);
    }
}

extern "C" const hdql_AttrDef *
hdql_query_top_attr(const struct hdql_Query * q_) {
    const hdql::Query<hdql::QueryState> * q = q_;
    while(q->next) { q = q->next; }
    #if 0
    if(hdql_attr_def_is_fwd_query(q->subject)) {
        return hdql_query_top_attr(hdql_attr_def_fwd_query(q->subject));
    }
    return q->subject;
    #else
    return hdql_attr_def_top_attr(hdql_query_get_subject(static_cast<const struct hdql_Query *>(q)));
    #endif
}


