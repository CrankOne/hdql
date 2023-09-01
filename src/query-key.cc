#include "hdql/query-key.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/query.h"

#include <cassert>

//
// Keys
//////

extern "C" int
hdql_query_keys_reserve( struct hdql_Query * query
                       , struct hdql_CollectionKey ** keys_
                       , hdql_Context_t ctx
                       ) {
    if(NULL == query || NULL == keys_ || NULL == query)
        return HDQL_ERR_BAD_ARGUMENT;
    size_t nKeys = hdql_query_depth(query);
    if(nKeys <= 0)
        return HDQL_ERR_BAD_ARGUMENT;
    *keys_ = reinterpret_cast<struct hdql_CollectionKey *>(
            hdql_context_alloc(ctx, sizeof(struct hdql_CollectionKey)*nKeys));
    if(NULL == *keys_)
        return HDQL_ERR_MEMORY;
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;
    hdql_CollectionKey * cKey = *keys_;
    int rc = 0;
    do {
        assert(cKey - *keys_ < (ssize_t) nKeys);
        assert(query);
        cKey->isTerminal = 0x0;
        const struct hdql_AttributeDefinition * subj = hdql_query_get_subject(query);
        assert(subj);
        if(0x0 == subj->keyTypeCode) {
            if(hdql_kAttrIsCollection & subj->attrFlags) {
                // TODO: check interface for completeness
                if(subj->keyInterface.collection->reserve_keys_list) {
                    cKey->code = 0x0;
                    cKey->pl.keysList = subj->keyInterface.collection->reserve_keys_list(
                              query->state.collection.selectionArgs
                            , query->subject->interface.collection.definitionData
                            , ctx
                            );
                    if(NULL == cKey->pl.keysList) {
                        hdql_context_err_push(ctx, HDQL_ERR_INTERFACE_ERROR
                                , "List-key reserve collection method failed"
                                  " to reserve key for query %p"
                                , query
                                );
                        cKey->isList = 0x0;
                        bzero(&cKey->pl, sizeof(cKey->pl));
                        cKey->isTerminal = 0x1;
                        return HDQL_ERR_INTERFACE_ERROR;
                    } else {
                        cKey->isList = 0x0;
                    }
                } else {
                    cKey->code = 0x0;
                    cKey->isList = 0x0;
                    bzero(&cKey->pl, sizeof(cKey->pl));
                }
            } else {
                cKey->code = 0x0;
                if(query->subject->keyInterface.scalar->reserve_keys_list) {
                    // TODO: check interface for completeness
                    cKey->pl.keysList = query->subject->keyInterface.scalar->reserve_keys_list(
                              query->subject->interface.scalar.definitionData
                            , ctx
                            );
                    cKey->isList = 0x1;
                    if(NULL == cKey->pl.keysList) {
                        hdql_context_err_push(ctx, HDQL_ERR_INTERFACE_ERROR
                                , "List-key reserve scalar method failed"
                                  " to reserve key for query %p"
                                , query
                                );
                        cKey->isList = 0x0;
                        bzero(&cKey->pl, sizeof(cKey->pl));
                        cKey->isTerminal = 0x1;
                        return HDQL_ERR_INTERFACE_ERROR;
                    } else {
                        cKey->isList = 0x0;
                    }
                } else {
                    cKey->isList = 0x0;
                    bzero(&cKey->pl, sizeof(cKey->pl));
                }
            }
        }
        #if 0
        if(query->subject->isSubQuery) {
            // subquery
            cKey->code = 0x0;
            cKey->isCompound = 0x1;
            rc = hdql_query_reserve_keys_for(
                      query->subject->typeInfo.subQuery
                    , &(cKey->payload.keysList)
                    //, reinterpret_cast<hdql_CollectionKey **>(&(cKey->datum))
                    , ctx
                    );
            assert(0 == rc);  // TODO: handle errors
            goto nextQuery;
        }
        if(query->subject->isCollection) {
            if( NULL != query->subject->keyInterface.collection.get_key ) {
                // collection does not provide key retrieve function; i.e. it
                // is a set. In debug mode make sure interface does not define
                // other key-related routines and continue
                assert(NULL == query->subject->keyInterface.collection.reserve_key);
                // ...
                goto nextQuery;
            }
            if(0x0 != query->subject->interface.collection.keyTypeCode) {
                // ordinary (defined in HDQL context) key type
                const hdql_ValueInterface * vi =
                    hdql_types_get_type(types, query->subject->interface.collection.keyTypeCode);
                assert(vi);
                assert(0 != vi->size);  // controlled at insertion
                cKey->code  = query->subject->interface.collection.keyTypeCode;
                cKey->isCompound = 0x0;
                cKey->payload.datum = hdql_create_value( query->subject->interface.collection.keyTypeCode
                                               , ctx);
                assert(cKey->payload.datum);
                if(vi->init)
                    vi->init(cKey->payload.datum, vi->size, ctx);
                assert(!query->subject->isSubQuery);
            } else {
                // key type is not defined for collection, but key retrieval
                // operation is set, key reserve operation must be defined
                assert(NULL != query->subject->interface.collection.reserve_key);  // TODO: control at insertion
                rc = query->subject->interface.collection.reserve_key(
                          reinterpret_cast<hdql_CollectionKey **>(&(cKey->payload.datum))
                        , query->state.iterable.selectionArgs
                        , ctx
                        );
                assert(0 == rc);  // TODO: handle errors
            }
            goto nextQuery;
        }
        if(!query->subject->isCollection) {
            if( NULL != query->subject->interface.scalar.get_key
             || NULL != query->subject->interface.scalar.reserve_key
             || NULL != query->subject->interface.scalar.destroy_key
              ) {
                // scalar attribute provides key retrieval method

            } else {
                // scalar attribute does not provide key retrieval method ->
                // make this a blank key
                cKey->code = 0x0;
                cKey->isCompound = 0x0;
                bzero(&(cKey->payload), sizeof(cKey->payload));
                goto nextQuery;
            }
        }
        assert(0);
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
                // NOTE: we anticipate the only possible cases when key code is
                // not set, but `get_key()` is defined as functional query -- a
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
        #endif
        query = static_cast<hdql_Query*>(query->next);
        ++cKey;
    } while(query);
    --cKey;
    cKey->isTerminal = 0x1;
    return rc;
}

