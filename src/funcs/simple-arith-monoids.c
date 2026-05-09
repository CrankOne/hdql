#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <string.h>
#include <assert.h>

/* Simple monoid arithmetics (SMA)
 * ===============================
 *
 * - receives one or more queries resulting in atomic arithmetic type;
 * - results in a scalar real-valued convolution of all the query results;
 * - Usual type promotion is applied for result type. I.e. sum of integers
 *   results in the larger integer, presense of float makes the sum result to
 *   be of floating point, etc.
 * - are not annotated with keys.
 *
 * Last feature makes this class of functions different from classic monoid
 * definitions, which can result in element selection -- min(), max(),
 * arbitrary() etc. Currently, SMA are:
 *
 *    Func. name  | Operation   | Neutral el. | Types           | Result type
 *  --------------+-------------+-------------+-----------------+-------------
 *      sum       | a + b       | 0           | all numeric     | promoted
 *      product   | a * b       | 1           | all numeric     | promoted
 *      count     | a += b? 0:1 | 0           | all             | uin64_t
 *      each_of   | a && b      | true        | all             | bool
 *      any_of    | a || b      | false       | all             | bool
 *      none_of   | (!a) && (!b)| false       | all             | bool
 *      b_AND     | a & b       | ~0x0        | integer only    | promoted int
 *      b_OR      | a | b       | 0x0         | integer only    | promoted int
 *      b_XOR     | a ^ b       | 0x0         | integer only    | promoted int
 *
 * Additionally, there is also a `big_sum()` aggregate function implementing
 * Kahan-Babuska precise summator resulting `double` value from numerical types.
 *
 * The usefulness of XOR-based boolean monoid ("all odd are true") is doubtful,
 * yet one may imagine some practical applications still.
 */

typedef struct {
    hdql_ValueTypeCode_t rTypeCode;
    struct hdql_Query ** queries;
    hdql_TypeConverter * converters;
    size_t nQueries;
} SMADefData_t;

typedef struct {
    struct hdql_Datum ** convertedValues;
    struct hdql_Datum * result;
} SMADynamicData_t;

/*
 * Interface functions
 */

#define SMA_PREFIX sum
#define SMA_OPERATION(R, b) R += b
#define SMA_NEUTRAL 0

#define VALUE_TYPE float
#define VALUE_SUFFIX i32

#define _M_concat_name(a, b, c) a##_##b##__##c
#define _M_func_name(prefix, suffix, name) _M_concat_name(prefix, suffix, name)

static hdql_Datum_t
_M_func_name(SMA_PREFIX, VALUE_SUFFIX, __instantiate)
            ( hdql_Datum_t newOwner
            , const hdql_Datum_t defData_
            , hdql_Context_t context
            ) {
    ((void) newOwner);  /* owner unused here */
    const SMADefData_t *defData = hdql_cast(context, const SMADefData_t, defData_);
    SMADynamicData_t *dynData = hdql_alloc(context, SMADynamicData_t);
    /* allocate destinations */
    dynData->convertedValues
        = (struct hdql_Datum **) hdql_context_alloc(context, sizeof(struct hdql_Datum *)*defData->nQueries);
    /* mark reserved values with zeros to conditionally free them on failure */
    bzero(dynData->convertedValues, sizeof(struct hdql_Datum *));
    /* allocate destinations */
    for(size_t nq = 0; nq < defData->nQueries; ++nq) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(defData->queries[nq])
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        if(atf->arithTypeCode == defData->rTypeCode) {
            /* no conversion needed */
            assert(defData->converters == NULL || NULL != defData->converters[nq]);
            continue;
        }
        /* conversion is needed -- allocate destination */
        dynData->convertedValues[nq] = hdql_create_value(defData->rTypeCode, context);
        assert(dynData->convertedValues[nq]);  /* allocation error */
    }
    /* allocate result datum */
    dynData->result = hdql_create_value(defData->rTypeCode, context);
    assert(dynData->result);
    return (struct hdql_Datum *) dynData;
}

