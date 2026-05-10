#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/context.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef HDQL_CONVERTERS_REALLOC_INCREMENT
#   define HDQL_CONVERTERS_REALLOC_INCREMENT 16
#endif

/* TODO: utilize BFS for types lookup */

typedef struct {
    union {
        struct {
            hdql_ValueTypeCode_t from;
            hdql_ValueTypeCode_t   to;
        } semantic;
        unsigned int numeric;
    } key;
    hdql_TypeConverter converter;
} TypeConverterRecord_t;

struct hdql_Converters {
    size_t nEntres, nAvailable;
    bool sorted;
    TypeConverterRecord_t * records;
    struct hdql_Converters * parent;
};

static int _compare_records(const void * a_, const void * b_) {
    const TypeConverterRecord_t * a = (const TypeConverterRecord_t *) a_;
    const TypeConverterRecord_t * b = (const TypeConverterRecord_t *) b_;
    if(a->key.numeric < b->key.numeric) return -1;
    if(a->key.numeric > b->key.numeric) return  1;
    return 0;
}

//                                    _________________________________________
// _________________________________/ Context-private converter routines mgmnt

struct hdql_Converters *
_hdql_converters_create(struct hdql_Converters * parent, struct hdql_Context * context) {
    struct hdql_Converters * converters = hdql_alloc(context, struct hdql_Converters);
    if(!converters) return NULL;
    converters->nEntres = 0;
    converters->nAvailable = 0;
    converters->records = NULL;
    converters->sorted = false;
    converters->parent = parent;
    return converters;
}

int
hdql_converters_add( struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   , hdql_TypeConverter cnvf
                   , hdql_Context_t context
                   ) {
    assert(cnvs);
    assert(to);
    assert(from);
    assert(context);
    assert(cnvf);

    /* init new entry for copying by value */
    TypeConverterRecord_t newRecord;
    newRecord.key.numeric = 0x0;
    newRecord.key.semantic.from = from;
    newRecord.key.semantic.to   = to;
    newRecord.converter = cnvf;

    {   /* to avoid duplicates, do the linear search */
        for(size_t i = 0; i < cnvs->nEntres; ++i) {
            if(cnvs->records[i].key.numeric == newRecord.key.numeric) {
                if(cnvs->records[i].converter == cnvf)
                    return HDQL_ERR_CODE_OK;  /* just skip full duplicate */
                else {
                    /* detect mismatch */
                    return HDQL_ERR_NAME_COLLISION;
                }
            }
        }
        hdql_TypeConverter pc = hdql_converters_get(cnvs, to, from);
        if(pc) return HDQL_ERR_NAME_COLLISION;
    }

    if(cnvs->nEntres + 1 >= cnvs->nAvailable) {
        /* allocate extended */
        TypeConverterRecord_t * newC = (TypeConverterRecord_t *) hdql_context_alloc(context
                , sizeof(TypeConverterRecord_t)*(cnvs->nAvailable + HDQL_CONVERTERS_REALLOC_INCREMENT) );
        if(!newC)
            return HDQL_ERR_MEMORY;
        /* copy existing and free, if any */
        if(cnvs->nEntres) {
            memcpy(newC, cnvs->records, cnvs->nAvailable*sizeof(TypeConverterRecord_t));
            hdql_context_free(context, (hdql_Datum_t) cnvs->records);
        }
        /* substitute existing with/set to extended */
        cnvs->records = newC;
        cnvs->nAvailable += HDQL_CONVERTERS_REALLOC_INCREMENT;
    }
    assert(cnvs->nEntres + 1 < cnvs->nAvailable);
    assert(cnvs->records);

    cnvs->records[cnvs->nEntres++] = newRecord;
    cnvs->sorted = false;
    return HDQL_ERR_CODE_OK;
}

hdql_TypeConverter
hdql_converters_get( const struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   ) {
    if(0 == cnvs->nEntres)
        goto lookParent;
    if(!cnvs->sorted) {  /* recache */
        struct hdql_Converters * C = (struct hdql_Converters *) cnvs;
        assert(C->records);
        qsort(C->records, C->nEntres, sizeof(TypeConverterRecord_t), _compare_records);
        C->sorted = true;
    }
    TypeConverterRecord_t k;
    k.key.numeric = 0x0;
    k.key.semantic.from = from;
    k.key.semantic.to   = to;
    void *r_ = bsearch(&k, cnvs->records, cnvs->nEntres
            , sizeof(TypeConverterRecord_t)
            , _compare_records);

    if(!r_) return NULL;
    return ((TypeConverterRecord_t*) r_)->converter;
lookParent:
    if(!cnvs->parent) return NULL;
    return hdql_converters_get(cnvs->parent, to, from);
}

void
_hdql_converters_destroy(struct hdql_Converters * cnvs, struct hdql_Context * context) {
    assert(context);
    if(!cnvs) return;
    if(cnvs->records) {
        hdql_context_free(context, (hdql_Datum_t) cnvs->records);
    }
    hdql_context_free(context, (hdql_Datum_t) cnvs);
}

