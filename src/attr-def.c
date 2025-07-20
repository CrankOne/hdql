#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/internal-ifaces.h"

#include <string.h>
#include <assert.h>

/**\brief Compound's attribute definition descriptor
 *
 * Defines, how certain attribute has to be accessed within a compound type.
 * In terms of data access interface may define either:
 *  - (if `isAtomic` is set) -- atomic attribute, i.e. attribute of simple
 *    arithmetic type; `typeInfo.atomic` struct instance defines
 *    data access interface
 *  - (if `isAtomic` is not set) -- a compound attribute, i.e. attribute
 *    consisting of multiple attributes (atomic or compounds);
 *    `typeInfo.compound` shall be used to access the data in term of
 *    sub-attributes.
 *  - (if `isFwdQuery` is set) -- value retrieved using collection interface
 *    based on query object provided instead of atomic or compound interface
 * In terms of how the data is associated to owning object:
 *  - (if `isCollection` is set) -- a collection attribute that should be
 *    accessed via iterator, which lifecycle is steered by `interface.collection`
 *  - (if `isCollection` is not set) -- a scalar attribute (optionally)
 *    providing value which can be retrieved (or set) using
 *    `interface.scalar` interface.
 * Prohibited combinations:
 *  - isFwdQuery && !isCollection -- subqueries are always iterable collections,
 *    (yet subquery type must never be a terminal one)
 *  - isFwdQuery && staticValue -- "static" values do not need an owning
 *    instance
 * */
struct hdql_AttrDef {
    /** If set, it attribute is of atomic type (int, float, etc), otherwise 
     * it is a compound */
    unsigned int isAtomic:1;
    /** If set, attribute is collection of values indexed by certain key,
     * otherwise it is a scalar (a single value) */
    unsigned int isCollection:1;
    /** If set, attribute should be interpreted as a sub-query */
    unsigned int isFwdQuery:1;
    /** If set, attribute has no owner:
     *  0x1 -- constant value (parser should calculate simple arithmetics)
     *  0x2 -- externally set variable (no arithmetics on parsing) */
    unsigned int staticValueFlags:2;
    /** If set, owning query must delete this attribute definition with
     * finilize() */
    unsigned int isStray:1;
 
    /**\brief Key type code, can be zero (unset) */
    hdql_ValueTypeCode_t keyTypeCode:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;

    /** Data access interface for a value */
    union {
        struct hdql_ScalarAttrInterface     scalar;
        struct hdql_CollectionAttrInterface collection;
    } interface;

    /**\brief Keys list allocation callback */
    hdql_ReserveKeysListCallback_t reserveKeys;

    /** Defines value type features */
    union {
        struct hdql_AtomicTypeFeatures   atomic;
        struct hdql_Compound           * compound;
        struct hdql_Query              * fwdQuery;
    } typeInfo;
};  /* struct hdql_AttrDef */

/*                          * * *   * * *   * * *                            */

struct hdql_AttrDef *
hdql_attr_def_create_atomic_scalar(
          struct hdql_AtomicTypeFeatures        * typeInfo
        , struct hdql_ScalarAttrInterface       * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t context
        ) {
    if(NULL == typeInfo || NULL == interface) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Type info or/and interface is (are) NULL: typeInfo=%p"
                  ", interface=%p; failed to construct new atomic scalar"
                  " attribute definition", typeInfo, interface );
        return NULL;
    }
    if((0x0 != keyTypeCode) && (NULL != keyIFace)) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Both key type code and key retrieve interface are provided"
                  " for atomic scalar attribute, refusing to"
                  " construct attribute definition: keyTypeCode=%d, key iface=%p"
                , (int) keyTypeCode, keyIFace );
        return NULL;
    }
    /* ... other validity checks */
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x1;
    ad->isCollection     = 0x0;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x0;
    ad->isStray          = 0x0;

    ad->interface.scalar = *interface;
    ad->typeInfo.atomic = *typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserveKeys = keyIFace;

    return ad;
}

