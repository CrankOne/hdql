#include "hdql/query-key.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/helpers/print-tree.h"
#include "hdql/query.h"
#include "hdql/query-key.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/attr-def.h"

#include <assert.h>
#include <string.h>

#define KEY_PL_TYPE_PLAIN   0x0
#define KEY_PL_TYPE_LIST    0x1
#define KEY_PL_TYPE_UNION   0x2

struct hdql_Key {
    /**\brief Collection key type code
     *
     * HDQL manages key instance lifecycle, so key must be one of the
     * registered types (yet, only iomplementing some rather basic lifecycle
     * transitions, like copying or destruction).
     *
     * Zero code type code is permitted here and is reserved for empty key
     * placeholders, key lists and unions. */
    hdql_ValueTypeCode_t code:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
    hdql_ValueTypeCode_t plType:2;  /**< how to interpret payload */
    hdql_ValueTypeCode_t isTerminal:1;  /**< marks last key in list when `isList` */
    /**\brief Collection key item payload */
    union {
        /**\brief Actual datum of the key when it is not a list */
        hdql_Datum_t datum;
        /**\brief Array of keys
         *
         * Used only when `isList` is set. Sequence must be terminated with
         * item with `isTerminal` set. */
        struct hdql_Key * keysList;
    } pl;
    /**\brief Key may have a string label assigned */
    const char * label;
};

/* internal API call */
struct hdql_Key * hdql__keys_next(struct hdql_Key * cur) {
    assert(!cur->isTerminal);
    return cur + 1;
}

/* internal API call */
struct hdql_Key * hdql__keys_prev(struct hdql_Key * cur) {
    return cur - 1;
}

/* internal API call */
struct hdql_Key * hdql__key_get_list_bgn(struct hdql_Key * k) {
    assert(k->plType == KEY_PL_TYPE_LIST);
    assert(k->pl.keysList);
    return k->pl.keysList;
}

struct hdql_Key *
hdql__key_get_list_at(struct hdql_Key * k, size_t n) {
    return k->pl.keysList + n;
}

/* internal API call */
bool hdql__key_is_terminal(const struct hdql_Key * k) {
    return k->isTerminal;
}

hdql_Key_t
hdql_key_new(hdql_Context_t context) {
    hdql_Key_t k = (hdql_Key_t) hdql_alloc(context, struct hdql_Key);
    assert(k);
    hdql_key_mark_empty(k);
    return k;
}


bool hdql_key_is_empty(const struct hdql_Key *k) {
    assert(k);
    return k->plType == KEY_PL_TYPE_PLAIN && k->code == 0x0;
}

void
hdql_key_mark_empty(struct hdql_Key * k) {
    assert(k);
    bzero(k, sizeof(struct hdql_Key));
}

/* Key label API
 */

void hdql_key_set_label(hdql_Key_t key, const char * label) {
    key->label = label;
}

bool hdql_key_is_labeled(hdql_Key_t key) {
    return key->label;
}

const char * hdql_key_get_label(hdql_Key_t key) {
    return key->label;
}

/* Datum key API
 */

bool hdql_key_is_datum(const struct hdql_Key * k) {
    assert(k);
    return k->plType == KEY_PL_TYPE_PLAIN && k->code != 0x0;
}

void hdql_key_set_datum(struct hdql_Key *k, hdql_ValueTypeCode_t tc, hdql_Datum_t d) {
    assert(k);
    assert(k->plType == KEY_PL_TYPE_PLAIN);
    assert(k->code == 0x0 || k->code == tc);
    k->code = tc;
    k->pl.datum = d;
}

hdql_ValueTypeCode_t
hdql_key_datum_get_type_code(const struct hdql_Key * k)  {
    assert(k);
    assert(k->plType == KEY_PL_TYPE_PLAIN);
    assert(k->code != 0x0);
    return k->code;
}

hdql_Datum_t
hdql_key_datum_get(const struct hdql_Key * k) {
    assert(k);
    assert(k->plType == KEY_PL_TYPE_PLAIN);
    return k->pl.datum;
}

/* Keys list API
 */

bool hdql_key_is_list(const struct hdql_Key *k) {
    assert(k);
    return k->plType == KEY_PL_TYPE_LIST;
}

int
hdql_key_mark_as_list(struct hdql_Key * k, size_t n, struct hdql_Context * context) {
    assert(k);
    assert(context);
    if(k->code || k->plType) return HDQL_ERR_GENERIC;
    const size_t nb = sizeof(struct hdql_Key)*n;
    k->pl.keysList = (struct hdql_Key *) hdql_context_alloc(context, nb);
    bzero(k->pl.keysList, nb);
    k->plType = 0x1;
    k->pl.keysList[n-1].isTerminal = 0x1;
    return HDQL_ERR_CODE_OK;
}

