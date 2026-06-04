#include "hdql/value.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/internal-api.h"
#include "hdql/types.h"
#include "hdql/util/allocator.h"
#include "hdql/util/avl-tree.h"
#include "hdql/util/ht.h"

#include <assert.h>
#include <string.h>
#include <math.h>

#ifndef HDQL_HT_DEFAULT_VALUES_NTYPES
#   define HDQL_HT_DEFAULT_VALUES_NTYPES 4
#endif

#ifndef HDQL_TIER_BITSIZE
/**\brief Defines number of bits used to store the type table's tier */
#   define HDQL_TIER_BITSIZE 4
#endif

#if HDQL_VALUE_TYPEDEF_CODE_BITSIZE <= HDQL_TIER_BITSIZE
/* One have to fix it either by increasing `HDQL_VALUE_TYPEDEF_CODE_BITSIZE`
 * in the project's types.h, or by decreasing `HDQL_TIER_BITSIZE` specified
 * for this file */
#   error("Bit size of type table tier is greater than type code bitsize.")
#endif

static const hdql_ValueTypeCode_t gMaxTypeID = HDQL_VALUE_TYPE_CODE_MAX;
static const hdql_ValueTypeCode_t gMaxTypeIDInTier
        = ((0x1 << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE)) - 1);
static const hdql_ValueTypeCode_t gTierMask
        = HDQL_VALUE_TYPE_CODE_MAX & (~gMaxTypeIDInTier);

