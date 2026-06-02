#include "hdql/value.h"
#include "hdql/errors.h"
#include "hdql/util/ht.h"

#include <assert.h>

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

struct hdql_ValueTypes {
    struct hdql_ValueTypes *parent;
    struct hdql_ht *codeByName;
    struct hdql_avl *itemByCode;
    uint8_t tier;
    struct hdql_ArithTypePromotionTable *promotions;
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
    size_t nItems = hdql_ht_size(vt->itemByCode);
    if(nItems >= gMaxTypeIDInTier) {
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;  /* IDs exceed for tier */
    }
    hdql_ValueTypeCode_t tCode = (hdql_ValueTypeCode_t) nItems;
    tCode |= ((hdql_ValueTypeCode_t) vt->tier) << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
    auto ir = _codeByName.emplace(iface.name, tCode);
    if(!ir.second) return -3;  // duplicating name
    auto iir = _itemByCode.emplace(tCode, iface);
    if(!iir.second) {
        _codeByName.erase(ir.first);
        return -4;  // internal logic error
    }
    return 0;
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