size_t
hdql_key_get_list_len(const struct hdql_Key * k) {
    assert(k);
    assert(k->plType == KEY_PL_TYPE_LIST);
    size_t r = 0;
    for(struct hdql_Key * kk = k->pl.keysList; ; ++kk) {
        ++r;
        if(kk->isTerminal)
            return r;
    }
    return 0;
}

int
hdql_key_swap_list_item(hdql_Key_t k, size_t n, hdql_Key_t element) {
    assert(k);
    assert(element);
    if(k->plType != KEY_PL_TYPE_LIST) return HDQL_ERR_GENERIC;
    struct hdql_Key interim = *element;
    *element = k->pl.keysList[n];
    k->pl.keysList[n] = interim;
    return HDQL_ERR_CODE_OK;
}

hdql_Key_t
hdql_key_get_list_item(hdql_Key_t k, size_t n) {
    if(k->plType != KEY_PL_TYPE_LIST) return NULL;
    return k->pl.keysList + n;
}

/* Union key API
 */

bool hdql_key_is_union(const struct hdql_Key *k) {
    assert(k);
    return k->plType == KEY_PL_TYPE_UNION;
}

/* Complex API
 */

/* Callback type for recursive iteration. `queryLevel' denotes the level with
 * respect to the root item, `queryNoInLevel' denotes the number of element
 * in a list or union and starts from 1 to differ from scalar items. */
typedef int (*hdql_KeyIterationCallback_t)(
          struct hdql_Key *
        , hdql_Context_t
        , void *
        , size_t queryLevel
        , size_t queryNoInLevel
        );

/* Utility function recursively iterating the key structure
 * NOTE: ctx can be null if unused by callback */
static int
_hdql__for_each_key( struct hdql_Key * k
             , hdql_Context_t ctx
             , hdql_KeyIterationCallback_t callback
             , void * userdata
             , size_t cLevel
             , size_t nq
             ) {
    if(hdql_key_is_empty(k) || hdql_key_is_datum(k)) {
        return callback(k, ctx, userdata, cLevel, nq);
    }
    assert(hdql_key_is_list(k) || hdql_key_is_union(k));
    size_t snq = 1;
    for(hdql_Key_t ck = k->pl.keysList; ; ++ck, ++snq) {
        bool isLast = ck->isTerminal;
        int rc = _hdql__for_each_key(ck, ctx, callback, userdata, cLevel+1, snq);
        if(rc != HDQL_ERR_CODE_OK) return rc;
        if(isLast) break;
    }
    return callback(k, ctx, userdata, cLevel, nq);
}

/* Callback for key item destruction, for _hdql_for_each_key() */
static int _hdql__destroy_key_plain(struct hdql_Key * k, hdql_Context_t context
        , void * unused1, size_t unused2, size_t snq) {
    ((void) unused1);
    ((void) unused2);
    if(hdql_key_is_list(k)) {
        hdql_context_free(context, (hdql_Datum_t) k->pl.keysList);
    } else if(hdql_key_is_datum(k)) {
        hdql_context_free(context, (hdql_Datum_t) k->pl.datum);
    } 
    {
        if(snq) {
            return HDQL_ERR_CODE_OK;
        }
        hdql_context_free(context, (hdql_Datum_t) k);
    }
    return HDQL_ERR_CODE_OK;
}

int
hdql_key_destroy(hdql_Key_t k, struct hdql_Context * context) {
    return _hdql__for_each_key(k, context, _hdql__destroy_key_plain, NULL, 0, 0);
}