int
hdql_query_keys_copy( struct hdql_CollectionKey * dest
                    , const struct hdql_CollectionKey * src
                    , hdql_Context_t ctx
                    ) {
    assert( NULL != dest
         && NULL != src
         && NULL != ctx
         );
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    hdql_CollectionKey       * destKey = dest;
    const hdql_CollectionKey * srcKey  = src;
    int rc = 0;
    while(true) {  // exit condition by srcKey->isTerminal flag
        if(0x0 != srcKey->code) {
            // if of typed key
            const hdql_ValueInterface * vi =
                hdql_types_get_type(types, srcKey->code);
            assert(vi);
            assert(0 != vi->size);  // controlled at insertion
            assert(srcKey->code  == destKey->code);
            assert(srcKey->pl.datum);
            assert(destKey->pl.datum);
            assert(vi->size);
            rc = vi->copy(destKey->pl.datum, srcKey->pl.datum, vi->size, ctx);
            assert(0 == rc);
        } else if(srcKey->isList) {
            // subquery
            assert(srcKey->code  == 0x0);
            assert(srcKey->isList);
            assert(destKey->code == 0x0);
            assert(destKey->isList);
            rc = hdql_query_keys_copy(destKey->pl.keysList, srcKey->pl.keysList, ctx);
            assert(0 == rc);  // TODO: treat errors
        }
        #ifndef NDEBUG
        else {
            // a scalar attribute or collection which does not provide key
            // copying procedure in its interface
            assert(srcKey->code   == 0x0);
            assert(srcKey->pl.datum  == NULL);
            assert(destKey->code  == 0x0);
            assert(destKey->pl.datum == NULL);
        }
        #endif
        if(srcKey->isTerminal) {
            assert(destKey->isTerminal);
            break;
        }
        ++destKey;
        ++srcKey;
    }
    return rc;
}