static hdql_Datum_t
_M_func_name(SMA_PREFIX, VALUE_SUFFIX, __reset)
            ( hdql_Datum_t newOwner
            , hdql_Datum_t prevDynData_
            , const hdql_Datum_t defData_
            , hdql_Context_t context
            ) {
    /* Note: following lazy computation logic we do not immediately compute
     * SMA when new owner is set for the sub-queries. SMA should be computed
     * only when the value is requested, so we postpone actual calculus to
     * a "dereference" call. Reset only causes recursive reset of argument
     * queries */
    const SMADefData_t *defData = hdql_cast(context, const SMADefData_t, defData_);
    SMADynamicData_t *dynData = hdql_cast(context, SMADynamicData_t, prevDynData_);
    for(size_t i = 0; i < defData->nQueries; ++i) {
        hdql_query_reset(defData->queries[i], newOwner, context);
    }
    VALUE_TYPE * r = (VALUE_TYPE *) dynData->result;  /* SMA result */
    *r = SMA_NEUTRAL;
    return prevDynData_;
}

static hdql_Datum_t
_M_func_name(SMA_PREFIX, VALUE_SUFFIX, __dereference)
            ( hdql_Datum_t root  /* owning object */
            , hdql_Datum_t dynData_  /* allocated with `instantiate()` */
            , struct hdql_CollectionKey * keys /* may be NULL */
            , const hdql_Datum_t defData_ /* may be NULL */
            , hdql_Context_t context
            ) {
    assert(defData_);
    assert(dynData_);
    ((void) keys);  /* unused as SMA results in a scalar by defintion */
    const SMADefData_t *defData = hdql_cast(context, const SMADefData_t, defData_);
    SMADynamicData_t *dynData = hdql_cast(context, SMADynamicData_t, dynData_);
    assert(dynData->result);
    VALUE_TYPE * r = (VALUE_TYPE *) dynData->result;  /* SMA result */
    hdql_Datum_t locRDatum;
    for(size_t i = 0; i < defData->nQueries; ++i) {
        while(!!(locRDatum = hdql_query_get(defData->queries[i], NULL, context))) {
            if(defData->converters && defData->converters[i]) {
                /* conversion needed */
                defData->converters[i](dynData->convertedValues[i], locRDatum);
                SMA_OPERATION(*r, *((const VALUE_TYPE *) dynData->convertedValues[i]));
            } else {
                /* use result directly */
                SMA_OPERATION(*r, *((const VALUE_TYPE *) locRDatum));
            }
        }
    }
    return dynData->result;
}

static void
_M_func_name(SMA_PREFIX, VALUE_SUFFIX, __destroy)
            ( hdql_Datum_t dynData_
            , const hdql_Datum_t defData_
            , hdql_Context_t context
            ) {
    if(!defData_) return;
    if(!dynData_) return;
    const SMADefData_t *defData = hdql_cast(context, const SMADefData_t, defData_);
    SMADynamicData_t *dynData = hdql_alloc(context, SMADynamicData_t);
    if(dynData->convertedValues) {
        for(size_t nq = 0; nq < defData->nQueries; ++nq) {
            if(!dynData->convertedValues[nq]) continue;
            hdql_context_free(context, dynData->convertedValues[nq]);
        }
        hdql_context_free(context, (hdql_Datum_t) dynData->convertedValues);
    }
    if(dynData->result) {
        hdql_context_free(context, dynData->result);
    }
    hdql_context_free(context, (hdql_Datum_t) dynData);
}

#undef VALUE_TYPE
#undef VALUE_SUFFIX

#undef SMA_PREFIX
#undef SMA_OPERATION
#undef SMA_NEUTRAL

/*
 *
 */

static void
_transient_dtr__simple_monoid_arith(hdql_Datum_t dd_, hdql_Context_t context) {
    const SMADefData_t *dd = hdql_cast(context, const SMADefData_t, dd_);
    hdql_context_free(context, (hdql_Datum_t) dd->converters);
    hdql_context_free(context, (hdql_Datum_t) dd->queries);
    hdql_context_free(context, (hdql_Datum_t) dd);
}

