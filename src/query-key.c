#include "hdql/query-key.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query.h"
#include "hdql/query-key.h"
#include "hdql/value.h"
#include "hdql/attr-def.h"

#include <assert.h>
#include <string.h>

//
// Keys
//////

int
hdql_query_keys_reserve( struct hdql_Query * query
                       , struct hdql_CollectionKey ** keys_
                       , hdql_Context_t ctx
                       ) {
    if(NULL == query || NULL == keys_ || NULL == query)
        return HDQL_ERR_BAD_ARGUMENT;
    size_t nKeys = hdql_query_depth(query);
    if(nKeys <= 0)
        return HDQL_ERR_BAD_ARGUMENT;  // bad query arg, zero depth
    *keys_ = (struct hdql_CollectionKey *) (
            hdql_context_alloc(ctx, sizeof(struct hdql_CollectionKey)*nKeys));
    if(NULL == *keys_)
        return HDQL_ERR_MEMORY;  // key alloc failure
    struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;  // no types table in context
    struct hdql_CollectionKey * cKey = *keys_;
    int rc = 0;
    do {
        assert(cKey - *keys_ < (ssize_t) nKeys);
        assert(query);
        cKey->isTerminal = 0x0;
        cKey->isList = 0x0;
        cKey->label = NULL;
        const struct hdql_AttrDef * subj = hdql_query_get_subject(query);
        assert(subj);
        hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(subj);
        cKey->code = keyTypeCode;
        if(0x0 != keyTypeCode) {
            const struct hdql_ValueInterface * vi = hdql_types_get_type(types, keyTypeCode);
            cKey->pl.datum = hdql_context_alloc(ctx, vi->size);
        } else {
            // type code for query is zero, that can be a list or null key
            rc = hdql_attr_def_reserve_keys(subj, cKey, ctx);
            if(0 != rc) {
                hdql_context_err_push(ctx, HDQL_ERR_INTERFACE_ERROR
                        , "List-key reserve method failed"
                          " to reserve key for query %p with code %d"
                        , query, rc
                        );
                cKey->isList = 0x0;
                bzero(&cKey->pl, sizeof(cKey->pl));
                cKey->isTerminal = 0x1;
                return HDQL_ERR_INTERFACE_ERROR;
            }
            assert(cKey->isList || NULL == cKey->pl.datum);  // key is either null key or a list
        }
        assert( /* either a typed key (with non-zero code and allocated payload */
                (cKey->code != 0 &&  (!cKey->isList) && cKey->pl.datum != NULL)
                /* or untyped key with list or null (trivial) payload */
             || (cKey->code == 0 && (( cKey->isList && cKey->pl.keysList ) || (cKey->pl.datum == NULL) ))
              );
        query = hdql_query_next_query(query);
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
    struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
    struct hdql_CollectionKey       * destKey = dest;
    const struct hdql_CollectionKey * srcKey  = src;
    int rc = 0;
    while(true) {  // exit condition by srcKey->isTerminal flag
        if(0x0 != srcKey->code) {
            // if of typed key
            const struct hdql_ValueInterface * vi =
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
    struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;
    int rc = 0;
    for(const struct hdql_CollectionKey * cKey = keys; ; ++cKey) {
        if( cKey->code != 0x0 ) {
            const struct hdql_ValueInterface * vi =
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

int
hdql_query_keys_for_each( const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        , hdql_KeyIterationCallback_t callback
                        , void * userdata
                        ) {
    return _hdql_for_each_key(keys, ctx, callback, userdata, 0);
}

int
hdql_query_keys_destroy( struct hdql_CollectionKey * keys
                       , hdql_Context_t ctx
                       ) {
    if(NULL == keys || NULL == ctx)
        return HDQL_ERR_BAD_ARGUMENT;
    struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
    if(NULL == types)
        return HDQL_ERR_CONTEXT_INCOMPLETE;
    int rc = 0;
    for(struct hdql_CollectionKey * cKey = keys; ; ++cKey) {
        if(0x0 != cKey->code) {
            const struct hdql_ValueInterface * vi =
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
    hdql_context_free(ctx, (hdql_Datum_t) keys);
    return rc;
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
    struct _hdql_KeyPrintParams * pp = (struct _hdql_KeyPrintParams *) userdata;
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
    struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
    const struct hdql_ValueInterface * vi = hdql_types_get_type(types, key->code);
    if(!vi->get_as_string) {
        snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
                , " (%zu:%zu:%s:%p)", queryLevel, queryNoInLevel, vi->name, key->pl.datum );
        return 0;
    }
    char bf[64];
    vi->get_as_string(key->pl.datum, bf, sizeof(bf), ctx);
    snprintf( pp->strBuf + cn, pp->strBufLen - cn - 1
            , " (%zu:%zu:%s:\"%s\")", queryLevel, queryNoInLevel, vi->name, bf );
    return 0;
}

int
hdql_query_keys_dump( const struct hdql_CollectionKey * key
                    , char * buf, size_t bufLen
                    , hdql_Context_t ctx
                    ) {
    struct _hdql_KeyPrintParams kpp = {buf, bufLen};
    return hdql_query_keys_for_each(key, ctx, _hdql_key_str, &kpp);
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
    size_t * szPtr = (size_t *) count_;
    if(keys->code) {
        ++(*szPtr);
    }
    return 0;
}

size_t
hdql_keys_flat_view_size( const struct hdql_CollectionKey * keys
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
    struct KeysFlatViewParams * kcpPtr = (struct KeysFlatViewParams *) count_;
    if(keys->code) {
        struct hdql_ValueTypes * vts = hdql_context_get_types(ctx);
        kcpPtr->c->code      = keys->code;
        kcpPtr->c->keyPtr    = keys;
        kcpPtr->c->interface = hdql_types_get_type(vts, keys->code);
        kcpPtr->c->label     = keys->label;
        ++(kcpPtr->c);
    }
    return 0;
}

int
hdql_keys_flat_view_update( const struct hdql_Query * q
                          , const struct hdql_CollectionKey * keys
                          , struct hdql_KeyView * dest
                          , hdql_Context_t ctx ) {
    struct KeysFlatViewParams kfvp = {dest};
    hdql_query_keys_for_each(keys, ctx, _copy_flat_view_ptrs, &kfvp);
    return 0;
}
