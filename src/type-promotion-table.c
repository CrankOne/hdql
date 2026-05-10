#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <stdlib.h>

#ifndef HDQL_PROMOTION_TABLE_MAX_ENTRIES
#   define HDQL_PROMOTION_TABLE_MAX_ENTRIES 64
#endif

#define _M_for_each_arith_types_pair(m) \
    m(  int8_t,   uint8_t,  int16_t ) \
    m(  int8_t,   int16_t,  int16_t ) \
    m(  int8_t,  uint16_t,  int32_t ) \
    m(  int8_t,   int32_t,  int32_t ) \
    m(  int8_t,  uint32_t,  int64_t ) \
    m(  int8_t,   int64_t,  int64_t ) \
    m(  int8_t,  uint64_t, uint64_t ) \
    m(  int8_t,     float,    float ) \
    m(  int8_t,    double,   double ) \
    m( uint8_t,   int16_t,  int16_t ) \
    m( uint8_t,  uint16_t, uint16_t ) \
    m( uint8_t,   int32_t,  int32_t ) \
    m( uint8_t,  uint32_t, uint32_t ) \
    m( uint8_t,   int64_t,  int64_t ) \
    m( uint8_t,  uint64_t, uint64_t ) \
    m( uint8_t,     float,    float ) \
    m( uint8_t,    double,   double ) \
    m( int16_t,  uint16_t,  int32_t ) \
    m( int16_t,   int32_t,  int32_t ) \
    m( int16_t,  uint32_t,  int64_t ) \
    m( int16_t,   int64_t,  int64_t ) \
    m( int16_t,  uint64_t, uint64_t ) \
    m( int16_t,     float,    float ) \
    m( int16_t,    double,   double ) \
    m(uint16_t,   int32_t,  int32_t ) \
    m(uint16_t,  uint32_t, uint32_t ) \
    m(uint16_t,   int64_t,  int64_t ) \
    m(uint16_t,  uint64_t, uint64_t ) \
    m(uint16_t,     float,    float ) \
    m(uint16_t,    double,   double ) \
    m( int32_t,  uint32_t,  int64_t ) \
    m( int32_t,   int64_t,  int64_t ) \
    m( int32_t,  uint64_t, uint64_t ) \
    m( int32_t,     float,    float ) \
    m( int32_t,    double,   double ) \
    m(uint32_t,   int64_t,  int64_t ) \
    m(uint32_t,  uint64_t, uint64_t ) \
    m(uint32_t,     float,    float ) \
    m(uint32_t,    double,   double ) \
    m( int64_t,  uint64_t, uint64_t ) \
    m( int64_t,     float,    float ) \
    m( int64_t,    double,   double ) \
    m(uint64_t,     float,    float ) \
    m(uint64_t,    double,   double ) \
    m(   float,    double,   double )

typedef struct {
    union {
        struct {
            hdql_ValueTypeCode_t a:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
            hdql_ValueTypeCode_t b:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
        } semantic;
        unsigned int numeric;
    } key;
    hdql_ValueTypeCode_t r:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
} ArithTypePromotionRecord_t;

struct hdql_ArithTypePromotionTable {
    int nEntres;
    ArithTypePromotionRecord_t records[HDQL_PROMOTION_TABLE_MAX_ENTRIES];
};

struct hdql_ArithTypePromotionTable *
hdql_arith_type_promotion_create(hdql_Context_t context) {
    return hdql_alloc(context, struct hdql_ArithTypePromotionTable);
}

void
hdql_arith_type_promotion_destroy(hdql_Context_t context
        , struct hdql_ArithTypePromotionTable * t) {
    hdql_context_free(context, (hdql_Datum_t) t);
}



static int _compare_records(const void * a_, const void * b_) {
    const ArithTypePromotionRecord_t * a = (const ArithTypePromotionRecord_t *) a_;
    const ArithTypePromotionRecord_t * b = (const ArithTypePromotionRecord_t *) b_;
    if(a->key.numeric < b->key.numeric) return -1;
    if(a->key.numeric > b->key.numeric) return  1;
    return 0;
}

int
hdql_arith_type_promotion_rebuild(const struct hdql_ValueTypes * vt,
        struct hdql_ArithTypePromotionTable * t) {
    size_t n = 0;
    /* (re)populate the table */
    hdql_ValueTypeCode_t a, b, r;
    #define _M_add_entry(A, B, R)  \
    a = hdql_types_get_type_code(vt, #A);  \
    b = hdql_types_get_type_code(vt, #B);  \
    r = hdql_types_get_type_code(vt, #R);  \
    if( a && b && r ) {  \
        if(n >= HDQL_PROMOTION_TABLE_MAX_ENTRIES) \
            return HDQL_ERR_MEMORY; \
        t->records[n].key.numeric    = 0x0;  \
        if(a < b) { \
            t->records[n].key.semantic.a = a;  \
            t->records[n].key.semantic.b = b;  \
        } else {  \
            t->records[n].key.semantic.a = b;  \
            t->records[n].key.semantic.b = a;  \
        }  \
        t->records[n++].r            = r;  \
    }
    _M_for_each_arith_types_pair(_M_add_entry);
    t->nEntres = n;
    if(0 == n) return HDQL_ERR_EMPTY_SET;
    /* sort table */
    qsort(t->records, n, sizeof(ArithTypePromotionRecord_t)
            , _compare_records);
    return HDQL_ERR_CODE_OK;
}

hdql_ValueTypeCode_t
hdql_arith_type_promote(const struct hdql_ArithTypePromotionTable * t
        , hdql_ValueTypeCode_t a, hdql_ValueTypeCode_t b
        ) {
    if(0 == a || 0 == b || 0 == t->nEntres) return 0;

    ArithTypePromotionRecord_t k;
    k.key.numeric = 0x0;
    if(a < b) {
        k.key.semantic.a = a;
        k.key.semantic.b = b;
    } else {
        k.key.semantic.a = b;
        k.key.semantic.b = a;
    }

    void *r_ = bsearch(&k, t->records, t->nEntres
            , sizeof(ArithTypePromotionRecord_t)
            , _compare_records);
    if(!r_) return 0;
    return ((ArithTypePromotionRecord_t*) r_)->r;
}