struct hdql_AttrDef *
hdql_attr_def_create_atomic_collection(
          struct hdql_AtomicTypeFeatures        * typeInfo
        , struct hdql_CollectionAttrInterface   * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t context
        ) {
    if(NULL == typeInfo || NULL == interface) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Type info or/and interface is (are) NULL: typeInfo=%p"
                  ", interface=%p; failed to construct new atomic collection"
                  " attribute definition", typeInfo, interface );
        return NULL;
    }
    if((0x0 != keyTypeCode) && (NULL != keyIFace)) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Both key type code and key retrieve interface are provided"
                  " for atomic collection attribute, refusing to"
                  " construct attribute definition: keyTypeCode=%d, key iface=%p"
                , (int) keyTypeCode, keyIFace );
        return NULL;
    }
    /* ... other validity checks */
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x1;
    ad->isCollection     = 0x1;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x0;
    ad->isStray          = 0x0;

    ad->interface.collection = *interface;
    ad->typeInfo.atomic = *typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserveKeys = keyIFace;

    return ad;
}

struct hdql_AttrDef *
hdql_attr_def_create_compound_scalar(
          struct hdql_Compound                  * typeInfo
        , struct hdql_ScalarAttrInterface       * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t context
        ) {
    if(NULL == typeInfo || NULL == interface) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Type info or/and interface is (are) NULL: typeInfo=%p"
                  ", interface=%p; failed to construct new compound scalar"
                  " attribute definition", typeInfo, interface );
        return NULL;
    }
    if((0x0 != keyTypeCode) && (NULL != keyIFace)) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Both key type code and key retrieve interface are provided"
                  " for compound scalar attribute, refusing to"
                  " construct attribute definition: keyTypeCode=%d, key iface=%p"
                , (int) keyTypeCode, keyIFace );
        return NULL;
    }
    /* ... other validity checks */
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x0;
    ad->isCollection     = 0x0;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x0;
    ad->isStray          = 0x0;

    ad->interface.scalar = *interface;
    ad->typeInfo.compound = typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserveKeys = keyIFace;

    return ad;
}

struct hdql_AttrDef *
hdql_attr_def_create_compound_collection(
          struct hdql_Compound                  * typeInfo
        , struct hdql_CollectionAttrInterface   * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t context
        ) {
    if(NULL == typeInfo || NULL == interface) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Type info or/and interface is (are) NULL: typeInfo=%p"
                  ", interface=%p; failed to construct new compound collection"
                  " attribute definition", typeInfo, interface );
        return NULL;
    }
    if((0x0 != keyTypeCode) && (NULL != keyIFace)) {
        hdql_context_err_push(context, HDQL_ERR_BAD_ARGUMENT
                , "Both key type code and key retrieve interface are provided"
                  " for compound collection attribute, refusing to"
                  " construct attribute definition: keyTypeCode=%d, key iface=%p"
                , (int) keyTypeCode, keyIFace );
        return NULL;
    }
    /* ... other validity checks */
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x0;
    ad->isCollection     = 0x1;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x0;
    ad->isStray          = 0x0;

    ad->interface.collection = *interface;
    ad->typeInfo.compound = typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserveKeys = keyIFace;

    return ad;
}

static struct hdql_CollectionKey *
_hdql_reserve_keys_list_for_fwd_query(
          const hdql_Datum_t fwdQ_
        , hdql_Context_t context ) {
    struct hdql_Query * sq = (struct hdql_Query *) fwdQ_;
    struct hdql_CollectionKey * k = NULL;
    int rc = hdql_query_keys_reserve(sq, &k, context);
    if(0 != rc) return NULL;
    return k;
}

