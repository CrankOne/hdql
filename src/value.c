#include "hdql/value.h"
#include "hdql/util/ht.h"

struct hdql_ValueTypes {
    struct hdql_ValueTypes *parent;
    struct hdql_ht *codeByName
                 , *itemByCode;
    uint8_t tier;
    struct hdql_ArithTypePromotionTable *promotions;
    bool isPromotionsValid;
};  // struct hdql_ValueTypes

