#include <cstdio>
#include <unordered_map>
#include <string>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <iostream>

#include <cstring>

#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/function.h"
#include "hdql/query.h"
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
    const hdql_AttributeDefinition * subject;

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

    Selection( const hdql_AttributeDefinition * subject_
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

    Query( const hdql_AttributeDefinition * subject_
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
    
    TestingItem( const hdql_AttributeDefinition * subject_
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
    hdql_Query( const hdql_AttributeDefinition * subject_
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

Selection::Selection( const hdql_AttributeDefinition * subject_
                    , hdql_SelectionArgs_t selexpr
                    ) : subject(subject_)
                      {
    //if(subject->isSubQuery) {
    //    bzero(&state, sizeof(state));
    //} else
    if(hdql_kAttrIsCollection & subject->attrFlags) {
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

    if(hdql_kAttrIsSubquery & subject->attrFlags) {
        assert(NULL == state.collection.selectionArgs);
        assert(subject->typeInfo.subQuery);
        state.collection.selectionArgs = reinterpret_cast<hdql_SelectionArgs_t>(subject->typeInfo.subQuery);
    }
}

void
Selection::finalize_tier(hdql_Context_t ctx) {
    if(hdql_kAttrIsCollection & subject->attrFlags) {
        if(state.collection.iterator)
            subject->interface.collection.destroy( state.collection.iterator
                                                 , subject->interface.collection.definitionData
                                                 , ctx
                                                 );
        if(state.collection.selectionArgs) {
            if(!(hdql_kAttrIsSubquery & subject->attrFlags)) {
                subject->interface.collection.free_selection(ctx, state.collection.selectionArgs);
            }
        }
    } else {
        if(subject->interface.scalar.destroy)
            subject->interface.scalar.destroy(state.scalar.dynamicSuppData
                    , subject->interface.scalar.definitionData, ctx);
    }

    if(hdql_kAttrIsStaticValue & subject->attrFlags) {
        //hdql_context_local_attribute_destroy(ctx, );
        //attrDef->typeInfo.staticValue.datum = hdql_context_alloc(ws->context, sizeof(hdql_Int_t));
        if(subject->typeInfo.staticValue.datum) {
            hdql_context_free(ctx, subject->typeInfo.staticValue.datum);
        }
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
    if(hdql_kAttrIsStaticValue & subject->attrFlags) {
        owner = subject->typeInfo.staticValue.datum;
    }
    assert(owner);  // otherwise missing `reset()`
    //if(subject->isSubQuery) {
    //    value = hdql_query_get(subject->typeInfo.subQuery, owner
    //            , keyPtr ? reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum) : NULL
    //            , ctx
    //            );
    //    return NULL != value;
    //}
    if(hdql_kAttrIsCollection & subject->attrFlags) {
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
        assert(state.collection.iterator);
        value = subject->interface.collection.dereference(
                          state.collection.iterator
                        , keyPtr
                        , subject->interface.collection.definitionData
                        , ctx
                        );
    } else {
        if(state.scalar.isVisited)
            return false;
        assert(subject->interface.scalar.dereference);
        value = subject->interface.scalar.dereference( owner
                , state.scalar.dynamicSuppData
                , keyPtr
                , subject->interface.scalar.definitionData
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
    if(hdql_kAttrIsCollection & subject->attrFlags) {
        if(!state.collection.iterator)
            return;  // permitted state for empty collections
        state.collection.iterator = subject->interface.collection.advance(
                  state.collection.iterator
                , subject->interface.collection.definitionData
                , ctx
                );
    } else {
        state.scalar.isVisited = true;
    }
}

void
Selection::reset( hdql_Datum_t newOwner
                , hdql_Context_t ctx
                ) {
    if(hdql_kAttrIsSubquery & subject->attrFlags) {
        hdql_query_reset(subject->typeInfo.subQuery, newOwner, ctx);
    }
    if(hdql_kAttrIsCollection & subject->attrFlags) {
        if(NULL != state.collection.iterator) {
            state.collection.iterator =
                subject->interface.collection.reset(newOwner
                    , state.collection.selectionArgs
                    , state.collection.iterator
                    , subject->interface.collection.definitionData
                    , ctx
                    );
        } else {
            state.collection.iterator =
                subject->interface.collection.create(newOwner
                    , subject->interface.collection.definitionData
                    , state.collection.selectionArgs
                    , ctx
                    );
        }
    } else {
        if(subject->interface.scalar.instantiate) {
            if(NULL != state.scalar.dynamicSuppData ) {
                assert(subject->interface.scalar.reset);  // provided `instantiate()`, but no `reset()`?
                state.scalar.dynamicSuppData = subject->interface.scalar.reset(newOwner
                        , state.scalar.dynamicSuppData
                        , subject->interface.scalar.definitionData
                        , ctx
                        );
            } else {
                state.scalar.dynamicSuppData
                    = subject->interface.scalar.instantiate( newOwner
                        , subject->interface.scalar.definitionData
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
          const struct hdql_AttributeDefinition * attrDef
        , hdql_SelectionArgs_t selArgs
        , hdql_Context_t ctx
        ) {
    return new hdql_Query( attrDef, selArgs );
}

extern "C" const struct hdql_AttributeDefinition *
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
            , q->subject->attrFlags & hdql_kAttrIsStaticValue ? "static " : ""
            , q->subject->attrFlags & hdql_kAttrIsCollection ? "collection" : "scalar"
            , q->subject->attrFlags & hdql_kAttrIsSubquery
              ? " forwarding to "
              : ( q->subject->attrFlags & hdql_kAttrIsAtomic
                ? "of atomic type"
                : ( hdql_compound_is_virtual(q->subject->typeInfo.compound)
                  ? "of virtual compound type"
                  : "of compound type"
                  )
                )
            );
    if(hdql_kAttrIsSubquery) {
        // query 0x23fff34 is "[static ](collection|scalar) forwarding to %p which is ..."
        _M_pr("%p which is ", q->subject->typeInfo.subQuery );
        return hdql_query_str( q->subject->typeInfo.subQuery
                , strbuf + nUsed, buflen - nUsed, vts );
    }
    if(hdql_kAttrIsAtomic & q->subject->attrFlags) {
        // "[static ](collection|scalar) of atomic type <%s>"
        assert(vts);
        const hdql_ValueInterface * vi
            = hdql_types_get_type( vts
                                 , (hdql_kAttrIsStaticValue & q->subject->attrFlags)
                                 ? q->subject->typeInfo.atomic.arithTypeCode
                                 : q->subject->typeInfo.staticValue.typeCode
                                 );
        assert(NULL != vi);
        _M_pr("<%s>", vi->name);
        if(hdql_kAttrIsStaticValue & q->subject->attrFlags) {
            // "[static ](collection|scalar) of atomic type <%s> [=%s|at %p]"
            if(q->subject->typeInfo.staticValue.datum && vi->get_as_string) {
                char vBf[64];
                int rc = vi->get_as_string(q->subject->typeInfo.staticValue.datum
                            , vBf, sizeof(vBf));
                if(0 == rc) {
                    _M_pr(" =%s", vBf);
                } else {
                    _M_pr(" =? at %p", q->subject->typeInfo.staticValue.datum);
                }
            } else {
                _M_pr(" at %p", q->subject->typeInfo.staticValue.datum);
            }
        }
    } else {
        // query 0x23fff34 is "[static ](collection|scalar) of [virtual] compound type [based on] <%s>"
        _M_pr("%s<%s>"
                , hdql_compound_is_virtual(q->subject->typeInfo.compound)
                ? "based on "
                : ""
                , hdql_compound_get_name(q->subject->typeInfo.compound));
    }
    #undef _M_pr
    return 0;
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
        if(hdql_kAttrIsCollection & q->subject->attrFlags) return false;
        if(hdql_kAttrIsSubquery & q->subject->attrFlags) {
            if(!hdql_query_is_fully_scalar(q->subject->typeInfo.subQuery)) return false;
        }
    } while(q->next && (q = static_cast<hdql_Query *>(q->next)));
    return true;
}

extern "C" const struct hdql_AttributeDefinition *
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

extern "C" const hdql_AttributeDefinition *
hdql_query_top_attr(const struct hdql_Query * q_) {
    const hdql::Query<hdql::Selection> * q = q_;
    while(q->next) { q = q->next; }
    if(hdql_kAttrIsSubquery & q->subject->attrFlags) {
        return hdql_query_top_attr(q->subject->typeInfo.subQuery);
    }
    return q->subject;
}


//                                                           __________________
// ________________________________________________________/ Cartesian Product

struct hdql_QueryProduct {
    hdql_Query ** qs;

    hdql_CollectionKey ** keys;
    hdql_Datum_t * values;
};

extern "C" struct hdql_QueryProduct *
hdql_query_product_create( hdql_Query ** qs
                         , hdql_Datum_t ** values_
                         , hdql_Context_t ctx
                         ) {
    struct hdql_QueryProduct * qp = hdql_alloc(ctx, struct hdql_QueryProduct);
    size_t nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {}
    if(0 == nQueries) {
        hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(qp));
        return NULL;
    }
    qp->qs      = reinterpret_cast<hdql_Query **>(hdql_context_alloc(ctx, (nQueries+1)*sizeof(struct hdql_Query*)));
    qp->keys    = reinterpret_cast<hdql_CollectionKey **>(hdql_context_alloc(ctx, (nQueries)*sizeof(struct hdql_CollectionKey*)));
    qp->values  = reinterpret_cast<hdql_Datum_t *>(hdql_context_alloc(ctx, (nQueries)*sizeof(hdql_Datum_t)));
    *values_ = qp->values;
    nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {
        qp->qs[nQueries] = qs[nQueries];
        hdql_query_keys_reserve(qp->qs[nQueries], qp->keys + nQueries, ctx);
        qp->values[nQueries] = NULL;
    }
    qp->qs[nQueries++] = NULL;
    return qp;
}

extern "C" bool
hdql_query_product_advance(
          hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context
        ) {
    // keys iterator (can be null)
    hdql_CollectionKey ** keys = qp->keys;
    // datum instance pointer currently set
    hdql_Datum_t * value = qp->values;
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(hdql_Query ** q = qp->qs; NULL != *q; ++q, ++value ) {
        assert(*q);
        // retrieve next item from i-th list and consider it as an argument
        *value = hdql_query_get(*q, root, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        // if i-th value is retrieved, nothing else should be done with the
        // argument list
        if(NULL != *value) {
            return true;
        }
        // i-th generator depleted
        // - if it is the last query list, we have exhausted argument list
        if(NULL == *(q+1)) {
            return false;
        }
        // - otherwise, reset it and all previous and proceed with next
        {
            // supplementary iteration pointers to use at re-set
            hdql_CollectionKey ** keys2 = qp->keys;
            hdql_Datum_t * argValue2 = qp->values;
            // iterate till i-th inclusive
            for(hdql_Query ** q2 = qp->qs; q2 <= q; ++q2, ++argValue2 ) {
                assert(*q2);
                // re-set the query
                hdql_query_reset(*q2, root, context);
                // get 1st value of every query till i-th, inclusive
                *argValue2 = hdql_query_get(*q2, root, *keys2, context);
                // since we've iterated it already, a 1st lements of j-th
                // query should always exist, otherwise one of the argument
                // queris is not idempotent and that's an error
                assert(*argValue2);
                if(keys2) ++keys2;
            }
        }
    }
    assert(false);  // unforeseen loop exit, algorithm error
}

extern "C" void
hdql_query_product_reset( hdql_Datum_t root
        , struct hdql_QueryProduct * qp
        , hdql_Context_t context ) {
    hdql_CollectionKey ** keys = qp->keys;
    hdql_Datum_t * value = qp->values;
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(hdql_Query ** q = qp->qs; NULL != *q; ++q, ++value ) {
        assert(*q);
        hdql_query_reset(*q, root, context);
        *value = hdql_query_get(*q, root, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        if(NULL != *value) {
            continue;
        }
        {   // i-th value is not retrieved, product results in an empty set,
            // we should drop the result
            hdql_Datum_t * argValue2 = qp->values;
            // iterate till i-th inclusive
            for(hdql_Query ** q2 = qp->qs; q2 <= q; ++q2, ++argValue2 ) {
                hdql_query_reset(*q2, root, context);
                *argValue2 = NULL;
            }
            return;
        }
    }
    assert(false);  // unforeseen loop exit, algorithm error
}

extern "C" hdql_CollectionKey **
hdql_query_product_reserve_keys( struct hdql_Query ** qs
        , hdql_Context_t context ) {
    size_t nQueries = 0;
    for(struct hdql_Query ** q = qs; NULL != *q; ++q) { ++nQueries; }
    hdql_CollectionKey ** keys = reinterpret_cast<hdql_CollectionKey**>(
        hdql_context_alloc(context, sizeof(struct hdql_CollectionKey *)*nQueries));
    hdql_CollectionKey ** cKey = keys;
    for(struct hdql_Query ** q = qs; NULL != *q; ++q, ++cKey) {
        hdql_query_keys_reserve(*q, cKey, context);
    }
    return keys;
}

extern "C" struct hdql_Query **
hdql_query_product_get_query(struct hdql_QueryProduct * qp) {
    return qp->qs;
}

extern "C" void
hdql_query_product_destroy(
            struct hdql_QueryProduct *qp
          , hdql_Context_t context
          ) {
    hdql_CollectionKey ** cKey = qp->keys;
    for(struct hdql_Query ** cq = qp->qs; NULL != *cq; ++cq, ++cKey) {
        hdql_query_keys_destroy(*cKey, context);
        *cKey = NULL;
    }

    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(qp->values));
    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(qp->keys));
    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(qp->qs));

    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(qp));
}

//                                                     ________________________
// __________________________________________________/ "Flat" keys (key views)

static int _count_flat_keys(
          const struct hdql_CollectionKey * keys
        , hdql_Context_t ctx
        , void * count_
        , size_t queryLevel
        , size_t queryNoInLevel 
        ) {
    assert(count_);
    assert(keys);
    size_t * szPtr = reinterpret_cast<size_t *>(count_);
    if(keys->code) {
        ++(*szPtr);
    }
    return 0;
}

extern "C" size_t
hdql_keys_flat_view_size( const struct hdql_Query * q
                        , const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        ) {
    size_t count = 0;
    hdql_query_keys_for_each( keys, ctx, _count_flat_keys, &count);
    return count;
}


struct KeysFlatViewParams {
    struct hdql_KeyView * c;
};

static int _copy_flat_view_ptrs(
          const struct hdql_CollectionKey * keys
        , hdql_Context_t ctx
        , void * count_
        , size_t queryLevel
        , size_t queryNoInLevel 
        ) {
    assert(count_);
    assert(keys);
    struct KeysFlatViewParams * kcpPtr = reinterpret_cast<struct KeysFlatViewParams *>(count_);
    if(keys->code) {
        hdql_ValueTypes * vts = hdql_context_get_types(ctx);
        kcpPtr->c->code      = keys->code;
        kcpPtr->c->keyPtr    = keys;
        kcpPtr->c->interface = hdql_types_get_type(vts, keys->code);
        ++(kcpPtr->c);
    }
    return 0;
}

extern "C" int
hdql_keys_flat_view_update( const struct hdql_Query * q
                          , const struct hdql_CollectionKey * keys
                          , struct hdql_KeyView * dest
                          , hdql_Context_t ctx ) {
    KeysFlatViewParams kfvp = {dest};
    hdql_query_keys_for_each(keys, ctx, _copy_flat_view_ptrs, &kfvp);
    return 0;
}


