#include "hdql/context.h"
#include "hdql/compound.h"
#include "hdql/types.h"
#include "hdql/util/allocator.h"
#include "hdql/util/ht.h"
#include "hdql/errors.h"

#include "hdql/internal-api.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>  /* vsnprintf() */

/* Init HT size of the context's local data */
#ifndef HDQL_HT_DEFAULT_CONTEXT_LOCALDATA
#   define HDQL_HT_DEFAULT_CONTEXT_LOCALDATA 128
#endif

/* Init HT size of the context's type names dictionary */
#ifndef HDQL_HT_DEFAULT_CONTEXT_NTYPES
#   define HDQL_HT_DEFAULT_CONTEXT_NTYPES 256
#endif

/* Max length of the error message kept in context's stack */
#ifndef HDQL_ERR_MAX_MSG_LEN
#   define HDQL_ERR_MAX_MSG_LEN 1024
#endif

/* Context-local data
 *
 * Whenever applications would like to store their own data bound to context
 * (for possible future usage in the functions and interfaces), this dictionary
 * is a proper place. Inherited across context like types, functions, etc.
 *
 * We do not expose this type, it is built in to the context instance. Context,
 * however does not know about its parent (but definition tables do).
 * */
struct hdql_ContextLocalData {
    struct hdql_ContextLocalData *parent;
    struct hdql_ht * byName;
};

struct hdql__VCmpd {
    struct hdql__VCmpd *next;
    struct hdql_Compound *compound;
};

struct hdql__Error {
    struct hdql__Error *next;
    hdql_Err_t code;
    char *message;
};

struct VariadicDatumInfo {
    uint32_t nUsedBytes
           , nAllocatedBytes;
};

struct hdql_Context {
    const struct hdql_Allocator *allocator;
    uint32_t flags;

    struct hdql_ContextLocalData *localData;
    struct hdql_ValueTypes       *valueTypes;
    struct hdql_Operations       *operations;
    struct hdql_Functions        *functions;
    struct hdql_Converters       *converters;
    struct hdql_Constants        *constants;
    struct hdql_RandGen          *randgen;
    struct hdql_Compounds        *compounds;

    struct hdql__VCmpd *virtualCompounds;
    struct hdql__Error *errors;

#ifdef HDQL_TYPES_DEBUG
    /* records of type `char *' */
    struct hdql_ht *typesByPtr;
#endif
};

/*
 * Lifecycle
 */

hdql_Context_t
hdql_context_create(uint32_t flags, const struct hdql_Allocator *allocator) {
    assert(allocator);
    hdql_Context_t r = (hdql_Context_t) allocator->alloc(
            sizeof(struct hdql_Context), allocator->userdata);
    if(!r) return NULL;  /* allocation failure */
    bzero(r, sizeof(struct hdql_Context));  /* init to zeroes to cond check on failures */
    r->allocator = allocator;
    r->flags = flags;

#ifdef HDQL_TYPES_DEBUG
    if(!(r->typesByPtr = hdql_ht_create(r->allocator
            , HDQL_HT_DEFAULT_CONTEXT_NTYPES
            , HDQL_MURMUR3_32_DEFAULT_SEED
            ))) goto onFailure;
#endif

    if(!(r->localData  = allocator->alloc(sizeof(struct hdql_ContextLocalData)
                    , allocator->userdata))) goto onFailure;
    r->localData->byName = NULL;
    r->localData->parent = NULL;
    bzero(r->localData, sizeof(struct hdql_ContextLocalData));
    if(!(r->valueTypes = _hdql_value_types_table_create(NULL, r))) goto onFailure;
    if(!(r->converters = _hdql_converters_create(NULL, r))) goto onFailure;
    if(!(r->operations = _hdql_operations_create(NULL, r))) goto onFailure;
    if(!(r->functions  = _hdql_functions_create(NULL, r))) goto onFailure;
    if(!(r->constants  = _hdql_constants_create(NULL, r))) goto onFailure;
    if(!(r->randgen    = _hdql_randgen_create(NULL, r))) goto onFailure;
    if(!(r->compounds  = hdql__compounds_create(NULL, r))) goto onFailure;

    return r;
onFailure:
    /* TODO: check error stack and print if not empty to communicate errors? */
    hdql_context_destroy(r);
    return NULL;
}