static uint8_t
_get_tier_number(hdql_ValueTypeCode_t code) {
    return (code & gTierMask) >> (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
}

/* Internaly represents table: (code:hdql_ValueTypeCode_t, name:char*, typeDef:hdql_ValueInterface*)
 *
 * CAVEAT: a little hacky -- codes are stored in has table as integers
 * written in pointer field (not pointer referencing number).
 * */
struct hdql_ValueTypes {
    struct hdql_ValueTypes *parent;
    /* code by name hash table
     * NOTE: code number is stored as pointer; never try to de-reference it! */
    struct hdql_ht *codeByName;
    /* value interface by code */
    struct hdql_umap *itemByCode;
    /* current types tier, root is 0, max is 32 (4 bits) */
    uint8_t tier;
    /* type promotions table; invalidated on type insertion */
    struct hdql_ArithTypePromotionTable *promotions;
    /* flag to track type promotions table validity */
    bool isPromotionsValid;
};  /* struct hdql_ValueTypes */


int
hdql_types_define( struct hdql_ValueTypes *vt
        , const struct hdql_ValueInterface *vti
        , hdql_ValueTypeCode_t *tcPtr
        ) {
    vt->isPromotionsValid = false;

    /* NOTE: 0 size can be reserved for variadic types in future */
    if(NULL == vti->name || *vti->name == '\0' || vti->size < 0) {
        return HDQL_ERR_BAD_ARGUMENT;
    }
    size_t nItems = hdql_umap_size(vt->itemByCode);
    if(nItems >= gMaxTypeIDInTier) {
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;  /* IDs exceed for tier */
    }

    /* generate type code entry */
    hdql_ValueTypeCode_t tCode = (hdql_ValueTypeCode_t) nItems;
    tCode |= ((hdql_ValueTypeCode_t) vt->tier) << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
    assert(tCode);  /* 0x0 avoided since tier is never 0 */
    if(tCode > gMaxTypeID) return HDQL_ERR_GENERIC;

    /* pre-caution -- ensure the code is unique */
    void *byCodeEntry = hdql_umap_get(vt->itemByCode, tCode);
    if(byCodeEntry)
        return HDQL_ERR_GENERIC;  /* internal inconsistency: generated code exists */

    /* add name-to-code entry */
    const size_t nameLen1 = strlen(vti->name) + 1;
    size_t nb; struct hdql_htEntry * entry; {
        entry = hdql_ht_lookup(vt->codeByName, (const unsigned char *) vti->name
                    , nameLen1, &nb);
        if(entry) return HDQL_ERR_NAME_COLLISION;  /* Type name exists in current context */
        int rc = hdql_ht_ins_cached(vt->codeByName
                , (const unsigned char *) vti->name, nameLen1
                , (void *) ((unsigned long) tCode), entry, nb);
        switch(rc) {
            case HDQL_HT_RC_INSERTED:
                break;
            case HDQL_HT_ERR_MEM:
                return HDQL_ERR_MEMORY;
            default:
                return HDQL_ERR_GENERIC;
        }
    }

    /* add code-to-type record */
    struct hdql_ValueInterface * vtiCpy; {
        const struct hdql_Allocator *alloc = hdql_umap_get_alloc(vt->itemByCode);
        vtiCpy = alloc->alloc(sizeof(struct hdql_ValueInterface), alloc->userdata);
        /* failed to allocate interface copy struct */
        if(!vtiCpy) {
            hdql_ht_erase(vt->codeByName, entry, nb, NULL);
            return HDQL_ERR_MEMORY;
        }
        memcpy(vtiCpy, vti, sizeof(struct hdql_ValueInterface));
        vtiCpy->name = alloc->alloc(nameLen1, alloc->userdata);
        if(!vtiCpy->name) {
            hdql_ht_erase(vt->codeByName, entry, nb, NULL);
            alloc->free(vtiCpy, alloc->userdata);
            return HDQL_ERR_MEMORY;
        }
        memcpy((char *) vtiCpy->name, vti->name, nameLen1);
    }
    int rc = hdql_umap_insert(vt->itemByCode, tCode, vtiCpy);
    int myRC = HDQL_ERR_CODE_OK;
    switch(rc) {
        case HDQL_AVL_OK:
            if(tcPtr) *tcPtr = tCode;
            return myRC;

        case HDQL_AVL_MEM_ERROR:
            myRC = HDQL_ERR_MEMORY;
        case HDQL_AVL_BAD_ARG_ERROR:
            myRC = HDQL_ERR_BAD_ARGUMENT;
        case HDQL_AVL_CHANGED:
            myRC = HDQL_ERR_GENERIC;
        default:
            hdql_ht_remove(vt->codeByName, (const unsigned char *) vti->name, nameLen1, NULL);
            return myRC;
    }
}

int
hdql_types_alias(
          struct hdql_ValueTypes * vt
        , const char * alias
        , hdql_ValueTypeCode_t tgtCode
        ) {
    vt->isPromotionsValid = false;
    assert(0 != tgtCode);  /* we assume user code controlled it */
    uint8_t tierNo = _get_tier_number(tgtCode);
    assert(tierNo);
    if(tierNo > vt->tier) return HDQL_ERR_GENERIC;  /* type is from children context */

    assert(tierNo == vt->tier || vt->parent);  /* logic error (tierNo must be zero in case this assertion failed) */
    /* assure type exist in parent tables */
    const struct hdql_ValueInterface *vti
            = hdql_types_get_type(tierNo < vt->tier ? vt->parent : vt, tgtCode);
    if(!vti) return HDQL_ERR_UNKNOWN_ATTRIBUTE;  /* no target type found in parent*/

    const size_t nameLen = strlen(alias) + 1;
    size_t nb;
    struct hdql_htEntry *entry = hdql_ht_lookup( vt->codeByName
            , (const unsigned char *) alias, nameLen
            , &nb);
    if(entry) {
        if( ((unsigned long) tgtCode) == ((unsigned long) entry) )
            return HDQL_ERR_CODE_OVERRIDDEN;  /* same alias added more than once */
        return HDQL_ERR_NAME_COLLISION;  /* same name, different tgt */
    }
    int rc = hdql_ht_ins_cached( vt->codeByName
            , (const unsigned char *) alias, nameLen
            , (void *) ((unsigned long) tgtCode)
            , entry
            , nb);
    switch(rc) {
        case HDQL_HT_RC_INSERTED:
            return HDQL_ERR_CODE_OK;
        case HDQL_HT_ERR_MEM:
            return HDQL_ERR_MEMORY;
        default:
            return HDQL_ERR_GENERIC;
    }
}

const struct hdql_ValueInterface *
hdql_types_get_type_by_name(const struct hdql_ValueTypes *vt, const char *nm) {
    hdql_ValueTypeCode_t tc = hdql_types_get_type_code(vt, nm);
    if(!tc) {
        if(!vt->parent) return NULL;
        return hdql_types_get_type_by_name(vt->parent, nm);
    }
    return hdql_types_get_type(vt, tc);
}

const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc) {
    const struct hdql_ValueInterface *r
        = (struct hdql_ValueInterface *) hdql_umap_get(vt->itemByCode, tc);
    if(r) return r;
    if(!vt->parent) return NULL;
    return hdql_types_get_type(vt->parent, tc);
}