struct hdql_AttrDef *
hdql_attr_def_create_fwd_query(
          struct hdql_Query * subquery
        , hdql_Context_t context
        ) {
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    const struct hdql_AttrDef * fwdAD = hdql_query_top_attr(subquery);
    bool isFullyScalar = hdql_query_is_fully_scalar(subquery);

    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    /* atomic/collection flags are inherited from what the fwded
     * query is supposed to fetch */
    ad->isAtomic         = hdql_attr_def_is_atomic(fwdAD) ? 0x1 : 0x0;
    ad->isCollection     = isFullyScalar ? 0x0 : 0x1;
    /* ^^^ we check here the full chain and consider
     *     fwd query as a scalar only if all the chained queries results in
     *     a scalar values */
    
    ad->isFwdQuery       = 0x1;
    ad->staticValueFlags = 0x0;
    ad->isStray          = 0x0;

    if(isFullyScalar) {
        ad->interface.scalar = _hdql_gScalarFwdQueryIFace;
        ad->interface.scalar.definitionData = (hdql_Datum_t) subquery;
    } else {
        ad->interface.collection = _hdql_gCollectionFwdQueryIFace;
        ad->interface.collection.definitionData = (hdql_Datum_t) subquery;
    }

    ad->keyTypeCode = 0x0;
    ad->reserveKeys = _hdql_reserve_keys_list_for_fwd_query;

    ad->typeInfo.fwdQuery = subquery;

    return ad;
}


static hdql_Datum_t
_static_atomic_scalar_dereference( hdql_Datum_t root
        , hdql_Datum_t dynData
        , struct hdql_CollectionKey * key
        , const hdql_Datum_t defData
        , hdql_Context_t ctx ) {
    assert(NULL == dynData);
    assert(NULL == key || NULL == key->pl.datum);
    assert(NULL != defData);
    return (hdql_Datum_t) defData;
}

static void
_static_atomic_scalar_destroy( hdql_Datum_t dynData
        , const hdql_Datum_t defData
        , hdql_Context_t context) {
    assert(dynData == NULL);
    assert(defData != NULL);
    assert(context != NULL);
    hdql_context_free(context, (hdql_Datum_t) defData);
}

struct hdql_AttrDef *
hdql_attr_def_create_static_atomic_scalar_value(
          hdql_ValueTypeCode_t valueType
        , const hdql_Datum_t valueDatum
        , hdql_Context_t context
        ) {
    assert(valueType != 0x0);

    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x1;
    ad->isCollection     = 0x0;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x1;  /* 0x1 means "const extern value" */
    ad->isStray          = 0x0;

    struct hdql_ScalarAttrInterface iface;
    bzero(&iface, sizeof(iface));
    iface.definitionData = valueDatum;
    iface.dereference = _static_atomic_scalar_dereference;
    iface.destroy = _static_atomic_scalar_destroy;

    ad->interface.scalar = iface;

    ad->typeInfo.atomic.isReadOnly = 0x1;
    
    ad->typeInfo.atomic.arithTypeCode = valueType;

    ad->keyTypeCode = 0x0;
    ad->reserveKeys = NULL;

    return ad;
}


struct hdql_AttrDef *
hdql_attr_def_create_dynamic_value(
          hdql_ValueTypeCode_t valueType
        , hdql_Datum_t (*get)(void *, hdql_Context_t)
        , void * userdata
        , hdql_Context_t
        ) {
    assert(0);  // TODO
}

void
hdql_attr_def_set_stray(struct hdql_AttrDef * ad) {
    assert(ad);
    ad->isStray = 0x1;
}

bool
hdql_attr_def_is_atomic(const hdql_AttrDef_t ad) {
    if(ad->isFwdQuery) return false;
    /* if(0x0 != ad->staticValueFlags) return false;  XXX ?! */
    return ad->isAtomic;
}

bool
hdql_attr_def_is_compound(const hdql_AttrDef_t ad) {
    if(ad->isFwdQuery) return false;
    if(0x0 != ad->staticValueFlags) return false;
    return !ad->isAtomic;
}