hdql_Context_t
hdql_context_create_descendant(hdql_Context_t pCtx, uint32_t flags) {
    /* TODO: use argument allocator instead */
    hdql_Context_t r = pCtx->allocator->alloc(
            sizeof(struct hdql_Context), pCtx->allocator->userdata);
    if(!r) return NULL;  /* allocation failure */
    bzero(r, sizeof(struct hdql_Context));  /* init to zeroes to cond check on failures */
    r->allocator = pCtx->allocator;
    r->flags = flags;

#ifdef HDQL_TYPES_DEBUG
    if(!(r->typesByPtr = hdql_ht_create(r->allocator
            , HDQL_HT_DEFAULT_CONTEXT_NTYPES
            , HDQL_MURMUR3_32_DEFAULT_SEED
            ))) goto onFailure;
#endif

    if(!(r->localData  = r->allocator->alloc(sizeof(struct hdql_ContextLocalData)
                    , r->allocator->userdata))) goto onFailure;
    r->localData->byName = NULL;
    r->localData->parent = pCtx->localData;
    if(!(r->valueTypes = _hdql_value_types_table_create(pCtx->valueTypes, r))) goto onFailure;
    if(!(r->converters = _hdql_converters_create(pCtx->converters, r))) goto onFailure;
    if(!(r->operations = _hdql_operations_create(pCtx->operations, r))) goto onFailure;
    if(!(r->functions  = _hdql_functions_create(pCtx->functions, r))) goto onFailure;
    if(!(r->constants  = _hdql_constants_create(pCtx->constants, r))) goto onFailure;
    if(!(r->randgen    = _hdql_randgen_create( flags & HDQL_CTX_LOCAL_RANDGEN
                                          ? NULL : pCtx->randgen
                                          , r))) goto onFailure;
    if(!(r->compounds  = hdql__compounds_create(pCtx->compounds, r))) goto onFailure;

    return r;
onFailure:
    /* TODO: check error stack and print if not empty to communicate errors? */
    hdql_context_destroy(r);
    return NULL;
}

#ifdef HDQL_TYPES_DEBUG
/* called on destruction of variadic value */
static int
hdql__destroy_type_label(const unsigned char *ptr
        , size_t kLen
        , void ** value
        , void * userdata
        ) {
    struct hdql_Allocator *allocator = (struct hdql_Allocator *) userdata;
    allocator->free(*value, allocator->userdata);
    *value = NULL;
    return 0;
}
#endif

void
hdql_context_destroy(hdql_Context_t context) {
    assert(context);
    assert(context->allocator);
    if(context->functions) _hdql_functions_destroy(context->functions, context);
    /* iterate v compounds backwards as they can be based on each other and
     * are created from basic to derived, so deletion order must be in a
     * reverse order. Reverting and deleting reverted list */
    if(context->virtualCompounds) {
        struct hdql__VCmpd *cur = context->virtualCompounds;
        struct hdql__VCmpd *rev = NULL;
        /* revert */
        while(cur) {
            struct hdql__VCmpd *next = cur->next;
            cur->next = rev;
            rev = cur;
            cur = next;
        }
        /* delete reverted */
        while(rev) {
            struct hdql__VCmpd *next = rev->next;
            if(rev->compound)
                hdql_virtual_compound_destroy(rev->compound, context);
            context->allocator->free(rev, context->allocator->userdata);
            rev = next;
        }
        context->virtualCompounds = NULL;
    }
    if(context->operations)
        _hdql_operations_destroy(context->operations, context);
    if(context->converters)
        _hdql_converters_destroy(context->converters, context);
    if(context->valueTypes)
        _hdql_value_types_table_destroy(context->valueTypes, context);
    if(context->constants)
        _hdql_constants_destroy(context->constants, context);
    if(context->randgen)
        _hdql_randgen_destroy(context->randgen, context);
    /* sic! compounds destroyed in this order: non-virtual compounds get destroyed
     * AFTER context as they are used to resolve attribute definitions in forwarding
     * queries while virtual compounds get deleted */
    if(context->compounds)
        hdql__compounds_destroy(context->compounds, context);
    hdql_context_wipe_errors(context);
    if(context->localData) {
        context->allocator->free(context->localData, context->allocator->userdata);
    }
#ifdef HDQL_TYPES_DEBUG
    hdql_ht_iter(context->typesByPtr, hdql__destroy_type_label, context->allocator);
    hdql_ht_destroy(context->typesByPtr);
    context->typesByPtr = NULL;
#endif
    context->allocator->free(context, context->allocator->userdata);
}