int
hdql_key_reserve_for_query( struct hdql_Query * query
                          , struct hdql_Key * rootKey
                          , hdql_Context_t context
                          ) {
    if(NULL == query || NULL == rootKey || NULL == query)
        return HDQL_ERR_BAD_ARGUMENT;
    size_t nKeys = hdql_query_depth(query);
    if(nKeys == 0) return HDQL_ERR_BAD_ARGUMENT;  /* bad query arg, zero depth */
    int rc = hdql_key_mark_as_list(rootKey, nKeys, context);
    if(HDQL_ERR_CODE_OK != rc) return rc;  /* failed to allocate key list */
    struct hdql_ValueTypes * types = hdql_context_get_types(context);
    if(NULL == types) return HDQL_ERR_CONTEXT_INCOMPLETE;  /* no types table in the context */
    /* circumventing public API here to avoid integer indexes and work directly
     * with ptrs, possibly gain some performance */
    struct hdql_Key * cKey = rootKey->pl.keysList;
    do {
        assert(cKey - rootKey->pl.keysList < (ssize_t) nKeys);
        assert(query);
        //if( hdql_query_is_labeled(query) )
        //    cKey->label = hdql_query_get_label(query);
        //else
            cKey->label = NULL;  // TODO
        const struct hdql_AttrDef * subj = hdql_query_get_subject(query);
        assert(subj);
        cKey->code = hdql_attr_def_get_key_type_code(subj);
        if(0x0 != cKey->code) {
            const struct hdql_ValueInterface * vi = hdql_types_get_type(types, cKey->code);
            cKey->pl.datum = hdql_context_alloc(context, vi->size);
            if(!cKey->pl.datum) {
                /* TODO: cleanup */
                hdql_context_err_push(context, HDQL_ERR_MEMORY
                        , "Failed to allocate key datum of type %x of size"
                        " %zu for query %p"
                        , cKey->code, vi->size, query
                        );
                return HDQL_ERR_MEMORY;
            }
            bzero(cKey->pl.datum, vi->size);
        } else {
            // type code for query is zero, that can be a list or null key
            rc = hdql_attr_def_reserve_key(subj, cKey, context);
            if(0 != rc) {
                /* TODO: cleanup */
                hdql_context_err_push(context, HDQL_ERR_INTERFACE_ERROR
                        , "Attribute definition reserve method failed"
                          " to reserve key for query %p with code %d"
                        , query, rc
                        );
                cKey->plType = KEY_PL_TYPE_LIST;
                bzero(&cKey->pl, sizeof(cKey->pl));
                cKey->isTerminal = 0x1;
                return HDQL_ERR_INTERFACE_ERROR;
            }
        }  /* TODO: consider union key here? */
        assert( /* either a typed key (with non-zero code and allocated payload */
                (cKey->code != 0 &&  (cKey->plType == KEY_PL_TYPE_PLAIN) && cKey->pl.datum != NULL)
                /* or untyped key with list or null (trivial) payload */
             || (cKey->code == 0 && (( cKey->plType == KEY_PL_TYPE_LIST && cKey->pl.keysList ) || (cKey->pl.datum == NULL) ))
              );
        query = hdql_query_next_query(query);
        ++cKey;
    } while(query);
    --cKey;
    cKey->isTerminal = 0x1;
    return rc;
}

int
hdql_key_copy_value( struct hdql_Key * destKey
                   , const struct hdql_Key * srcKey
                   , hdql_Context_t ctx
                   ) {
    assert( destKey );
    assert( srcKey );
    assert( ctx );

    if(hdql_key_is_empty(srcKey)) {
        assert(hdql_key_is_empty(destKey));
        return HDQL_ERR_CODE_OK;
    } else if(hdql_key_is_datum(srcKey)) {
        assert(hdql_key_is_datum(destKey));
        struct hdql_ValueTypes * types = hdql_context_get_types(ctx);
        const struct hdql_ValueInterface * vi = hdql_types_get_type(types, srcKey->code);
        if(hdql_key_datum_get_type_code(srcKey) != hdql_key_datum_get_type_code(destKey)) {
            hdql_context_err_push(ctx, HDQL_ERR_GENERIC,
                        "mismatch type code during key datum: src type is %#x (%s), dest is %#x (%s)"
                        , srcKey->code, vi ? vi->name : "(null)"
                        , destKey->code, srcKey->code ? hdql_types_get_type(types, destKey->code)->name : "(null)"
                        );
            return HDQL_ERR_GENERIC;
        }
        memcpy(hdql_key_datum_get(destKey), hdql_key_datum_get(srcKey), vi->size);
        return HDQL_ERR_CODE_OK;
    } else if(hdql_key_is_list(srcKey)) {
        assert(hdql_key_is_list(destKey));
        /* circumvent API */
        struct hdql_Key * dk = destKey->pl.keysList;
        for(const struct hdql_Key * sk = srcKey->pl.keysList; ; ++sk) {
            int rc = hdql_key_copy_value(dk++, sk, ctx);
            if(rc != HDQL_ERR_CODE_OK) {
                hdql_context_err_push(ctx, rc, "key copy: failed to copy key(s)"
                        " within a list, element %p #%zu", sk
                        , sk - srcKey->pl.keysList);
                return rc;
            }
            if(sk->isTerminal) break;
        }
        return HDQL_ERR_CODE_OK;
    }
    hdql_context_err_push(ctx, HDQL_ERR_BAD_ARGUMENT, "key copy: src key %p is in"
            " a bad state: not a list, nor a datum/empty; plType=%d, code=%x, pl={%d, %p}"
            , srcKey, (int) srcKey->plType, srcKey->code
            , srcKey->pl.datum
            , (void*) srcKey->pl.keysList );
    return HDQL_ERR_GENERIC;  /* unknown key state */
}

