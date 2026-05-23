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
     *  0x1 -- constant value, usually a literal (parser calculates simple
     *         arithmetics); can be some public constant as well (e.g. pi,
     *         Avogadro number, etc);
     *  0x2 -- externally set variable (no evaluation applied during the parsing) */
    unsigned int staticValueFlags:2;
    /** If set, scalar or collection iface's def. data is dynamic and has to
     * be deleted along with attr. def */
    unsigned int isTransient:1;
 
    /**\brief Key type code, can be zero (unset) */
    hdql_ValueTypeCode_t keyTypeCode:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;

    /** Data access interface for a value */
    union {
        struct hdql_ScalarAttrInterface     scalar;
        struct hdql_CollectionAttrInterface collection;
    } interface;

    /**\brief Keys list allocation callback */
    hdql_ReserveKeysListCallback_t reserve_key;

    /** Defines value type features */
    union {
        struct hdql_AtomicTypeFeatures   atomic;
        struct hdql_Compound           * compound;
    } typeInfo;

    void (*transient_dtr)(hdql_Datum_t, hdql_Context_t ctx);
};  /* struct hdql_AttrDef */

/*                          * * *   * * *   * * *                            */

/* internal API */
bool hdql__attr_def_is_fwd_query(hdql_AttrDef_t ad) { return ad->isFwdQuery; }

struct hdql_Query * hdql__attr_def_fwd_query(const struct hdql_AttrDef * ad) {
    assert(ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    if(hdql_attr_def_is_collection(ad)) {
        return (struct hdql_Query *) hdql_attr_def_collection_iface(ad)->definitionData;
    } else {
        return (struct hdql_Query *) hdql_attr_def_scalar_iface(ad)->definitionData;
    }
}

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
    ad->isTransient      = 0x0;

    ad->interface.scalar = *interface;
    ad->typeInfo.atomic = *typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserve_key = keyIFace;

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
    ad->isTransient      = 0x0;

    ad->interface.collection = *interface;
    ad->typeInfo.atomic = *typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserve_key = keyIFace;

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
    ad->isTransient      = 0x0;

    ad->interface.scalar = *interface;
    ad->typeInfo.compound = typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserve_key = keyIFace;

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
    ad->isTransient      = 0x0;

    ad->interface.collection = *interface;
    ad->typeInfo.compound = typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserve_key = keyIFace;

    return ad;
}

/* Forwarding attribute definition
 */

/* Key allocation callback for forwarding query */
static int _hdql_reserve_key_for_fwd_query(
          struct hdql_Key * k
        , const struct hdql_Datum *fwdQ_
        , hdql_Context_t context
        ) {
    struct hdql_Query * sq = (struct hdql_Query *) fwdQ_;
    int rc = hdql_key_reserve_for_query(sq, k, context);
    assert(hdql_key_is_list(k));
    return rc;
}

/* Destructor for attribute definition's static data when AD is forwarding
 * query. Invokes destruction of associated query. */
static void _transient_dtr__fwd_query(hdql_Datum_t d, hdql_Context_t ctx) {
    if(d) {
        hdql_query_destroy((struct hdql_Query *) d, ctx);
    }
}

struct hdql_AttrDef *
hdql_attr_def_create_fwd_query(
          struct hdql_Query * subquery
        , hdql_Context_t context
        , int * rc
        ) {
    assert(rc);
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    if(!ad) {
        *rc = HDQL_ERR_MEMORY;
        return NULL;
    }
    const struct hdql_AttrDef * fwdAD = hdql_query_top_attr(subquery);
    bool isFullyScalar = hdql_query_is_fully_scalar(subquery);
    /* ^^^ here we check here the full chain and consider
     *     fwd query as a scalar only if all the chained queries results in
     *     a scalar values */

    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    /* atomic/collection flags are inherited from what the fwded
     * query is supposed to fetch */
    ad->isAtomic         = hdql_attr_def_is_atomic(fwdAD) ? 0x1 : 0x0;
    ad->isCollection     = isFullyScalar ? 0x0 : 0x1;
    
    ad->isFwdQuery       = 0x1;
    ad->staticValueFlags = 0x0;
    ad->isTransient      = 0x0;  /* overridden below */

    if(isFullyScalar) {
        ad->interface.scalar = _hdql_gScalarFwdQueryIFace;
        ad->interface.scalar.definitionData = (hdql_Datum_t) subquery;
    } else {
        ad->interface.collection = _hdql_gCollectionFwdQueryIFace;
        ad->interface.collection.definitionData = (hdql_Datum_t) subquery;
    }

    hdql_attr_def_set_transient(ad, _transient_dtr__fwd_query);

    ad->keyTypeCode = 0x0;
    ad->reserve_key = _hdql_reserve_key_for_fwd_query;

    return ad;
}