/*
 * Getters
 */

struct hdql_ValueTypes *
hdql_context_get_types( hdql_Context_t context ) {
    return context->valueTypes;
}

struct hdql_Operations *
hdql_context_get_operations( hdql_Context_t context ) {
    return context->operations;
}

struct hdql_Functions *
hdql_context_get_functions( hdql_Context_t context ) {
    return context->functions;
}

struct hdql_Converters *
hdql_context_get_conversions(hdql_Context_t context) {
    return context->converters;
}

struct hdql_Constants *
hdql_context_get_constants(hdql_Context_t context) {
    return context->constants;
}

struct hdql_RandGen *
hdql_context_get_randgen(hdql_Context_t context) {
    return context->randgen;
}

struct hdql_Compounds *
hdql_context_get_compounds(hdql_Context_t context) {
    return context->compounds;
}

hdql_Datum_t
hdql_context_alloc( hdql_Context_t context
                  , size_t len
                  ) {
    assert(len > 0);
    return context->allocator->alloc(len, context->allocator->userdata);
}

#ifdef HDQL_TYPES_DEBUG  /* TODO: rewrite in C */
hdql_Datum_t
hdql_context_alloc_typed( hdql_Context_t context
                        , size_t size
                        , const char * typeName
                        ) {
    /* TODO: strip off `const' qualifier ?*/
    assert(size > 0);
    hdql_Datum_t ptr = hdql_context_alloc(ctx, size);
    int rc = hdql_ht_ins(context->typesByPtr, &ptr, sizeof(void*), typeName );
    return ptr;
}

hdql_Datum_t
hdql_context_check_type( hdql_Context_t ctx
                       , hdql_Datum_t ptr
                       , const char * typeName
                       ) {
    char errbf[128];
    auto it = ctx->typesByPtr.find(ptr);
    if(ctx->typesByPtr.end() == it) {
        snprintf(errbf, sizeof(errbf), "Pointer %p is not allocated by HDQL"
                " context %p", ptr, ctx );
        throw std::runtime_error(errbf);
    }
    // TODO: strip off `const' qualifier
    if(it->second != typeName) {
        snprintf(errbf, sizeof(errbf), "Pointer %p is of C-type `%s' while"
                " cast to `%s' requested", ptr, it->second.c_str(), typeName );
        throw std::runtime_error(errbf);
    }
    return ptr;
}
#endif

int
hdql_context_free(hdql_Context_t context, hdql_Datum_t ptr) {
    context->allocator->free(ptr, context->allocator->userdata);
    return 0;
}

/* internal API */
int
hdql__context_add_virtual_compound(hdql_Context_t context, struct hdql_Compound * compoundPtr) {
    struct hdql__VCmpd *n
        = (struct hdql__VCmpd *) context->allocator->alloc(sizeof(struct hdql__VCmpd)
                , context->allocator->userdata);
    if(!n) return HDQL_ERR_MEMORY;
    n->compound = compoundPtr;
    n->next = NULL;
    if(context->virtualCompounds) {
        struct hdql__VCmpd *last = context->virtualCompounds;
        while(last->next) last = last->next;
        last->next = n;
    } else {
        context->virtualCompounds = n;
    }
    return HDQL_ERR_CODE_OK;
}

int
hdql_context_err_push( hdql_Context_t context
                     , hdql_Err_t code
                     , const char * format, ...) {
    char errBuf[HDQL_ERR_MAX_MSG_LEN];
    size_t actualErrMsgLength = 0;
    if(format && *format != '\0') {
        va_list argptr;
        va_start(argptr, format);
        vsnprintf(errBuf, sizeof(errBuf), format, argptr);
        va_end(argptr);
        /* print error message to stderr if special flags was set for the context */
        if(HDQL_CTX_PRINT_PUSH_ERROR & context->flags) {
            fputs(errBuf, stderr);
            fputc('\n', stderr);
        }
        /* put error to error stack */
        actualErrMsgLength = strlen(errBuf);
    }
    /* create error entry */
    struct hdql__Error *n = (struct hdql__Error *) context->allocator->alloc(
            sizeof(struct hdql__Error *), context->allocator->userdata);
    if(!n) return HDQL_ERR_MEMORY;
    n->code = code;
    if(actualErrMsgLength) {
        n->message = (char *) context->allocator->alloc(
                actualErrMsgLength+1, context->allocator->userdata);
        if(n->message) {
            context->allocator->free((hdql_Datum_t) n, context->allocator->userdata);
            return HDQL_ERR_MEMORY;
        }
        strcpy(n->message, errBuf);
    } else {
        n->message = NULL;
    }
    /* append errors list with new error record */
    if(context->virtualCompounds) {
        struct hdql__Error *last = context->errors;
        while(last->next) last = last->next;
        last->next = n;
    } else {
        context->errors = n;
    }
    return HDQL_ERR_CODE_OK;
}