hdql_ValueTypeCode_t
hdql_types_get_type_code(const struct hdql_ValueTypes * vt, const char * name) {
    void *code_ = hdql_ht_get(vt->codeByName, (const unsigned char *) name, strlen(name) + 1);
    if(NULL == code_) {
        if(!vt->parent) return 0x0;
        return hdql_types_get_type_code(vt->parent, name);
    }
    return (hdql_ValueTypeCode_t) ((unsigned long) code_);
}

/*                                          __________________________________
 * _______________________________________/ Context-private types table mgmnt
 */

/* NOT exposed to public header */
struct hdql_ValueTypes *
hdql__value_types_table_create(struct hdql_ValueTypes *parent
        , hdql_Context_t context) {
    struct hdql_ValueTypes *r
        = (struct hdql_ValueTypes *) hdql_context_alloc(context, sizeof(struct hdql_ValueTypes));
    bzero(r, sizeof(struct hdql_ValueTypes));
    r->tier = parent ? parent->tier + 1 : 1;
    r->parent = parent;
    r->isPromotionsValid = false;

    const struct hdql_Allocator *alloc = hdql__context_get_allocator(context);
    if(!(r->codeByName = hdql_ht_create(alloc
            , HDQL_HT_DEFAULT_VALUES_NTYPES, HDQL_MURMUR3_32_DEFAULT_SEED)))
        goto onFailure;
    if(!(r->itemByCode = hdql_umap_create(alloc)))
        goto onFailure;
    if(!(r->promotions = hdql_arith_type_promotion_create(context)))
        goto onFailure;
    return r;
onFailure:
    if(r->codeByName) hdql_ht_destroy(r->codeByName);
    if(r->itemByCode) hdql_umap_destroy(r->itemByCode);
    /*if(r->promotions) hdql_arith_type_promotion_destroy(context, r->promotions);*/
    return NULL;
}

static int hdql__delete_type_def(const unsigned long key, void **value, void *alloc_) {
    if((!value) || (!*value)) return 0;
    struct hdql_Allocator *alloc = (struct hdql_Allocator *) alloc_;
    struct hdql_ValueInterface *vti = (struct hdql_ValueInterface *) *value;
    alloc->free((char *) vti->name, alloc->userdata);
    alloc->free(vti, alloc->userdata);
    *value = NULL;
    return 0;
}

void
hdql__value_types_table_destroy(struct hdql_ValueTypes *vt, hdql_Context_t context) {
    if(vt->codeByName) hdql_ht_destroy(vt->codeByName);
    if(vt->itemByCode) {
        const struct hdql_Allocator *alloc = hdql_umap_get_alloc(vt->itemByCode);
        hdql_umap_iter(vt->itemByCode, hdql__delete_type_def, (void *) alloc);
        hdql_umap_destroy(vt->itemByCode);
    }
    if(vt->promotions) hdql_arith_type_promotion_destroy(context, vt->promotions);
    hdql_context_free(context, (hdql_Datum_t) vt);
}

/*                                              ______________________________
 * ___________________________________________/ Typed value creation/deletion
 */

hdql_Datum_t
hdql_create_value(hdql_ValueTypeCode_t tc, hdql_Context_t context, int *rc) {
    struct hdql_ValueTypes * vtx = hdql_context_get_types(context);
    if(NULL == vtx) return NULL;
    const struct hdql_ValueInterface * vti = hdql_types_get_type(vtx, tc);
    if(NULL == vti) return NULL;
    if(vti->size > 0) {
        /* const-size data type */
        hdql_Datum_t r = hdql_context_alloc(context, vti->size);
        if(!r) return NULL;
        if(vti->init) {
            int rc_ = vti->init(r, vti->size, context);
            if(rc_ != 0) {
                hdql_context_free(context, r);
                if(rc) *rc = rc_;
                return NULL;
            }
        }
        return r;
    } else {
        assert(false);  /* TODO: variadic length type */
        return NULL;
    }
}