/* Static atomic scalar AD
 */

static hdql_Datum_t
_static_atomic_scalar_reset( hdql_Datum_t root
        , hdql_Datum_t dynData
        , const struct hdql_Datum *defData
        , struct hdql_Key * key
        , hdql_Context_t ctx
        ) {
    ((void) root);
    ((void) ctx);
    ((void) key);
    assert(NULL == dynData);
    assert(NULL != defData);
    return (hdql_Datum_t) defData;
}

static void
_transient_dtr__static_atomic(hdql_Datum_t datum, hdql_Context_t ctx) {
    hdql_context_free(ctx, datum);
}

struct hdql_AttrDef *
hdql_attr_def_create_static_atomic_scalar_value(
          hdql_ValueTypeCode_t valueType
        , const struct hdql_Datum *valueDatum
        , hdql_Context_t context
        ) {
    assert(valueType != 0x0);

    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    bzero((void*) ad, sizeof(struct hdql_AttrDef));

    ad->isAtomic         = 0x1;
    ad->isCollection     = 0x0;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x1;  /* 0x1 means "const extern value" */
    ad->isTransient      = 0x0;

    struct hdql_ScalarAttrInterface iface;
    bzero(&iface, sizeof(iface));
    iface.reset = _static_atomic_scalar_reset;
    iface.definitionData = valueDatum;
    iface.destroy_dyn_data = NULL;

    ad->interface.scalar = iface;

    ad->typeInfo.atomic.isReadOnly = 0x1;
    
    ad->typeInfo.atomic.arithTypeCode = valueType;

    ad->keyTypeCode = 0x0;
    ad->reserve_key = NULL;

    /* static atomic scalars are implicitly always transient */
    hdql_attr_def_set_transient(ad, _transient_dtr__static_atomic);

    return ad;
}

static void _transient_dtr__binding_query(hdql_Datum_t d, hdql_Context_t ctx) {
    hdql_bound_value_interface_definition_data_destroy(d, ctx);
}

struct hdql_AttrDef *
hdql_attr_def_create_bound(
          struct hdql_Query * subquery
        , hdql_Context_t context
        , int * rc
        ) {
    assert(!hdql_query_is_fully_scalar(subquery));
    *rc = 0;
    const struct hdql_AttrDef * qTopAD = hdql_query_top_attr(subquery);
    assert(qTopAD);
    /* Bound attribute addresses external value provided by the query which is
     * managed outside the compound this request is applied to. */
    struct hdql_AttrDef * ad = hdql_alloc(context, struct hdql_AttrDef);
    if(!ad) {
        *rc = HDQL_ERR_MEMORY;
        return NULL;
    }

    /* `is-atomic' prop is inherited */
    ad->isAtomic = qTopAD->isAtomic;
    /* result is always a scalar */
    ad->isCollection = 0x0;
    /* bound is NOT forwarding */
    ad->isFwdQuery = 0x0;
    /* static value is inherited (TODO: verify) */
    ad->staticValueFlags = qTopAD->staticValueFlags;
    /* always transient */
    ad->isTransient = 0x1;
    ad->transient_dtr = _transient_dtr__binding_query;
    /* attribute has no key  */
    ad->keyTypeCode = 0x0;
    ad->reserve_key = NULL;
    /* inherit type info as is */
    memcpy(&ad->typeInfo, &qTopAD->typeInfo, sizeof(qTopAD->typeInfo));

    /* set interface to bound shim, initialized with query */
    ad->interface.scalar = _hdql_gBoundQueryIFace;
    ad->interface.scalar.definitionData
        = hdql_bound_value_interface_definition_data_init(subquery, context);
    
    return ad;
}