static int
_hdql_for_each_key( const struct hdql_CollectionKey * keys
                  , hdql_Context_t ctx
                  , hdql_KeyIterationCallback_t callback
                  , void * userdata
                  , size_t cLevel
                  ) {
    if(NULL == keys || NULL == ctx || NULL == callback )
        return HDQL_ERR_BAD_ARGUMENT;
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;
    int rc = 0;
    for(const struct hdql_CollectionKey * cKey = keys; ; ++cKey) {
        if( cKey->code != 0x0 ) {
            const hdql_ValueInterface * vi =
                hdql_types_get_type(types, cKey->code);
            assert(vi);
            assert(0 != vi->size);  // controlled at insertion
            assert(cKey->pl.datum);
            rc = callback(cKey, ctx, userdata, cLevel, cKey - keys);
            if(0 != rc) return rc;
        } else if( cKey->isList ) {
            // subquery
            assert(cKey->code  == 0x0);
            rc = _hdql_for_each_key(
                      cKey->pl.keysList
                    , ctx
                    , callback
                    , userdata
                    , cLevel + 1
                    );
            if(0 != rc) return rc;
        }
        #ifndef NDEBUG
        else {
            assert(cKey->code == 0x0);
            assert(!cKey->isList);
            assert(NULL == cKey->pl.keysList);
        }
        #endif
        if(cKey->isTerminal) break;
    }
    return rc;
}

extern "C" int
hdql_query_keys_for_each( const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        , hdql_KeyIterationCallback_t callback
                        , void * userdata
                        ) {
    return _hdql_for_each_key(keys, ctx, callback, userdata, 0);
}

extern "C" int
hdql_query_keys_destroy( struct hdql_CollectionKey * keys
                       , hdql_Context_t ctx
                       ) {
    if(NULL == keys || NULL == ctx)
        return HDQL_ERR_BAD_ARGUMENT;
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;
    int rc = 0;
    for(hdql_CollectionKey * cKey = keys; ; ++cKey) {
        if(0x0 != cKey->code) {
            const hdql_ValueInterface * vi =
                hdql_types_get_type(types, cKey->code);
            assert(vi);
            assert(cKey->pl.datum);
            if(vi->destroy) {
                rc = vi->destroy(cKey->pl.datum, vi->size, ctx);
                assert(rc);
            } else {
                rc = hdql_destroy_value(cKey->code, cKey->pl.datum, ctx);
                assert(0 == rc);
            }
        } else if( cKey->isList ) {
            rc = hdql_query_keys_destroy(cKey->pl.keysList, ctx);
        }
        #ifndef NDEBUG
        else {
            assert(cKey->code == 0x0);
            assert(!cKey->isList);
            assert(NULL == cKey->pl.datum);
        }
        #endif
        if(cKey->isTerminal) break;
    }
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

struct _hdql_KeyPrintParams {
    char * strBuf;
    size_t strBufLen;
};

static int
_hdql_key_str(
          const struct hdql_CollectionKey * key
        , hdql_Context_t ctx
        , void * userdata
        , size_t queryLevel
        , size_t queryNoInLevel
        ) {
    assert(userdata);
    assert(!key->isList);
    _hdql_KeyPrintParams * pp = reinterpret_cast<_hdql_KeyPrintParams *>(userdata);
    size_t cn = strlen(pp->strBuf);
    if(cn >= pp->strBufLen) return -1;  // buffer depleted

    if(0x0 == key->code) {
        if(NULL == key->pl.datum) {
            snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                    , " (%zu:%zu:null)", queryLevel, queryNoInLevel );
            return 0;
        }
        snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                , " (%zu:%zu:%p)", queryLevel, queryNoInLevel, key->pl.datum );
        return 0;
    }
    hdql_ValueTypes * types = hdql_context_get_types(ctx);
    const hdql_ValueInterface * vi =
                hdql_types_get_type(types, key->code);
    if(!vi->get_as_string) {
        snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                , " (%zu:%zu:%s:%p)", queryLevel, queryNoInLevel, vi->name, key->pl.datum );
        return 0;
    }
    char bf[64];
    vi->get_as_string(key->pl.datum, bf, sizeof(bf));
    snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
            , " (%zu:%zu:%s:\"%s\")", queryLevel, queryNoInLevel, vi->name, bf );
    return 0;
}

extern "C" int
hdql_query_keys_dump( const struct hdql_CollectionKey * key
                    , char * buf, size_t bufLen
                    , hdql_Context_t ctx
                    ) {
    struct _hdql_KeyPrintParams kpp = {buf, bufLen};
    return hdql_query_keys_for_each(key, ctx, _hdql_key_str, &kpp);
}