bool hdql_attr_def_is_scalar(hdql_AttrDef_t ad) { return !ad->isCollection; }
bool hdql_attr_def_is_collection(hdql_AttrDef_t ad) { return ad->isCollection; }
bool hdql_attr_def_is_fwd_query(hdql_AttrDef_t ad) { return ad->isFwdQuery; }
bool hdql_attr_def_is_direct_query(hdql_AttrDef_t ad) { return !ad->isFwdQuery; }
bool hdql_attr_def_is_static_value(hdql_AttrDef_t ad) { return ad->staticValueFlags; }
bool hdql_attr_def_is_stray(hdql_AttrDef_t ad) { return ad->isStray; }

hdql_ValueTypeCode_t
hdql_attr_def_get_atomic_value_type_code(const hdql_AttrDef_t ad) {
    assert(ad->isAtomic || 0x0 != ad->staticValueFlags);  // unguarded type code
    return ad->typeInfo.atomic.arithTypeCode;
}

hdql_ValueTypeCode_t
hdql_attr_def_get_key_type_code(const hdql_AttrDef_t ad) {
    return ad->keyTypeCode;
}

const struct hdql_AtomicTypeFeatures *
hdql_attr_def_atomic_type_info(const hdql_AttrDef_t ad) {
    assert(ad->isAtomic);
    assert(!ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    return &ad->typeInfo.atomic;
}

const struct hdql_Compound *
hdql_attr_def_compound_type_info(const hdql_AttrDef_t ad) {
    assert(!ad->isAtomic);
    assert(!ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    return ad->typeInfo.compound;
}

struct hdql_Query *
hdql_attr_def_fwd_query(const hdql_AttrDef_t ad) {
    assert(ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    return ad->typeInfo.fwdQuery;
}

const struct hdql_ScalarAttrInterface *
hdql_attr_def_scalar_iface(const hdql_AttrDef_t ad) {
    assert(!ad->isCollection);
    return &ad->interface.scalar;
}

const struct hdql_CollectionAttrInterface *
hdql_attr_def_collection_iface(const hdql_AttrDef_t ad) {
    assert(ad->isCollection);
    return &ad->interface.collection;
}

const hdql_Datum_t
hdql_attr_def_get_static_value(const hdql_AttrDef_t ad) {
    assert(0x1 == ad->staticValueFlags);  // TODO: dynamic extern values
    if(hdql_attr_def_is_collection(ad)) {
        return ad->interface.collection.definitionData;
    } else {
        return ad->interface.scalar.definitionData;
    }
}

int
hdql_attr_def_reserve_keys( const hdql_AttrDef_t ad
                          , struct hdql_CollectionKey * key
                          , hdql_Context_t context
                          ) {
    assert(ad);
    assert(key);
    assert(0x0 == ad->keyTypeCode);  /* this routine must be called only if no key code set */
    key->code = 0x0;
    if(NULL == ad->reserveKeys) {
        /* no key reserve method, trivial key or set collection */
        key->isList = 0x0;
        bzero(&(key->pl), sizeof(key->pl));
        return 0;
    }
    if( hdql_attr_def_is_collection(ad) ) {
        hdql_Datum_t suppData
            = hdql_attr_def_is_direct_query(ad)
            ? ad->interface.collection.definitionData
            : (hdql_Datum_t) ad->typeInfo.fwdQuery
            ;
        key->pl.keysList
            = ad->reserveKeys( suppData
                             , context
                             ); 
        if(NULL == key->pl.keysList) return -1;
        key->isList = 0x1;
        return 0;
    }
    if( hdql_attr_def_is_scalar(ad) ) {
        hdql_Datum_t suppData
            = hdql_attr_def_is_direct_query(ad)
            ? ad->interface.collection.definitionData
            : (hdql_Datum_t) ad->typeInfo.fwdQuery
            ;
        key->pl.keysList
            = ad->reserveKeys( suppData
                             , context
                             ); 
        if(NULL == key->pl.keysList) return -1;
        key->isList = 0x1;
        return 0;
    }
    assert(0);  /* bad interface definition -- not a collection, nor a scalar */
    return -2;
}

void
hdql_attr_def_destroy( hdql_AttrDef_t ad
                     , hdql_Context_t ctx
                     ) {
    hdql_context_free(ctx, (hdql_Datum_t) ad);
}

int
hdql_top_attr_str( const struct hdql_AttrDef * subj
              , char * strbuf, size_t buflen
              , hdql_Context_t context
              ) {
    struct hdql_ValueTypes * vts = hdql_context_get_types(context);
    if(NULL == subj || 0 == buflen || NULL == strbuf)
        return HDQL_ERR_BAD_ARGUMENT;
    size_t nUsed = 0;
    #define _M_pr(fmt, ...) \
        nUsed += snprintf(strbuf + nUsed, buflen - nUsed, fmt, __VA_ARGS__); \
        if(nUsed >= buflen - 1) return 1;
    // query 0x23fff34 is "[static ](collection|scalar) [of [atomic|compound]type] "
    _M_pr("%s%s%s "
            , hdql_attr_def_is_static_value(subj) ? "static " : ""
            , hdql_attr_def_is_collection(subj) ? "collection" : "scalar"
            , hdql_attr_def_is_fwd_query(subj)
              ? " query result of query forwarding to "
              : ( hdql_attr_def_is_atomic(subj)
                ? " of atomic type"
                : ( hdql_compound_is_virtual(hdql_attr_def_compound_type_info(subj))
                  ? " of virtual compound type"
                  : " of compound type"
                  )
                )
            );
    if(hdql_attr_def_is_fwd_query(subj)) {
        // query 0x23fff34 is "[static ](collection|scalar) forwarding to %p which is ..."
        _M_pr("%p which is ", hdql_attr_def_fwd_query(subj) );
        return hdql_top_attr_str( hdql_query_get_subject(hdql_attr_def_fwd_query(subj))
                , strbuf + nUsed, buflen - nUsed, context );
    }
    if(hdql_attr_def_is_atomic(subj)) {
        // "[static ](collection|scalar) of atomic type <%s>"
        assert(vts);
        hdql_ValueTypeCode_t vtc = hdql_attr_def_get_atomic_value_type_code(subj);
        if(0x0 != vtc) {
            const struct hdql_ValueInterface * vi
                = hdql_types_get_type( vts
                                     , hdql_attr_def_get_atomic_value_type_code(subj)
                                     );
            assert(NULL != vi);
            _M_pr("<%s>", vi->name);
            if(hdql_attr_def_is_static_value(subj)) {
                assert(vi);
                // "[static ](collection|scalar) of atomic type <%s> [=%s|at %p]"
                if(vi->get_as_string) {
                    char vBf[64];
                    int rc = vi->get_as_string(hdql_attr_def_get_static_value(subj)
                                , vBf, sizeof(vBf), context);
                    if(0 == rc) {
                        _M_pr(" =%s", vBf);
                    } else {
                        _M_pr(" =? at %p", hdql_attr_def_get_static_value(subj));
                    }
                } else {
                    _M_pr(" at %p", hdql_attr_def_get_static_value(subj));
                }
            }
        } else {
            _M_pr("?%p?", subj);
        }
    } else {
        char buf[256];
        hdql_compound_get_full_type_str(hdql_attr_def_compound_type_info(subj), buf, sizeof(buf));
        // query 0x23fff34 is "[static ](collection|scalar) of [virtual] compound type [based on] <%s>"
        _M_pr("<%s>", buf
                //, hdql_compound_is_virtual(hdql_attr_def_compound_type_info(q->subject))
                //? "based on "
                //: ""
                //, hdql_compound_get_name(hdql_attr_def_compound_type_info(q->subject))
            );
    }
    #undef _M_pr
    return 0;
}