struct hdql_AttrDef *
hdql_attr_def_create_dynamic_value(
          hdql_ValueTypeCode_t valueType
        , hdql_Datum_t (*get)(void *, hdql_Context_t)
        , void * userdata
        , hdql_Context_t context
        ) {
    ((void) valueType);
    ((void) get);
    ((void) userdata);
    ((void) context);
    #warning "TODO: support external values"
    return NULL;
}

void
hdql_attr_def_set_transient(struct hdql_AttrDef * ad
        , void (*dtr)(hdql_Datum_t, hdql_Context_t) ) {
    assert(ad);
    ad->isTransient = 0x1;
    ad->transient_dtr = dtr;
}

bool
hdql_attr_def_is_atomic(const struct hdql_AttrDef * ad) {
    if(ad->isFwdQuery) return false;
    /* if(0x0 != ad->staticValueFlags) return false;  XXX ?! */
    return ad->isAtomic;
}

bool
hdql_attr_def_is_compound(const struct hdql_AttrDef * ad) {
    if(ad->isFwdQuery) return false;
    if(0x0 != ad->staticValueFlags) return false;
    return !ad->isAtomic;
}

bool hdql_attr_def_is_scalar(hdql_AttrDef_t ad) { return !ad->isCollection; }
bool hdql_attr_def_is_collection(hdql_AttrDef_t ad) { return ad->isCollection; }
bool hdql_attr_def_is_static_const_value(hdql_AttrDef_t ad) { return ad->staticValueFlags == 0x1; }
bool hdql_attr_def_is_static_external_value(hdql_AttrDef_t ad) { return ad->staticValueFlags == 0x2; }
bool hdql_attr_def_is_transient(hdql_AttrDef_t ad) { return ad->isTransient; }

bool hdql_attr_def_is_bound(hdql_AttrDef_t ad) {
    if(!hdql_attr_def_is_scalar(ad)) return false;
    /* ^^^ we do not have such thing as collection of collections in HDQL */
    const struct hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(ad);
    /* TODO: can we have something more elegant than this here? */
    if( iface->reset == _hdql_gBoundQueryIFace.reset ) {
        assert(iface->new_dyn_data == _hdql_gBoundQueryIFace.new_dyn_data );
        assert(iface->destroy_dyn_data == _hdql_gBoundQueryIFace.destroy_dyn_data );
        return true;
    }
    return false;
}

hdql_ValueTypeCode_t
hdql_attr_def_get_atomic_value_type_code(const struct hdql_AttrDef * ad) {
    assert(ad->isAtomic || 0x0 != ad->staticValueFlags);  // unguarded type code
    return ad->typeInfo.atomic.arithTypeCode;
}

hdql_ValueTypeCode_t
hdql_attr_def_get_key_type_code(const struct hdql_AttrDef * ad) {
    return ad->keyTypeCode;
}