/*
 * Instantiation (function constructor)
 */

struct hdql_AttrDef *
hdql_func_helper__try_instantiate_SMA(
          struct hdql_Query ** args, void * userdata
        , char * failureBuffer, size_t failureBufferSize
        , hdql_Context_t context
        ) {
    /* SMA type deduction should follow the rules:
     *  - integer type(s) result in largest int type
     *  - integer type(s) and/or float results in float
     *  - integer type(s) and/or float and/or results in double
     * */
    if(!args) {
        strncpy(failureBuffer, "no arguments", failureBufferSize);
        return NULL;
    }
    struct hdql_Converters * converters = hdql_context_get_conversions(context);
    struct hdql_ValueTypes * types = hdql_context_get_types(context);
    size_t nArgs = 0;
    hdql_ValueTypeCode_t rTypeCode = 0x0;

    /* iterate over queries, find out appropriate type and check we can use it
     * in arithmetics
     * */
    bool allIsStaticConst = true;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        if(hdql_attr_def_is_atomic(qAD)) {
            snprintf( failureBuffer, failureBufferSize
                    , "argument %zu is not of atomic type", nArgs );
            return NULL;
        }
        if(hdql_attr_def_is_static_const_value(qAD)) allIsStaticConst = false;
        /* get result type code */
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        if(0x0 != rTypeCode) {
            rTypeCode = hdql_types_numeric_promote(types, rTypeCode, atf->arithTypeCode);
            if(0x0 == rTypeCode) {
                snprintf( failureBuffer, failureBufferSize
                    , "can not combine %s and %s into arithmetic type (at argument #%zu)"
                    , hdql_types_get_type(types, rTypeCode)->name
                    , hdql_types_get_type(types, atf->arithTypeCode)->name
                    , nArgs
                    );
                return NULL;
            }
        } else {
            rTypeCode = atf->arithTypeCode;
        }
    }

    if(allIsStaticConst) {
        assert(0);  /* TODO: result can be computed right away */
    }

    /* allocate function "definition data" */
    SMADefData_t * dd = hdql_alloc(context, SMADefData_t);
    dd->nQueries = nArgs;
    dd->queries = (struct hdql_Query **) hdql_context_alloc(context, sizeof(struct hdql_Query *)*nArgs);
    dd->converters = (hdql_TypeConverter *) hdql_context_alloc(context, sizeof(hdql_TypeConverter)*nArgs);

    /* assign queries and set up type converters, if needed */
    nArgs = 0;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        dd->queries[nArgs] = *q;
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        /* get result type code */
        if(atf->arithTypeCode != rTypeCode) {
            dd->converters[nArgs] = hdql_converters_get(converters, rTypeCode, atf->arithTypeCode);
            if(!dd->converters[nArgs]) {
                snprintf( failureBuffer, failureBufferSize
                    , "no converter from %s to %s (at argument #%zu)"
                    , hdql_types_get_type(types, atf->arithTypeCode)->name
                    , hdql_types_get_type(types, rTypeCode         )->name
                    , nArgs
                    );
                goto onFailCleanup;
            }
        }
    }
    dd->rTypeCode = rTypeCode;

    /* form interface */
    struct hdql_ScalarAttrInterface iface;

    /* create (transient) attribute definition */
    struct hdql_AtomicTypeFeatures typeInfo;
    typeInfo.isReadOnly = 0x1;
    typeInfo.arithTypeCode = dd->rTypeCode;
    struct hdql_AttrDef * r = hdql_attr_def_create_atomic_scalar(&typeInfo
            , &iface
            , 0x0
            , NULL
            , context);
    if(!r) {
        goto onFailCleanup;
    }
    hdql_attr_def_set_transient(r, _transient_dtr__simple_monoid_arith);
    return r;
onFailCleanup:
    hdql_context_free(context, (hdql_Datum_t) dd->converters);
    hdql_context_free(context, (hdql_Datum_t) dd->queries);
    hdql_context_free(context, (hdql_Datum_t) dd);
    return NULL;
}