/* Printing
 */

static size_t _hdql_key_nchildren(hdql_TreeLikeNode_t node, void * context_) {
    ((void) context_);
    struct hdql_Key *k = (struct hdql_Key *) node;
    if(hdql_key_is_list(k)) {
        return hdql_key_get_list_len(k);
    } else {
        assert(hdql_key_is_union(k));  /* TODO */
        return 0;
    }
}

static hdql_TreeLikeNode_t _hdql_key_acquire_child(hdql_TreeLikeNode_t node, size_t i, void * context_) {
    ((void) context_);
    struct hdql_Key *k = (struct hdql_Key *) node;
    assert(hdql_key_is_list(k) || hdql_key_is_union(k));
    return (hdql_TreeLikeNode_t) (k->pl.keysList + i);
}

static int _hdql_key_is_leaf(hdql_TreeLikeNode_t node, void * context_) {
    ((void) context_);
    struct hdql_Key *k = (struct hdql_Key *) node;
    return hdql_key_is_datum(k) || hdql_key_is_empty(k);
}

static const char * _hdql_key_label(char * buf, size_t bufSize
            , hdql_TreeLikeNode_t node, void * context_) {
    struct hdql_Key *k = (struct hdql_Key *) node;
    struct hdql_Context * context = (struct hdql_Context *) context_;
    const struct hdql_ValueTypes *vt = hdql_context_get_types(context);
    /*size_t nUsed = 0;  TODO: use this to print <<label>> when available */
    if(hdql_key_is_empty(k)) {
        strncpy(buf, "(empty)", bufSize);
    } else if(hdql_key_is_datum(k)) {
        const struct hdql_ValueInterface * iface = hdql_types_get_type(vt, hdql_key_datum_get_type_code(k));
        char valueStr[64] = "...";
        if(k->pl.datum) {
            if(iface->get_as_string) {
                iface->get_as_string(k->pl.datum, valueStr, sizeof(valueStr), context);
            } /* otherwise "..." will be printed */
        } else {
            strcpy(valueStr, "(null)");
        }
        snprintf(buf, bufSize, "\"%s\":%s", valueStr, iface->name);
    } else {
        if(hdql_key_is_list(k)) {
            snprintf(buf, bufSize, "list of length %zu", hdql_key_get_list_len(k));
        } else {
            assert((hdql_key_is_union(k)));
            strncpy(buf, "union", bufSize);
        }
    }
    return buf;
}

void hdql_key_print_tree(const struct hdql_Key *k
        , FILE *outFile, struct hdql_Context * context) {
    static const struct hdql_TreeLikeNodeIFace tlIFace = {
        .nchildren = _hdql_key_nchildren,
        .acquire_child = _hdql_key_acquire_child,
        .is_leaf = _hdql_key_is_leaf,
        .label = _hdql_key_label,
        .release_child = NULL
    };
    hdql_print_tree_like((const hdql_TreeLikeNode_t) k
            , context, tlIFace, outFile, 80);
}

/* Flat key view
 */

static int _hdql_count_meaningful_keys(struct hdql_Key * k, hdql_Context_t context
        , void *userdata_, size_t unused, size_t snq) {
    ((void) unused);
    /* ignore non-datum items */
    if(!hdql_key_is_datum(k)) return HDQL_ERR_CODE_OK;
    ++(*((size_t *) userdata_));
    return HDQL_ERR_CODE_OK;
}

size_t
hdql_key_flat_view_size( const struct hdql_Key * key
                        , hdql_Context_t context
                        ) {
    size_t count = 0;
    int rc = _hdql__for_each_key((struct hdql_Key *) key, context
            , _hdql_count_meaningful_keys, &count, 0, 0);
    assert(rc == HDQL_ERR_CODE_OK);
    if(HDQL_ERR_CODE_OK != rc) return 0;
    return count;
}


typedef struct {
    hdql_Key_t * cflatView;
} FlatViewIterator;

static int _hdql_set_meaningful_keys(struct hdql_Key * k, hdql_Context_t unused1
        , void *userdata_, size_t unused2, size_t snq) {
    ((void) unused1);
    ((void) unused2);
    /* ignore non-datum items */
    if(!hdql_key_is_datum(k)) return HDQL_ERR_CODE_OK;
    FlatViewIterator * it = ((FlatViewIterator *) userdata_);
    *((it->cflatView)++) = k;
    return HDQL_ERR_CODE_OK;
}

int
hdql_key_flat_view_populate(struct hdql_Key * key
        , hdql_Key_t * flatView
        ) {
    FlatViewIterator it = {flatView};
    return _hdql__for_each_key(key, NULL
            , _hdql_set_meaningful_keys, &it, 0, 0);
}

