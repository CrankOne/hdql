#include "hdql/value.h"
#include "hdql/errors.h"
#include "hdql/util/avl-tree.h"
#include "hdql/util/ht.h"

#include <assert.h>
#include <string.h>

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
hdql_types_define( struct hdql_ValueTypes * vt
        , const struct hdql_ValueInterface * vti
        ) {
    vt->isPromotionsValid = false;
    if(NULL == vti->name || *vti->name == '\0') {
        return HDQL_ERR_BAD_ARGUMENT;
    }
    size_t nItems = hdql_umap_size(vt->itemByCode);
    if(nItems >= gMaxTypeIDInTier) {
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;  /* IDs exceed for tier */
    }

    /* generate type code entry */
    hdql_ValueTypeCode_t tCode = (hdql_ValueTypeCode_t) nItems;
    tCode |= ((hdql_ValueTypeCode_t) vt->tier) << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
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
        hdql_ht_ins_cached(vt->codeByName
                , (const unsigned char *) vti->name, nameLen1
                , (void *) ((unsigned long) tCode), entry, nb);
    }

    /* add code-to-type record */
    int rc = hdql_umap_insert(vt->itemByCode, tCode, (void *) vti);
    int myRC = HDQL_ERR_CODE_OK;
    switch(rc) {
        case HDQL_AVL_OK:
            return myRC;

        case HDQL_AVL_MEM_ERROR:
            myRC = HDQL_ERR_MEMORY;
        case HDQL_AVL_BAD_ARG_ERROR:
            myRC = HDQL_ERR_BAD_ARGUMENT;
        case HDQL_AVL_CHANGED:
            myRC = HDQL_ERR_GENERIC;
        default:
            hdql_ht_remove(vt->codeByName, (const unsigned char *) vti->name, nameLen1);
            return myRC;
}

int
hdql_types_alias(
          struct hdql_ValueTypes * vt
        , const char * alias
        , hdql_ValueTypeCode_t code
        ) {
    return vt->define_alias(code, alias);
}

const struct hdql_ValueInterface *
hdql_types_get_type_by_name(const struct hdql_ValueTypes * vt, const char * nm) {
    return vt->get_by_name(nm);
}

const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc) {
    assert(vt);
    return vt->get_by_code(tc);
}

extern "C" hdql_ValueTypeCode_t
hdql_types_get_type_code(const struct hdql_ValueTypes * vt, const char * name) {
    return vt->get_code_by_name(name);
}

