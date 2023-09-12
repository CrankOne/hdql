#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"

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
        struct {
            hdql_ValueTypeCode_t typeCode;
            union {
                /**\brief Extern const value (e.g. numeric literal, math constant, etc)*/
                hdql_Datum_t staticValue;
                /**\brief Extern value that can change during lifetime */
                struct {
                    const void * userdata;
                    hdql_Datum_t (*get)(void * userdata, hdql_Context_t);
                } dynamicValue;
            } pl;
        } staticValue;
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
    ad->isCollection     = 0x1;
    ad->isFwdQuery       = 0x0;
    ad->staticValueFlags = 0x0;

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

    ad->interface.collection = *interface;
    ad->typeInfo.compound = typeInfo;
    ad->keyTypeCode = keyTypeCode;
    ad->reserveKeys = keyIFace;

    return ad;
}

struct hdql_AttrDef *
hdql_attr_def_create_fwd_query(
          struct hdql_Query * subquery
        , hdql_Context_t context
        ) {
    assert(0);  // TODO
}

struct hdql_AttrDef *
hdql_attr_def_create_static_value(
          hdql_ValueTypeCode_t valueType
        , const hdql_Datum_t valueDatum
        , hdql_Context_t
        ) {
    assert(0);  // TODO, shall make copy
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

bool
hdql_attr_def_is_atomic(const hdql_AttrDef_t ad) {
    if(ad->isFwdQuery) return false;
    if(0x0 != ad->staticValueFlags) return false;
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


hdql_ValueTypeCode_t
hdql_attr_def_get_atomic_value_type_code(const hdql_AttrDef_t ad) {
    assert(ad->isAtomic || 0x0 != ad->staticValueFlags);  // unguarded type code
    if(ad->staticValueFlags) {
        return ad->typeInfo.staticValue.typeCode;
    } else {
        return ad->typeInfo.atomic.arithTypeCode;
    }
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
    assert(!ad->isAtomic);
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
hdql_attr_def_get_static_value(const hdql_AttrDef_t) {
    assert(0);  // TODO
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
        key->pl.keysList
            = ad->reserveKeys(ad->interface.collection.definitionData, context); 
        assert(key->pl.keysList);
        key->isList = 0x1;
        return 0;
    }
    if( hdql_attr_def_is_scalar(ad) ) {
        key->pl.keysList
            = ad->reserveKeys(ad->interface.scalar.definitionData, context); 
        assert(key->pl.keysList);
        key->isList = 0x1;
        return 0;
    }
    assert(0);  /* bad interface definition -- not a collection, nor a scalar */
    return -1;
}

void
hdql_attr_def_destroy( hdql_AttrDef_t ad
                     , hdql_Context_t ctx
                     ) {
    if(hdql_attr_def_is_fwd_query(ad)) {
        assert(0);  // TODO: query_destroy
    }
    hdql_context_free(ctx, (hdql_Datum_t) ad);
}