const struct hdql_AtomicTypeFeatures *
hdql_attr_def_atomic_type_info(const struct hdql_AttrDef * ad) {
    assert(ad->isAtomic);
    assert(!ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    return &ad->typeInfo.atomic;
}

const struct hdql_Compound *
hdql_attr_def_compound_type_info(const struct hdql_AttrDef * ad) {
    assert(!ad->isAtomic);
    assert(!ad->isFwdQuery);
    assert(0x0 == ad->staticValueFlags);
    return ad->typeInfo.compound;
}

const struct hdql_ScalarAttrInterface *
hdql_attr_def_scalar_iface(const struct hdql_AttrDef * ad) {
    assert(!ad->isCollection);
    return &ad->interface.scalar;
}

const struct hdql_CollectionAttrInterface *
hdql_attr_def_collection_iface(const struct hdql_AttrDef * ad) {
    assert(ad->isCollection);
    return &ad->interface.collection;
}

const struct hdql_Datum *
hdql_attr_def_get_static_value(const struct hdql_AttrDef * ad) {
    assert(0x1 == ad->staticValueFlags);  /* const static, otherwise TODO: dynamic extern values */
    if(hdql_attr_def_is_collection(ad)) {
        return ad->interface.collection.definitionData;
    } else {
        return ad->interface.scalar.definitionData;
    }
}

int
hdql_attr_def_reserve_key( const struct hdql_AttrDef * ad
                         , struct hdql_Key * key
                         , hdql_Context_t context
                         ) {
    assert(ad);
    assert(key);
    assert(0x0 == ad->keyTypeCode);  /* this routine must be called only if no key code set */
    #if 0
    key->code = 0x0;
    if(NULL == ad->reserveKeys) {
        /* no key reserve method, trivial key or set collection */
        key->isList = 0x0;
        bzero(&(key->pl), sizeof(key->pl));
        return 0;
    }
    if( hdql_attr_def_is_collection(ad) ) {
        key->pl.keysList
            = ad->reserveKeys( ad->interface.collection.definitionData
                             , context
                             ); 
        if(NULL == key->pl.keysList) return -1;
        key->isList = 0x1;
        return 0;
    }
    if( hdql_attr_def_is_scalar(ad) ) {
        key->pl.keysList
            = ad->reserveKeys( ad->interface.collection.definitionData
                             , context
                             ); 
        if(NULL == key->pl.keysList) return -1;
        key->isList = 0x1;
        /* ^^^ we're in scalar AD, why do we assign a list flag here?!
         *     this is sick... see issue #14 */
        return 0;
    }
    #else
    if(NULL == ad->reserve_key) {
        /* no key reserve method, trivial key or set collection */
        hdql_key_mark_empty(key);
        return HDQL_ERR_CODE_OK;
    }
    if( hdql_attr_def_is_collection(ad) ) {
        ad->reserve_key(key, ad->interface.collection.definitionData, context);
    } else {
        assert(hdql_attr_def_is_scalar(ad));
        ad->reserve_key(key, ad->interface.scalar.definitionData, context);
    }
    return HDQL_ERR_CODE_OK;
    #endif
}

hdql_AttrDef_t
hdql_attr_def_top_attr(const struct hdql_AttrDef * ad) {
    assert(ad);
    const struct hdql_AttrDef * topAD = ad;
    while(hdql__attr_def_is_fwd_query(topAD)) {
        topAD = hdql_query_top_attr(hdql__attr_def_fwd_query(topAD));
    }
    return topAD;
}

void
hdql_attr_def_destroy( hdql_AttrDef_t ad, hdql_Context_t ctx) {
    if(hdql_attr_def_is_transient(ad)) {
        /* "transient" attribute means that it manages its interface definition
         * data */
        if(hdql_attr_def_is_collection(ad) && ad->transient_dtr) {
            ad->transient_dtr((hdql_Datum_t) ad->interface.collection.definitionData, ctx);
        } else if(hdql_attr_def_is_scalar(ad) && ad->transient_dtr) {
            ad->transient_dtr((hdql_Datum_t) ad->interface.scalar.definitionData, ctx);
        }
        #ifndef NDEBUG
        else if(ad->transient_dtr) {
        assert(0);  /* transient AD withoud def. data? */
        }
        #endif
    }
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
            , hdql_attr_def_is_static_const_value(subj) ? "const.static " : ""
            , hdql_attr_def_is_collection(subj) ? "collection" : "scalar"
            , hdql__attr_def_is_fwd_query(subj)
              ? " query result of query forwarding to "
              : ( hdql_attr_def_is_atomic(subj)
                ? " of atomic type"
                : ( hdql_compound_is_virtual(hdql_attr_def_compound_type_info(subj))
                  ? " of virtual compound type"
                  : " of compound type"
                  )
                )
            );
    if(hdql__attr_def_is_fwd_query(subj)) {
        // query 0x23fff34 is "[static ](collection|scalar) forwarding to %p which is ..."
        _M_pr("%p which is ", (void*) hdql__attr_def_fwd_query(subj) );
        return hdql_top_attr_str( hdql_query_get_subject(hdql__attr_def_fwd_query(subj))
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
            if(hdql_attr_def_is_static_const_value(subj)) {
                assert(vi);
                // "[static ](collection|scalar) of atomic type <%s> [=%s|at %p]"
                if(vi->get_as_string) {
                    char vBf[64];
                    int rc = vi->get_as_string(hdql_attr_def_get_static_value(subj)
                                , vBf, sizeof(vBf), context);
                    if(0 == rc) {
                        _M_pr(" =%s", vBf);
                    } else {
                        _M_pr(" =? at %p", (void*) hdql_attr_def_get_static_value(subj));
                    }
                } else {
                    _M_pr(" at %p", (void*) hdql_attr_def_get_static_value(subj));
                }
            }
        } else {
            _M_pr("?%p?", (void*) subj);
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

