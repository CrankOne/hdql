#include <cstdio>
#include <unordered_map>
#include <string>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <iostream>

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

// Elementary query tier object
struct Selection {
    // Subject attribute definition
    //
    // This is immutable pointer to attribute's interface implementing all
    // access interfaces
    const hdql_AttrDef * subject;

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
    hdql_Datum_t owner;

    Selection( const hdql_AttrDef * subject_
             , hdql_SelectionArgs_t selexpr
             );

    void finalize_tier(hdql_Context_t ctx);

    ~Selection();
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
    hdql_Datum_t currentAttr;

    Query( const hdql_AttrDef * subject_
         , const hdql_SelectionArgs_t selexpr_
         ) : SelectionItemT( subject_
                           , selexpr_
                           )
           , next(NULL)
           , currentAttr(NULL) {}

    bool get( hdql_Datum_t ownerDatum
            , struct hdql_CollectionKey * keys
            , hdql_Context_t ctx
            ) {
        // try to get value using current selection
        if(!SelectionItemT::get_value( currentAttr  // destination
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
        while(!next->get(currentAttr, keys ? keys + 1 : NULL, ctx)) {  // try to get next and if failed, keep:
            SelectionItemT::advance(ctx);  // advance next one
            if(!SelectionItemT::get_value( currentAttr  // destination
                                         , keys
                                         , ctx
                                         ) ) {
                return false;
            }
            next->reset(currentAttr, ctx);  // reset next one
        }
        return true;
    }

    void finalize(hdql_Context_t ctx) {
        if(next) next->finalize(ctx);
        SelectionItemT::finalize_tier(ctx);
        if(next) delete next;
    }

    void reset( hdql_Datum_t owner, hdql_Context_t ctx ) {
        SelectionItemT::reset(owner, ctx);
        if(!SelectionItemT::get_value(currentAttr, NULL, ctx))
            return;
        if(!next) return;
        next->reset(currentAttr, ctx);
    }

    std::size_t depth() const {
        if(!next) return 1;
        return next->depth() + 1;
    }
};

}  // namespace hdql

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST  // tests for Query<> {{{
struct TestingItem {
    // Current state and maximum permitted
    int c, max_;
    // Label, used in debugging
    int label;
    // internal copy of the value in use; differs from `currentAttr' of
    // encompassing `Query<>` only for terminative item. Practically, should
    // correspond to actual entry in the collection or attribute in compound
    // item
    int valueCopy;
    
    TestingItem( const hdql_AttrDef * subject_
               , const hdql_SelectionArgs_t selexpr_ )
        : c(0), max_(0)
    {}

    bool get_value( hdql_Datum_t & value
                  , hdql_CollectionKey * keyPtr
                  , hdql_Context_t ctx
                  ) {
        assert(c <= max_);
        if(c == max_) return false;
        valueCopy = c;
        value = reinterpret_cast<hdql_Datum_t>(&valueCopy);
        return true;
    }

    void advance( hdql_Context_t ctx ) {
        assert(c < max_);
        ++c;
    }