int
hdql_destroy_value(hdql_ValueTypeCode_t tc, hdql_Datum_t r, hdql_Context_t context, int *rc) {
    struct hdql_ValueTypes * vtx = hdql_context_get_types(context);
    if(NULL == vtx) return HDQL_ERR_GENERIC;
    const struct hdql_ValueInterface * vti = hdql_types_get_type(vtx, tc);
    if(NULL == vti) return HDQL_ERR_GENERIC;
    if(vti->size > 0) {
        if(vti->destroy) {
            int rc_ = vti->destroy(r, vti->size, context);
            if(rc_ != 0 && rc) *rc = rc_;
        }
        hdql_context_free(context, r);
    } else {
        assert(false);  /* TODO: variadic length type */
        return HDQL_ERR_NO_KEY_SUPPORT;
    }
    return 0;
}

hdql_ValueTypeCode_t
hdql_types_numeric_promote(const struct hdql_ValueTypes * vt
        , hdql_ValueTypeCode_t a, hdql_ValueTypeCode_t b) {
    if(!vt->isPromotionsValid) {
        assert(vt->promotions);
        hdql_arith_type_promotion_rebuild(vt, vt->promotions);
        ((struct hdql_ValueTypes *) vt)->isPromotionsValid = true;
    }
    return hdql_arith_type_promote(vt->promotions, a, b);
}

/*                                                            ________________
 * _________________________________________________________/ Constant values
 */

typedef struct {
    enum hdql_ExternValueType type;
    union {
        hdql_Int_t asInt;
        hdql_Flt_t asFlt;
    } value;
} ConstValItem;

static void cvi_init_flt(ConstValItem *instance, hdql_Flt_t value) {
    instance->type = hdql_kExternValFltType;
    instance->value.asFlt = value;
}

static void cvi_init_int(ConstValItem *instance, hdql_Flt_t value) {
    instance->type = hdql_kExternValIntType;
    instance->value.asInt = value;
}

struct hdql_Constants {
    struct hdql_Constants *parent;
    struct hdql_ht *values;

    //hdql_Constants(hdql_Constants * parent_) : parent(parent_) {}  // TODO
};

int
hdql_constants_define_float( struct hdql_Constants * consts
                           , const char * name
                           , hdql_Flt_t value ) {
    assert(consts);
    if(!name || '\0' == *name) {  /* TODO: check name for general validity */
        return HDQL_ERR_BAD_ARGUMENT;
    }

    size_t nb;
    const size_t nameLen = strlen(name);
    struct hdql_htEntry *entry = hdql_ht_lookup( consts->values
            , (const unsigned char *) name, nameLen, &nb);
    if(entry) return HDQL_ERR_NAME_COLLISION;

    const struct hdql_Allocator *alloc = hdql_ht_get_alloc(consts->values);
    ConstValItem *csvi = alloc->alloc(sizeof(ConstValItem), alloc->userdata);
    if(!csvi) return HDQL_ERR_MEMORY;

    cvi_init_flt(csvi, value);

    int htRC = hdql_ht_ins_cached(consts->values, (const unsigned char *) name
            , nameLen, csvi, entry, nb);

    switch(htRC) {
        case HDQL_HT_RC_INSERTED:
            return HDQL_ERR_CODE_OK;

        case HDQL_HT_ERR_MEM:
            alloc->free(csvi, alloc->userdata);
            return HDQL_ERR_MEMORY;
        default:
            return HDQL_ERR_GENERIC;  /* must not happen; leaves undefined state */
    }
}