bool
hdql_context_has_errors(hdql_Context_t context) {
    return context->errors;
}

int
hdql_context_for_every_error(hdql_Context_t context
        , int (*callback)(int rc, const char *msg, void *userdata)
        , void *userdata ) {
    int rc = 0;
    struct hdql__Error *cur = context->errors;
    while(cur) {
        rc = callback(cur->code, cur->message, userdata);
        if(0 != rc) return rc;
    }
    return rc;
}

int
hdql_context_wipe_errors(hdql_Context_t context) {
    if(!context->errors) return HDQL_ERR_CODE_OK;
    /* destroy errors list, plain delete */
    struct hdql__Error *cur = context->errors;
    while(cur) {
        struct hdql__Error *next = cur->next;
        if(cur->message)
            context->allocator->free(cur->message, context->allocator->userdata);
        context->allocator->free(cur, context->allocator->userdata);
        cur = next;
    }
    context->errors = NULL;
    return HDQL_ERR_CODE_OK;
}

int
hdql_context_custom_data_add(
          hdql_Context_t context
        , const char * name
        , void * ptr
        , bool overwrite
        ) {
    if(!context->localData) {
        if(!(context->localData->byName = hdql_ht_create(context->allocator
                , HDQL_HT_DEFAULT_CONTEXT_LOCALDATA
                , HDQL_MURMUR3_32_DEFAULT_SEED ))) return HDQL_ERR_MEMORY;
    }
    int rc = hdql_ht_s_ins(context->localData->byName, name, ptr);
    if(rc == HDQL_HT_RC_INSERTED)
        return HDQL_ERR_CODE_OK;
    else if(rc == HDQL_HT_ERR_MEM)
        return HDQL_ERR_MEMORY;
    else if(rc == HDQL_HT_RC_UPDATED) {
        if(!overwrite)
            return HDQL_ERR_NAME_COLLISION;
        else
            return HDQL_ERR_CODE_OK;
    } else
        return HDQL_ERR_GENERIC;
}

static void *
hdql__context_custom_data_get( struct hdql_ContextLocalData *ld
                            , const char * name
                            ) {
    void *r = hdql_ht_s_get(ld->byName, name);
    if(r) return r;
    /* not found -- look in parent */
    if(ld->parent)
        return hdql__context_custom_data_get(ld->parent, name);
    else
        return NULL;
}

void *
hdql_context_custom_data_get( hdql_Context_t context
                            , const char * name
                            ) {
    if(!context->localData) return NULL;
    return hdql__context_custom_data_get(context->localData, name);
}

static int
hdql__context_custom_data_erase( struct hdql_ContextLocalData *ld
                               , const char * name
                               , bool unwind
                               ) {
    int rc = hdql_ht_s_rm(ld->byName, name);
    if(HDQL_HT_RC_OK == rc) return HDQL_ERR_CODE_OK;
    if(HDQL_HT_RC_ERR_NOENT != rc) return HDQL_ERR_GENERIC;  /* unkown code */
    if(!unwind) return HDQL_ERR_UNKNOWN_ATTRIBUTE;
    /* not found -- look in parent */
    if(ld->parent)
        return hdql__context_custom_data_erase(ld->parent, name, unwind);
    else
        return HDQL_ERR_UNKNOWN_ATTRIBUTE;
}

const struct hdql_Allocator *
hdql__context_get_allocator(struct hdql_Context *context) {
    return context->allocator;
}

int
hdql_context_custom_data_erase( hdql_Context_t context
                              , const char * name
                              , bool unwind
                              ) {
    if(!context->localData) return HDQL_ERR_UNKNOWN_ATTRIBUTE;
    return hdql__context_custom_data_erase(context->localData, name, unwind);
}



