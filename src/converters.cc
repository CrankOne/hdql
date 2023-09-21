#include "hdql/types.h"
#include "hdql/value.h"

#include <unordered_map>

typedef int
(*hdql_TypeConverter)( struct hdql_Datum * __restrict__ dest
                     , struct hdql_Datum * __restrict__ src
                     );

struct hdql_Converters
    : public std::unordered_map<uint32_t, hdql_TypeConverter> {
    struct hdql_Converters * parent;

    hdql_Converters() : parent(nullptr) {}
};

extern "C" int
hdql_converters_add( hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   , hdql_TypeConverter cnvf
                   ) {
    uint32_t k = ((uint32_t) from) | (((uint32_t) to) << 16);
    auto ir = cnvs->emplace(k, cnvf);
    return ir.second ? -1 : 0;
}

extern "C" hdql_TypeConverter
hdql_converters_get( hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   ) {
    uint32_t k = ((uint32_t) from) | (((uint32_t) to) << 16);
    auto it = cnvs->find(k);
    if(cnvs->end() != it) return it->second;
    if(cnvs->parent)
        return hdql_converters_get(cnvs->parent, to, from);
    return NULL;
}

//                                    _________________________________________
// _________________________________/ Context-private converter routines mgmnt

extern "C" struct hdql_Converters *
_hdql_converters_create(struct hdql_Context *) {
    return new hdql_Converters;
}

extern "C" void
_hdql_converters_destroy(struct hdql_Converters * cnvs, struct hdql_Context *) {
    delete cnvs;
}


