#include <cstdio>
#include <unordered_map>
#include <string>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <iostream>

#include <cstring>

#include "hdql/compound.h"
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
    // subject attribute definition
    const hdql_AttributeDefinition * subject;

    union {
        unsigned int scalarVisited:1;
        struct {
            // selection iterator at current tier
            hdql_It_t iterator;
            // internal copy of selector expression, used to create new iterators
            hdql_SelectionArgs_t selectionArgs;
        } iterable;
    } state;

    Selection( const hdql_AttributeDefinition * subject_
             , hdql_SelectionArgs_t selexpr
             );

    void finalize_tier(hdql_Context_t ctx);

    ~Selection();
    // if state is not initialized, creates iterator and set it to first
    // available item, if possible, otherwise, sets it to end
    bool get_value( hdql_Datum_t owner
                  , hdql_Datum_t & value
                  , hdql_CollectionKey * keyPtr
                  , hdql_Context_t ctx
                  );
    // sets iterator to next available item, if possible, or sets it to end
    void advance( hdql_Datum_t owner
                , hdql_Context_t ctx );
    // sets iterator to first available item
    void reset_iterator( hdql_Datum_t owner
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
        if(!SelectionItemT::get_value( ownerDatum  // owner
                                     , currentAttr  // destination
                                     , keys
                                     , ctx
                                     ) ) {
            return false;  // not available
        }
        if(!next) {
            SelectionItemT::advance(ownerDatum, ctx);
            return true;  // got current and this is terminal
        }
        // this is not a terminal query in the chain 
        while(!next->get(currentAttr, keys ? keys + 1 : NULL, ctx)) {  // try to get next and if failed, keep:
            SelectionItemT::advance(ownerDatum, ctx);  // advance next one
            if(!SelectionItemT::get_value( ownerDatum  // owner
                                         , currentAttr  // destination
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
        SelectionItemT::reset_iterator(owner, ctx);
        if(!SelectionItemT::get_value(owner, currentAttr, NULL, ctx))
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

    bool get_value( hdql_Datum_t owner
                  , hdql_Datum_t & value
                  , hdql_CollectionKey * keyPtr
                  , hdql_Context_t ctx
                  ) {
        assert(c <= max_);
        if(c == max_) return false;
        valueCopy = c;
        value = reinterpret_cast<hdql_Datum_t>(&valueCopy);
        return true;
    }

    void advance( hdql_Datum_t owner
                , hdql_Context_t ctx ) {
        assert(c < max_);
        ++c;
    }

    void reset_iterator( hdql_Datum_t owner
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
    bool is_functionlike() const {
        // operation or function node:
        return subject->isCollection  // ... is collection
            && 0x0 == subject->interface.collection.keyTypeCode  // ... provides no key type by itself
            && subject->interface.collection.get_key  // ... can provide keys
            && state.iterable.selectionArgs  // ...has its "selection args" are set (function instance)
            && NULL == subject->interface.collection.compile_selection  //...and has no way to create selection args 
            ;
    }
};

namespace hdql {

Selection::Selection( const hdql_AttributeDefinition * subject_
         , hdql_SelectionArgs_t selexpr
         ) : subject(subject_)
           {
    //if(subject->isSubQuery) {
    //    bzero(&state, sizeof(state));
    //} else
    if(subject->isCollection) {
        state.iterable.iterator = NULL;
        if(selexpr)
            state.iterable.selectionArgs = selexpr;
        else
            state.iterable.selectionArgs = NULL;
    } else {
        state.scalarVisited = 0x0;
        assert(!selexpr);
    }
    if(subject->isSubQuery) {
        assert(NULL == state.iterable.selectionArgs);
        assert(subject->typeInfo.subQuery);
        state.iterable.selectionArgs = reinterpret_cast<hdql_SelectionArgs_t>(subject->typeInfo.subQuery);
    }
}

void
Selection::finalize_tier(hdql_Context_t ctx) {
    if(subject->isCollection) {
        if(state.iterable.iterator)
            subject->interface.collection.destroy(state.iterable.iterator, ctx);
        if(state.iterable.selectionArgs) {
            if(!subject->isSubQuery) {
                subject->interface.collection.free_selection(ctx, state.iterable.selectionArgs);
            }
        }
    } else {
        if(subject->interface.scalar.free_supp_data)
            subject->interface.scalar.free_supp_data(subject->interface.scalar.suppData, ctx);
    }
    if(subject->staticValueFlags) {
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
Selection::get_value( hdql_Datum_t owner
              , hdql_Datum_t & value
              , hdql_CollectionKey * keyPtr
              , hdql_Context_t ctx
              ) {
    if(subject->staticValueFlags) {
        owner = subject->typeInfo.staticValue.datum;
    }
    assert(owner);
    //if(subject->isSubQuery) {
    //    value = hdql_query_get(subject->typeInfo.subQuery, owner
    //            , keyPtr ? reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum) : NULL
    //            , ctx
    //            );
    //    return NULL != value;
    //}
    if(subject->isCollection) {
        if(!state.iterable.iterator) {
            // not initialized/empty -- init, reset
            state.iterable.iterator =
                subject->interface.collection.create(owner, state.iterable.selectionArgs, ctx);
            if(!state.iterable.iterator) return false;
            subject->interface.collection.reset(owner, state.iterable.selectionArgs
                    , state.iterable.iterator, ctx);
        }
        assert(state.iterable.iterator);
        value = subject->interface.collection.dereference(owner, state.iterable.iterator, ctx);
        if(value && keyPtr) {
            //if(0x0 != subject->interface.collection.keyTypeCode) {
                if(subject->interface.collection.get_key) {
                    subject->interface.collection.get_key(owner, state.iterable.iterator, keyPtr, ctx);
                }// else {
                    // This must be considered as an error: generator yielded a
                    // value, key requested by caller, key type code is set,
                    // but collection interface does not provide key copy
                    // implementation.
                //    throw std::runtime_error("collection key requested, but subject"
                //            " interface does not provide key getter");  // TODO: dedicated exception
                //}
            //} else {
            //    // collection interface does not provide key type, this may be
            //    // intended for syntetic collections created by functions and
            //    // operations. In that case, returned value has to be iterated
            //    // ...
            //    assert(0);  // TODO
            //}
        }
    } else {
        if(state.scalarVisited) return false;
        assert(subject->interface.scalar.dereference);
        value = subject->interface.scalar.dereference(owner, ctx, subject->interface.scalar.suppData);
    }
    return NULL != value;
}

// sets iterator to next available item, if possible, or sets it to end
void
Selection::advance( hdql_Datum_t owner
                  , hdql_Context_t ctx ) {
    assert(subject);
    //if(subject->isSubQuery) {
    //    // NOTE: advance for sub-queries has no sense as it was already 
    //} else
    if(subject->isCollection) {
        if(!state.iterable.iterator) return;
        state.iterable.iterator = subject->interface.collection.advance(
                owner, state.iterable.selectionArgs, state.iterable.iterator, ctx);
    } else {
        state.scalarVisited = 0x1;
    }
}

void
Selection::reset_iterator( hdql_Datum_t owner
                   , hdql_Context_t ctx
                   ) {
    if(subject->isSubQuery) {
        hdql_query_reset(subject->typeInfo.subQuery, owner, ctx);
    }
    if(subject->isCollection) {
        if(NULL == state.iterable.iterator) return;
        subject->interface.collection.reset(owner, state.iterable.selectionArgs
                , state.iterable.iterator, ctx);
    } else {
        state.scalarVisited = 0x0;
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
        if(q->subject->isCollection) return false;
        if(q->subject->isSubQuery) {
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

extern "C" const hdql_AttributeDefinition *
hdql_query_top_attr(const struct hdql_Query * q_) {
    const hdql::Query<hdql::Selection> * q = q_;
    while(q->next) { q = q->next; }
    if(q->subject->isSubQuery) {
        return hdql_query_top_attr(q->subject->typeInfo.subQuery);
    }
    return q->subject;
}

extern "C" int
hdql_query_reserve_keys_for( struct hdql_Query * query
                           , struct hdql_CollectionKey ** keys_
                           , hdql_Context_t ctx
                           ) {
    if(NULL == query || NULL == keys_ || NULL == query) return -1;  // bad arguments
    size_t nKeys = query->depth();
    assert(nKeys > 0);
    *keys_ = reinterpret_cast<struct hdql_CollectionKey *>(
            hdql_context_alloc(ctx, sizeof(struct hdql_CollectionKey)*nKeys));
    assert(*keys_);
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types) return -2;  // types table not available in the context
    hdql_CollectionKey * cKey = *keys_;
    int rc = 0;
    do {
        assert(cKey - *keys_ < (ssize_t) nKeys);
        assert(query);
        if(query->subject->isSubQuery) {
            // subquery
            cKey->code = 0x0;
            rc = hdql_query_reserve_keys_for(
                      query->subject->typeInfo.subQuery
                    , reinterpret_cast<hdql_CollectionKey **>(&(cKey->datum))
                    , ctx
                    );
            assert(0 == rc);  // TODO: handle errors
        } else if( (!query->subject->isCollection)
                || (!query->subject->interface.collection.get_key)
                 ) {
            // a scalar attribute or collection which does not provide key
            // copying procedure in its interface
            cKey->code = 0x0;
            cKey->datum = NULL;
        } else {
            // collection which provides key copy
            assert(query->subject->isCollection);
            assert(query->subject->interface.collection.get_key);
            // if is of typed key
            if(0x0 != query->subject->interface.collection.keyTypeCode) {
                // ordinary (defined in HDQL context) key type
                const hdql_ValueInterface * vi =
                    hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
                assert(vi);
                assert(0 != vi->size);  // controlled at insertion
                cKey->code  = query->subject->interface.collection.keyTypeCode;
                cKey->datum = hdql_create_value( query->subject->interface.collection.keyTypeCode
                                               , ctx);
                assert(cKey->datum);
                if(vi->init)
                    vi->init(cKey->datum, vi->size, ctx);
                assert(!query->subject->isSubQuery);
            } else {
                // NOTE: we anticipate the only case when key code is not set,
                // but `get_key()` is defined as functional query -- a
                // function query
                assert(query->is_functionlike());
                hdql_Func * fDef = reinterpret_cast<hdql_Func *>(query->state.iterable.selectionArgs);
                hdql_CollectionKey * cKeyPtr;
                rc = hdql_func_reserve_keys( fDef
                    , &cKeyPtr
                    , ctx );
                if(0 != rc) {
                    hdql_context_err_push(ctx, rc, "reserving keys for query %p"
                            , query );
                }
                assert(NULL != cKeyPtr);
                //assert(NULL == cKey->datum);  // delibirately not initialized
                cKey->datum = reinterpret_cast<hdql_Datum_t>(cKeyPtr);
                assert(rc == 0);  // TODO: handle errors
            }
        }
        assert(!(query->subject->isSubQuery && !cKey->datum) );
        query = static_cast<hdql_Query*>(query->next);
        ++cKey;
    } while(query);
    return rc;
}

int
hdql_query_copy_keys( struct hdql_Query * query
                    , struct hdql_CollectionKey * dest
                    , const struct hdql_CollectionKey * src
                    , hdql_Context_t ctx
                    ) {
    if(NULL == query || NULL == dest || NULL == src || NULL == ctx ) return -1;  // bad arguments
    size_t nKeys = query->depth();
    assert(nKeys > 0);
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    hdql_CollectionKey       * destKey = dest;
    const hdql_CollectionKey * srcKey  = src;
    int rc = 0;
    do {
        assert(srcKey  - src  < (ssize_t) nKeys);
        assert(destKey - dest < (ssize_t) nKeys);
        assert(query);
        if(query->subject->isSubQuery) {
            // subquery
            assert(srcKey->code  == 0x0);
            assert(destKey->code == 0x0);
            rc = hdql_query_copy_keys(
                      query->subject->typeInfo.subQuery
                    , reinterpret_cast<hdql_CollectionKey *>(destKey->datum)
                    , reinterpret_cast<const hdql_CollectionKey *>(srcKey->datum)
                    , ctx
                    );
            assert(0 == rc);  // TODO: treat errors
        } else if( (!query->subject->isCollection)
                || (!query->subject->interface.collection.get_key)
                 ) {
            // a scalar attribute or collection which does not provide key
            // copying procedure in its interface
            assert(srcKey->code   == 0x0);
            assert(srcKey->datum  == NULL);
            assert(destKey->code  == 0x0);
            assert(destKey->datum == NULL);
        } else {
            // collection which provides key copy
            assert(query->subject->isCollection);
            assert(query->subject->interface.collection.get_key);
            // if of typed key
            if(0x0 != query->subject->interface.collection.keyTypeCode) {
                const hdql_ValueInterface * vi =
                    hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
                assert(vi);
                assert(0 != vi->size);  // controlled at insertion
                assert(srcKey->code  == query->subject->interface.collection.keyTypeCode);
                assert(destKey->code == query->subject->interface.collection.keyTypeCode);
                assert(srcKey->datum);
                assert(destKey->datum);
                assert(vi->size);
                rc = vi->copy(destKey->datum, srcKey->datum, vi->size, ctx);
                assert(0 == rc);
            } else {
                assert(query->is_functionlike());
                hdql_Func * fDef = reinterpret_cast<hdql_Func *>(query->state.iterable.selectionArgs);
                rc = hdql_func_copy_keys( fDef
                    , reinterpret_cast<hdql_CollectionKey *>(destKey->datum)
                    , reinterpret_cast<const hdql_CollectionKey *>(srcKey->datum)
                    , ctx
                    );
                assert(rc == 0);  // TODO: handle errors
            }
        }
        query = static_cast<hdql_Query*>(query->next);
        ++destKey;
        ++srcKey;
    } while(query);
    return rc;
}

static int
_hdql_query_for_every_key( const struct hdql_Query * query
                         , const struct hdql_CollectionKey * keys
                         , hdql_Context_t ctx
                         , hdql_KeyIterationCallback_t callback
                         , void * userdata
                         , size_t cLevel
                         ) {
    if(NULL == query || NULL == keys || NULL == ctx || NULL == callback ) return -1;  // bad arguments
    size_t nKeys = query->depth();
    assert(nKeys > 0);
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    const hdql_CollectionKey * keyPtr = keys;
    int rc = 0;
    do {
        assert(keyPtr - keys < (ssize_t) nKeys);
        assert(query);
        if(query->subject->isSubQuery) {
            // subquery
            assert(keyPtr->code  == 0x0);
            rc = _hdql_query_for_every_key(
                      query->subject->typeInfo.subQuery
                    , reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum)
                    , ctx
                    , callback
                    , userdata
                    , cLevel + 1
                    );
            assert(0 == rc);  // TODO: treat errors
        } else if( (!query->subject->isCollection)
                || (!query->subject->interface.collection.get_key)
                 ) {
            // a scalar attribute or collection which does not provide key
            // copying procedure in its interface
            assert(keyPtr->code   == 0x0);
            assert(keyPtr->datum  == NULL);
            callback(query, keyPtr, ctx, userdata, cLevel, keyPtr - keys);
        } else {
            // collection which provides key copy
            assert(query->subject->isCollection);
            assert(query->subject->interface.collection.get_key);
            if(0x0 != query->subject->interface.collection.keyTypeCode) { // if of typed key
                assert(0x0 != query->subject->interface.collection.keyTypeCode);  // controlled at insertion
                const hdql_ValueInterface * vi =
                    hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
                assert(vi);
                assert(0 != vi->size);  // controlled at insertion
                assert(keyPtr->code == query->subject->interface.collection.keyTypeCode);
                assert(keyPtr->datum);
                assert(vi->size);
                callback(query, keyPtr, ctx, userdata, cLevel, keyPtr - keys);
                assert(0 == rc);
            } else {
                assert(query->is_functionlike());
                hdql_Func * fDef = reinterpret_cast<hdql_Func *>(query->state.iterable.selectionArgs);
                rc = hdql_func_call_on_keys( fDef
                    , reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum)
                    , callback, userdata
                    , cLevel + 1, keyPtr - keys
                    , ctx
                    );
                assert(rc == 0);  // TODO: handle errors
            }
        }
        query = static_cast<hdql_Query*>(query->next);
        ++keyPtr;
    } while(query);
    return rc;
}

extern "C" int
hdql_query_for_every_key( const struct hdql_Query * query
                        , const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        , hdql_KeyIterationCallback_t callback
                        , void * userdata
                        ) {
    return _hdql_query_for_every_key(query, keys, ctx, callback, userdata, 0);
}

extern "C" int
hdql_query_destroy_keys_for( struct hdql_Query * query
                           , struct hdql_CollectionKey * keys
                           , hdql_Context_t ctx
                           ) {
    if(NULL == query || NULL == keys || NULL == query) return -1;  // bad arguments
    size_t nKeys = query->depth();
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types) return -2;  // types table not available in the context
    hdql_CollectionKey * cKey = keys;
    int rc = 0;
    do {
        assert(cKey - keys < (ssize_t) nKeys);
        assert(query);
        if(query->subject->isSubQuery) {
            // subquery
            assert(cKey->code == 0x0);
            assert(cKey->datum);
            rc = hdql_query_destroy_keys_for(query->subject->typeInfo.subQuery
                    , reinterpret_cast<hdql_CollectionKey *>(cKey->datum), ctx);
            assert(0 == rc);  // TODO: treat errors
        } else if( (!query->subject->isCollection)
                || (!query->subject->interface.collection.get_key)
                 ) {
            // a scalar attribute or collection which does not provide key
            // copying procedure in its interface
            assert(cKey->code == 0x0);
            assert(cKey->datum == NULL);
        } else {
            // collection which provides key copy
            assert(query->subject->isCollection);
            assert(query->subject->interface.collection.get_key);
            // if of typed key
            if(0x0 != query->subject->interface.collection.keyTypeCode) {
                const hdql_ValueInterface * vi =
                    hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
                assert(vi);
                assert(0 != vi->size);  // controlled at insertion
                assert(cKey->code == query->subject->interface.collection.keyTypeCode);
                assert(cKey->datum);
                if(vi->destroy) {
                    rc = vi->destroy(cKey->datum, vi->size, ctx);
                    assert(rc);
                }
                rc = hdql_destroy_value(cKey->code, cKey->datum, ctx);
                assert(0 == rc);
            } else {
                assert(query->is_functionlike());
                hdql_Func * fDef = reinterpret_cast<hdql_Func *>(query->state.iterable.selectionArgs);
                rc = hdql_func_destroy_keys( fDef
                    , reinterpret_cast<hdql_CollectionKey *>(cKey->datum)
                    , ctx );
                assert(rc == 0);  // TODO: handle errors
            }
        }
        query = static_cast<hdql_Query*>(query->next);
        ++cKey;
    } while(query);
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(keys));
    return rc;
}

extern "C" hdql_Datum_t
hdql_query_get( hdql_Query * query
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

static void
_hdql_query_dump( int count
                , FILE * outf
                , struct hdql_Query * q
                , hdql_Context_t ctx
                , int indent
                ) {
    const struct hdql_ValueTypes * types
        = hdql_context_get_types(ctx);
    fprintf(outf, "%*s", indent, "");
    fprintf(outf, "#%d query %p, current attr is %p, subject %p:\n"
            , count
            , q
            , q ? q->currentAttr : NULL
            , q ? q->subject : NULL
            );
    if(!q) return;

    if(q->subject) {
        fprintf(outf, "%*s", indent, "");
        fprintf( outf, "   %s, %s, "
               , q->subject->isAtomic ? "atomic" : "compound"
               , q->subject->isCollection ? "collection" : "scalar"
               );
        //fprintf(outf, "%*s", indent, "");
        if( q->subject->isSubQuery ) {
            fprintf( outf, "sub-query %p\n", q->subject->typeInfo.subQuery );
        } else if(q->subject->isAtomic) {  // it is an atomic attribute
            fprintf( outf, "type code is %#x, "
                   , (unsigned int) q->subject->typeInfo.atomic.arithTypeCode );
            if(types) {
                const struct hdql_ValueInterface * vi
                        = hdql_types_get_type(types, q->subject->typeInfo.atomic.arithTypeCode);
                if(vi) {
                    fprintf(outf, "`%s' of size %zub\n", vi->name, vi->size);
                } else {
                    fputs(" (no info of null type)\n", outf);
                }
            } else {
                fputs(" (no types in context)\n", outf);
            }
        } else {  // it is a compound
            if(!hdql_compound_is_virtual(q->subject->typeInfo.compound)) {
                fprintf( outf, "`%s' (%p)\n"
                       , hdql_compound_get_name(q->subject->typeInfo.compound)
                       , q->subject->typeInfo.compound
                       );
            } else {
                const hdql_Compound * parent
                    = hdql_virtual_compound_get_parent(q->subject->typeInfo.compound);
                if(!parent) {
                    fputs("virtual compound type\n", outf);
                } else {
                    fprintf(outf, "virtual compound type (from `%s')\n"
                            , hdql_compound_get_name(parent));
                }
            }
        }
        if(q->subject->isCollection) {
            fprintf(outf, "%*s", indent, "");
            fprintf(outf, "   key type code is %#x", q->subject->interface.collection.keyTypeCode );
            if(types) {
                const struct hdql_ValueInterface * vi
                        = hdql_types_get_type(types, q->subject->interface.collection.keyTypeCode);
                if(vi) {
                    fprintf(outf, ", `%s' of size %zub\n", vi->name, vi->size);
                } else {
                    fputs(" (no info of null type)\n", outf);
                }
            } else {
                fputs(" (no types in context)\n", outf);
            }
        }  // else it is scalar, not much to print for it
        if(q->subject->isSubQuery) {
            fprintf(outf, "%*s", indent, "");
            fprintf(outf, "   refers to sub-query %p:\n", q->subject->typeInfo.subQuery );
            if(q->subject->typeInfo.subQuery)
                _hdql_query_dump( 1, outf, reinterpret_cast<hdql_Query *>(q->subject->typeInfo.subQuery)
                        , ctx, indent + 3);
        }
    }

    // xxx, extra recursion?
    //if(q->next) {
    //    _hdql_query_dump( count + 1, outf, static_cast<hdql_Query *>(q->next), ctx, indent);
    //}
}

extern "C" void
hdql_query_dump( FILE * outf
               , struct hdql_Query * q
               , hdql_Context_t ctx
               ) {
    _hdql_query_dump(1, outf, q, ctx, 0);
}



extern "C" void
hdql_query_print_key_to_buf(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey * key
        , hdql_Context_t ctx
        , void * userdata
        , size_t queryLevel
        , size_t queryNoInLevel
        ) {
    assert(userdata);
    hdql_KeyPrintParams * pp = reinterpret_cast<hdql_KeyPrintParams *>(userdata);
    size_t cn = strlen(pp->strBuf);
    if(cn >= pp->strBufLen) return;  // buffer depleted

    if(0x0 == key->code) {
        if(NULL == key->datum) {
            snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                    , " (%zu:%zu:null)", queryLevel, queryNoInLevel );
            return;
        }
        snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                , " (%zu:%zu:%p)", queryLevel, queryNoInLevel, key->datum );
        return;
    }
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    const hdql_ValueInterface * vi =
                hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
    if(!vi->get_as_string) {
        snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                , " (%zu:%zu:%s:%p)", queryLevel, queryNoInLevel, vi->name, key->datum );
        return;
    }
    char bf[64];
    vi->get_as_string(key->datum, bf, sizeof(bf));
    snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
            , " (%zu:%zu:%s:\"%s\")", queryLevel, queryNoInLevel, vi->name, bf );
}

//                                                           __________________
// ________________________________________________________/ Cartesian Product

struct hdql_QueryProduct {
    hdql_Query ** qs;

    hdql_CollectionKey ** keys;
    hdql_Datum_t * values;
};

extern "C" struct hdql_QueryProduct *
hdql_query_product_create( hdql_Query ** qs, hdql_Context_t ctx) {
    struct hdql_QueryProduct * qp = hdql_alloc(ctx, struct hdql_QueryProduct);
    size_t nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {}
    qp->qs      = reinterpret_cast<hdql_Query **>(hdql_context_alloc(ctx, (nQueries+1)*sizeof(struct hdql_Query*)));
    qp->keys    = reinterpret_cast<hdql_CollectionKey **>(hdql_context_alloc(ctx, (nQueries)*sizeof(struct hdql_CollectionKey*)));
    qp->values  = reinterpret_cast<hdql_Datum_t *>(hdql_context_alloc(ctx, (nQueries)*sizeof(hdql_Datum_t)));
    nQueries = 0;
    for(struct hdql_Query ** cq = qs; NULL != *cq; ++cq, ++nQueries) {
        qp->qs[nQueries] = qs[nQueries];
        hdql_query_reserve_keys_for(qp->qs[nQueries], qp->keys + nQueries, ctx);
        qp->values[nQueries] = NULL;
    }
    qp->qs[nQueries++] = NULL;
    return qp;
}

extern "C" bool
hdql_query_cartesian_product_advance( hdql_Datum_t root, struct hdql_QueryProduct * qp, hdql_Context_t context ) {
    // keys iterator (can be null)
    hdql_CollectionKey ** keys = qp->keys;
    // datum instance pointer currently set
    hdql_Datum_t * argValue = qp->values;
    // Retrieve values from argument queries as cartesian product,
    // incrementing iterators one by one till the last one is depleted
    for(hdql_Query ** q = qp->qs; NULL != *q; ++q, ++argValue ) {
        assert(*q);
        // retrieve next item from i-th list and consider it as an argument
        *argValue = hdql_query_get(*q, root, *keys, context);
        if(keys) ++keys;  // increment keys ptr
        // if i-th value is retrieved, nothing else should be done with the
        // argument list
        if(NULL != *argValue) {
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

//                                                     ________________________
// __________________________________________________/ "Flat" keys (key views)

static void _count_flat_keys(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey * keys
        , hdql_Context_t ctx
        , void * count_
        , size_t queryLevel
        , size_t queryNoInLevel 
        ) {
    assert(count_);
    assert(query);
    assert(keys);
    size_t * szPtr = reinterpret_cast<size_t *>(count_);
    if(keys->code) {
        ++(*szPtr);
    }
    query = static_cast<const hdql_Query*>(query->next);
}

extern "C" size_t
hdql_keys_flat_view_size( const struct hdql_Query * q
                        , const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        ) {
    size_t count = 0;
    hdql_query_for_every_key(q, keys, ctx, _count_flat_keys, &count);
    return count;
}


struct KeysFlatViewParams {
    struct hdql_KeyView * c;
};

static void _copy_flat_view_ptrs(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey * keys
        , hdql_Context_t ctx
        , void * count_
        , size_t queryLevel
        , size_t queryNoInLevel 
        ) {
    assert(count_);
    assert(query);
    assert(keys);
    struct KeysFlatViewParams * kcpPtr = reinterpret_cast<struct KeysFlatViewParams *>(count_);
    if(keys->code) {
        hdql_ValueTypes * vts = hdql_context_get_types(ctx);
        kcpPtr->c->code      = keys->code;
        kcpPtr->c->keyPtr    = keys;
        kcpPtr->c->interface = hdql_types_get_type(vts, keys->code);
        ++(kcpPtr->c);
    }
}

extern "C" int
hdql_keys_flat_view_update( const struct hdql_Query * q
                          , const struct hdql_CollectionKey * keys
                          , struct hdql_KeyView * dest
                          , hdql_Context_t ctx ) {
    KeysFlatViewParams kfvp = {dest};
    hdql_query_for_every_key(q, keys, ctx, _copy_flat_view_ptrs, &kfvp);
    return 0;
}