    void reset( hdql_Datum_t newOwner
              , hdql_Context_t ctx
              ) {
        c = 0;
    }
};

static const struct {
    int vs[4];
} gTestExpectedOutput[] = {
    {0, 0, 0, 0},
    {0, 0, 0, 1},
    {0, 1, 0, 0},
    {0, 1, 0, 1},
    {0, 2, 0, 0},
    {0, 2, 0, 1},
    {1, 0, 0, 0},
    {1, 0, 0, 1},
    {1, 1, 0, 0},
    {1, 1, 0, 1},
    {1, 2, 0, 0},
    {1, 2, 0, 1},
};

TEST(BasicQueryIteration, ChainedSelectionIterationWorks) {
    hdql::Query<TestingItem>
        q1(NULL, NULL), q2(NULL, NULL), q3(NULL, NULL), q4(NULL, NULL);
    q1.next = &q2;  q1.max_ = 2;
    q2.next = &q3;  q2.max_ = 3;
    q3.next = &q4;  q3.max_ = 1;
    q4.next = NULL; q4.max_ = 2;
    q1.label = 1;
    q2.label = 2;
    q3.label = 3;
    q4.label = 4;
    hdql_Datum_t data = reinterpret_cast<hdql_Datum_t>(0xff);
    size_t n = 0;
    ASSERT_EQ(q1.depth(), 4);
    while(q1.get(data, NULL, NULL)) {
        ASSERT_LT(n, sizeof(gTestExpectedOutput)/sizeof(*gTestExpectedOutput));
        size_t nn = 0;
        for(hdql::Query<TestingItem> * q = &q1; q; q = q->next, ++nn) {
            int value = *reinterpret_cast<int*>(q->currentAttr);
           // std::cout << " " << value;
            ASSERT_LT(nn, 4);
            EXPECT_EQ(value, gTestExpectedOutput[n].vs[nn]);
        }
        //std::cout << std::endl;
        ++n;
    }
}
#endif  /// }}}

struct hdql_Query : public hdql::Query<hdql::Selection> {
    hdql_Query( const hdql_AttrDef * subject_
              , const hdql_SelectionArgs_t selexpr_
              ) : hdql::Query<hdql::Selection>( subject_
                                              , selexpr_
                                              ) {}
    #if 0
    bool is_functionlike() const {
        // operation or function node:
        return subject->isCollection  // ... is collection
            && 0x0 == subject->interface.collection.keyTypeCode  // ... provides no key type by itself
            && subject->interface.collection.get_key  // ... can provide keys
            && state.iterable.selectionArgs  // ...has its "selection args" are set (function instance)
            && NULL == subject->interface.collection.compile_selection  //...and has no way to create selection args 
            ;
    }
    #endif
};

namespace hdql {

Selection::Selection( const hdql_AttrDef * subject_
                    , hdql_SelectionArgs_t selexpr
                    ) : subject(subject_)
                      {
    //if(subject->isSubQuery) {
    //    bzero(&state, sizeof(state));
    //} else
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

    if(hdql_attr_def_is_fwd_query(subject)) {
        assert(NULL == state.collection.selectionArgs);
        assert(0);  // TODO
        //state.collection.selectionArgs = reinterpret_cast<hdql_SelectionArgs_t>(subject->typeInfo.subQuery);
    }
}

void
Selection::finalize_tier(hdql_Context_t ctx) {
    if(hdql_attr_def_is_collection(subject)) {
        const struct hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        if(state.collection.iterator) {
            iface->destroy( state.collection.iterator, ctx );
        }
        if(state.collection.selectionArgs) {
            if(hdql_attr_def_is_direct_query(subject)) {
                assert(iface->free_selection);
                iface->free_selection( iface->definitionData
                                     , state.collection.selectionArgs, ctx);
            }
        }
    } else {
        const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
        if(iface->destroy)
            iface->destroy( state.scalar.dynamicSuppData
                          , iface->definitionData
                          , ctx);
    }

    if(hdql_attr_def_is_static_value(subject)) {
        //hdql_context_local_attribute_destroy(ctx, );
        //attrDef->typeInfo.staticValue.datum = hdql_context_alloc(ws->context, sizeof(hdql_Int_t));
        assert(0);  // TODO
        //if(subject->typeInfo.staticValue.datum) {
        //  hdql_context_free(ctx, subject->typeInfo.staticValue.datum);
        //}
    }
}

Selection::~Selection() {
}

// if state is not initialized, creates iterator and set it to first
// available item, if possible, otherwise, sets it to end
bool
Selection::get_value( hdql_Datum_t & value
              , hdql_CollectionKey * keyPtr
              , hdql_Context_t ctx
              ) {
    if(hdql_attr_def_is_static_value(subject)) {
        owner = hdql_attr_def_get_static_value(subject);
    }
    assert(owner);  // otherwise missing `reset()`
    //if(subject->isSubQuery) {
    //    value = hdql_query_get(subject->typeInfo.subQuery, owner
    //            , keyPtr ? reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum) : NULL
    //            , ctx
    //            );
    //    return NULL != value;
    //}
    if(hdql_attr_def_is_collection(subject)) {
        #if 0
        if(!state.collection.iterator) {
            // not initialized/empty -- init, reset
            state.collection.iterator =
                subject->interface.collection.create( owner
                        , subject->interface.collection.definitionData
                        , state.collection.selectionArgs
                        , ctx
                        );
            if(!state.collection.iterator)
                return false;
            subject->interface.collection.reset( owner
                        , state.collection.selectionArgs
                        , state.collection.iterator
                        , subject->interface.collection.definitionData
                        , ctx
                        );
        }
        #endif
        const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        assert(state.collection.iterator);
        value = iface->dereference(state.collection.iterator, keyPtr);
    } else {
        if(state.scalar.isVisited)
            return false;
        const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
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
Selection::advance( hdql_Context_t ctx ) {
    assert(subject);
    assert(owner);  // otherwise missing reset()
    //if(subject->isSubQuery) {
    //    // NOTE: advance for sub-queries has no sense as it was already 
    //} else
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
Selection::reset( hdql_Datum_t newOwner
                , hdql_Context_t ctx
                ) {
    if(hdql_attr_def_is_fwd_query(subject)) {
        hdql_query_reset( hdql_attr_def_fwd_query(subject)
                , newOwner, ctx);
    }
    if(hdql_attr_def_is_collection(subject)) {
        const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(subject);
        if(NULL != state.collection.iterator) {
            state.collection.iterator =
                iface->reset(state.collection.iterator
                    , newOwner
                    , iface->definitionData
                    , state.collection.selectionArgs
                    , ctx
                    );
        } else {
            state.collection.iterator =
                iface->create(newOwner
                    , iface->definitionData
                    , state.collection.selectionArgs
                    , ctx
                    );
        }
    } else {
        const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(subject);
        if(iface->instantiate) {
            if(NULL != state.scalar.dynamicSuppData ) {
                assert(iface->reset);  // provided `instantiate()`, but no `reset()`?
                state.scalar.dynamicSuppData = iface->reset(newOwner
                        , state.scalar.dynamicSuppData
                        , iface->definitionData
                        , ctx
                        );
            } else {
                state.scalar.dynamicSuppData
                    = iface->instantiate( newOwner
                        , iface->definitionData
                        , ctx
                        );
            }
        }
        state.scalar.isVisited = false;
    }
}

}  // namespace hdql


extern "C" hdql_Query *
hdql_query_create(
          const struct hdql_AttrDef * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t ctx
        ) {
    return new hdql_Query( attrDef, selArgs );
}

extern "C" const struct hdql_AttrDef *
hdql_query_get_subject( struct hdql_Query * q ) {
    return q->subject;
}

extern "C" int
hdql_query_str( const struct hdql_Query * q
              , char * strbuf, size_t buflen
              , hdql_ValueTypes * vts
              ) {
    if(NULL == q || 0 == buflen || NULL == strbuf)
        return HDQL_ERR_BAD_ARGUMENT;
    size_t nUsed = 0;
    #define _M_pr(fmt, ...) \
        nUsed += snprintf(strbuf + nUsed, buflen - nUsed, fmt, __VA_ARGS__); \
        if(nUsed >= buflen - 1) return 1;
    // query 0x23fff34 is "[static ](collection|scalar) [of [atomic|compound]type] "
    _M_pr("%s%s%s "
            , hdql_attr_def_is_static_value(q->subject) ? "static " : ""
            , hdql_attr_def_is_collection(q->subject) ? "collection" : "scalar"
            , hdql_attr_def_is_fwd_query(q->subject)
              ? " forwarding to "
              : ( hdql_attr_def_is_atomic(q->subject)
                ? "of atomic type"
                : ( hdql_compound_is_virtual(hdql_attr_def_compound_type_info(q->subject))
                  ? "of virtual compound type"
                  : "of compound type"
                  )
                )
            );
    if(hdql_attr_def_is_fwd_query(q->subject)) {
        // query 0x23fff34 is "[static ](collection|scalar) forwarding to %p which is ..."
        _M_pr("%p which is ", hdql_attr_def_fwd_query(q->subject) );
        return hdql_query_str( hdql_attr_def_fwd_query(q->subject)
                , strbuf + nUsed, buflen - nUsed, vts );
    }
    if(hdql_attr_def_is_atomic(q->subject)) {
        // "[static ](collection|scalar) of atomic type <%s>"
        assert(vts);
        const hdql_ValueInterface * vi
            = hdql_types_get_type( vts
                                 , hdql_attr_def_get_atomic_value_type_code(q->subject)
                                 );
        assert(NULL != vi);
        _M_pr("<%s>", vi->name);
        if(hdql_attr_def_is_static_value(q->subject)) {
            assert(vi);
            // "[static ](collection|scalar) of atomic type <%s> [=%s|at %p]"
            if(vi->get_as_string) {
                char vBf[64];
                int rc = vi->get_as_string(hdql_attr_def_get_static_value(q->subject)
                            , vBf, sizeof(vBf));
                if(0 == rc) {
                    _M_pr(" =%s", vBf);
                } else {
                    _M_pr(" =? at %p", hdql_attr_def_get_static_value(q->subject));
                }
            } else {
                _M_pr(" at %p", hdql_attr_def_get_static_value(q->subject));
            }
        }
    } else {
        // query 0x23fff34 is "[static ](collection|scalar) of [virtual] compound type [based on] <%s>"
        _M_pr("%s<%s>"
                , hdql_compound_is_virtual(hdql_attr_def_compound_type_info(q->subject))
                ? "based on "
                : ""
                , hdql_compound_get_name(hdql_attr_def_compound_type_info(q->subject)));
    }
    #undef _M_pr
    return 0;
}

extern "C" hdql_Datum_t
hdql_query_get( struct hdql_Query * query
              , hdql_Datum_t root
              , struct hdql_CollectionKey * keys
              , hdql_Context_t ctx
              ) {
    query->currentAttr = root;
    // evaluate query on data, return NULL if evaluation failed
    // Query::get() changes states of query chain and returns `false' if
    // full chain can not be evaluated
    if(!query->get(root, keys, ctx)) return NULL;
    // for successfully evaluated chain, dereference results to topmost query
    // and return its "current" value as a query result
    struct hdql_Query * current = query;
    while(NULL != current->next) current = static_cast<hdql_Query*>(current->next);
    return current->currentAttr;
}

extern "C" void
hdql_query_reset( struct hdql_Query * query
                , hdql_Datum_t owner
                , hdql_Context_t ctx
                ) {
    query->reset(owner, ctx);
}

extern "C" void
hdql_query_destroy(struct hdql_Query * q, hdql_Context_t ctx) {
    assert(q);
    q->finalize(ctx);
    delete q;
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
    return static_cast<hdql_Query *>(q->next);
}

extern "C" hdql_SelectionArgs_t
hdql_query_get_collection_selection(struct hdql_Query * q) {
    assert(q->subject);
    assert(hdql_attr_def_is_collection(q->subject));
    return q->state.collection.selectionArgs;
}

extern "C" const struct hdql_AttrDef *
hdql_query_attr(const struct hdql_Query * q) {
    assert(q);
    return q->subject;
}

extern "C" void
hdql_query_dump( FILE * outf
               , struct hdql_Query * q
               , hdql_ValueTypes * vts
               ) {
    int rc;
    char buf[1024];
    for(; q; q = static_cast<hdql_Query *>(q->next)) {
        rc = hdql_query_str(q, buf, sizeof(buf), vts);
        if(0 != rc) return;
        fputs(buf, outf);
    }
}

extern "C" const hdql_AttrDef *
hdql_query_top_attr(const struct hdql_Query * q_) {
    const hdql::Query<hdql::Selection> * q = q_;
    while(q->next) { q = q->next; }
    if(hdql_attr_def_is_fwd_query(q->subject)) {
        return hdql_query_top_attr(hdql_attr_def_fwd_query(q->subject));
    }
    return q->subject;
}