int
hdql_constants_define_int( struct hdql_Constants * consts
                           , const char * name
                           , hdql_Int_t value ) {
    assert(consts);
    if(!name || '\0' == *name) {  /* TODO: check name for general validity */
        return HDQL_ERR_BAD_ARGUMENT;
    }

    size_t nb;
    const size_t nameLen = strlen(name);
    struct hdql_htEntry *entry = hdql_ht_lookup( consts->values
            , (const unsigned char *) name, nameLen, &nb);
    if(entry) return HDQL_ERR_NAME_COLLISION;

    const struct hdql_Allocator *alloc = hdql_ht_get_alloc(consts->values);
    ConstValItem *csvi = alloc->alloc(sizeof(ConstValItem), alloc->userdata);
    if(!csvi) return HDQL_ERR_MEMORY;

    cvi_init_int(csvi, value);

    int htRC = hdql_ht_ins_cached(consts->values, (const unsigned char *) name
            , nameLen, csvi, entry, nb);

    switch(htRC) {
        case HDQL_HT_RC_INSERTED:
            return HDQL_ERR_CODE_OK;

        case HDQL_HT_ERR_MEM:
            alloc->free(csvi, alloc->userdata);
            return HDQL_ERR_MEMORY;
        default:
            return HDQL_ERR_GENERIC;  /* must not happen; leaves undefined state */
    }
}


enum hdql_ExternValueType
hdql_constants_get_value( struct hdql_Constants * consts
                        , const char * name
                        , hdql_Datum_t * destPtr
                        ) {
    assert(consts);
    if(NULL == name || '\0' == *name) return hdql_kExternValUndefined;
    assert(destPtr);

    void *entry_ = hdql_ht_get(consts->values, (const unsigned char*) name, strlen(name));
    if(NULL == entry_) {
        return consts->parent ? hdql_constants_get_value(consts->parent
                , name, destPtr) : hdql_kExternValUndefined;
    }

    ConstValItem *csvi = (ConstValItem *) entry_;

    switch(csvi->type) {
        case hdql_kExternValFltType:
            *destPtr = (hdql_Datum_t) &csvi->value.asFlt;
            return csvi->type;
        case (hdql_kExternValIntType):
            *destPtr = (hdql_Datum_t) &csvi->value.asInt;
            return csvi->type;
        default:
            *destPtr = NULL;
            return csvi->type;
    }
}

/* ... TODO: LNG22. Unit tests for const values */

/*                                       _____________________________________
 * ____________________________________/ Context-private constants table mgmnt
 */

struct hdql_Constants *
hdql__constants_create(struct hdql_Constants *parent, struct hdql_Context *context) {
    const struct hdql_Allocator *alloc = hdql__context_get_allocator(context);
    if(!alloc) return NULL;
    hdql_Datum_t r_ = alloc->alloc(sizeof(struct hdql_Constants), alloc->userdata);
    if(!r_) return NULL;
    struct hdql_Constants *r = (struct hdql_Constants *) r_;
    r->parent = parent;
    r->values = hdql_ht_create(alloc, 4, HDQL_MURMUR3_32_DEFAULT_SEED);
    return r;
}

static int
destroy_static_const_definition(const unsigned char * key
        , size_t keyLen
        , void ** value
        , void * userdata) {
    ((void) key);
    assert(value);
    assert(userdata);
    if(!*value) return 0;
    const struct hdql_Allocator *alloc = (const struct hdql_Allocator *) userdata;
    alloc->free(*value, alloc->userdata);
    *value = NULL;
    return 0;
}

void
hdql__constants_destroy(struct hdql_Constants *consts, struct hdql_Context *context) {
    if(NULL == consts || NULL == context) return;
    const struct hdql_Allocator *alloc = hdql__context_get_allocator(context);
    if(!alloc) return;
    if(consts->values) {
        hdql_ht_iter(consts->values, destroy_static_const_definition, (void*) alloc);
        hdql_ht_destroy(consts->values);
    }
    alloc->free(consts, alloc->userdata);
}

int
hdql_constants_define_standard_math(struct hdql_Constants * consts) {
    assert(consts);
    int rc;

    rc = hdql_constants_define_float(consts, "pi", M_PI);
    if(rc!= HDQL_ERR_CODE_OK) return rc;

    rc = hdql_constants_define_float(consts, "e", M_E);
    if(rc!= HDQL_ERR_CODE_OK) return rc;

    return HDQL_ERR_CODE_OK;
}
