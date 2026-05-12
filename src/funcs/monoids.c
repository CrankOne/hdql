#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/function.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <string.h>
#include <assert.h>
#include <limits.h>

/* Basic monoid definition interface for numerical set(s): neutral element
 * and operation */
typedef struct {
    /* Sets neutral element (initializes value) */
    void (*set_neutral)(hdql_Datum_t);
    /* Applies operation; may interrupt convolution loop if returns non-zero */
    int (*operation)(hdql_Datum_t, hdql_Datum_t);
} MonoidDefinition_t;

/* Entry type for monoid definition of proper type */
typedef struct {
    /* Result type name (to be resolved into code by types table); used
     * for lookup */
    const char * resultTypeName;
    /* Properly typed definition in use: neutral element and operation routine */
    const MonoidDefinition_t definition;
} MonoidRecord_t;

/* Records collection for monoids with promoted type */
typedef struct {
    /* Records: type name and monoid definition */
    MonoidRecord_t records[16];
    /* Should infer return type and initialize converters, based on
     * null-terminated argument queries array */
    int (*infer_result_type)(struct hdql_Query ** args
        , struct hdql_ValueTypes * types
        , char * failureBuffer, size_t failureBufferSize
        , hdql_ValueTypeCode_t * rTypeCode
        );
    /* Overridden result type (set to empty string if inferred) */
    const char * overrideRType;
} MonoidRecords_t;


/* Static definition data for monoid's result as a scalar */
typedef struct {
    /* returned result value type */
    hdql_ValueTypeCode_t rTypeCode;
    /* list of queries (owned by definition data) */
    struct hdql_Query ** queries;
    /* value converters (can be null if not needed) */
    hdql_TypeConverter * converters;
    /* number of queries (arguments) */
    size_t nQueries;
    /* pointer to SMA definition */
    const MonoidDefinition_t * monoidDef;
    /* result instantiation callback (not setting the neutral element!); dets dynData->result */
    hdql_Datum_t (*instantiate_result)(hdql_ValueTypeCode_t, hdql_Context_t);
    /* result retrieval callback */
    hdql_Datum_t (*retrieve_result)(hdql_Datum_t, hdql_ValueTypeCode_t, hdql_Context_t);
} SMADefData_t;

/* Dynamic definition data for monoid's result as a scalar */
typedef struct {
    /* converters destinations */
    struct hdql_Datum ** convertedValues;
    /* result datum instance ptr; (not necessarily a single value!) */
    struct hdql_Datum * result;
} SMADynamicData_t;

/*
 * Implements Common monoid lifecycle
 */

static hdql_Datum_t
_monoid__instantiate
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
    bzero(dynData->convertedValues, sizeof(struct hdql_Datum *)*defData->nQueries);
    /* allocate destinations */
    for(size_t nq = 0; nq < defData->nQueries; ++nq) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(defData->queries[nq])
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        if(atf->arithTypeCode == defData->rTypeCode) {
            /* no conversion needed */
            assert(defData->converters == NULL || NULL == defData->converters[nq]);
            continue;
        }
        /* conversion is needed -- allocate destination */
        dynData->convertedValues[nq] = hdql_create_value(defData->rTypeCode, context);
        /* ^^^ TODO: monoid def data should create it (to handle advanced
         *     cases like meidan, average, etc */
        assert(dynData->convertedValues[nq]);  /* allocation error */
    }
    /* allocate result datum */
    dynData->result = defData->instantiate_result(defData->rTypeCode, context);
    assert(dynData->result);
    return (struct hdql_Datum *) dynData;
}

static hdql_Datum_t
_monoid__reset
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
    /* set result to neutral element */
    defData->monoidDef->set_neutral(dynData->result);
    return prevDynData_;
}

static hdql_Datum_t
_monoid__dereference
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
    hdql_Datum_t locRDatum;
    for(size_t i = 0; i < defData->nQueries; ++i) {
        while(!!(locRDatum = hdql_query_get(defData->queries[i], NULL, context))) {
            if(defData->converters && defData->converters[i]) {
                /* apply operation with converted */
                defData->converters[i](dynData->convertedValues[i], locRDatum);
                defData->monoidDef->operation(dynData->result, dynData->convertedValues[i]);
            } else {
                /* apply operation without conversion */
                defData->monoidDef->operation(dynData->result, locRDatum);
            }
        }
    }
    return defData->retrieve_result(dynData->result, defData->rTypeCode, context);
}

static void
_monoid__destroy
            ( hdql_Datum_t dynData_
            , const hdql_Datum_t defData_
            , hdql_Context_t context
            ) {
    if(!defData_) return;
    if(!dynData_) return;
    const SMADefData_t *defData = hdql_cast(context, const SMADefData_t, defData_);
    SMADynamicData_t *dynData = hdql_cast(context, SMADynamicData_t, dynData_);
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

static void
_transient_dtr__simple_monoid_arith(hdql_Datum_t dd_, hdql_Context_t context) {
    const SMADefData_t *dd = hdql_cast(context, const SMADefData_t, dd_);
    for(size_t i = 0; i < dd->nQueries; ++i) {
        hdql_query_destroy(dd->queries[i], context);
    }
    hdql_context_free(context, (hdql_Datum_t) dd->converters);
    hdql_context_free(context, (hdql_Datum_t) dd->queries);
    hdql_context_free(context, (hdql_Datum_t) dd);
}

/*
 * Monoid argument type inference
 */

/* Result type inference for simple arithmetic monoids: sum(), product(), etc.
 * - takes numerical (no bool)
 * - result type obeys usual promotion rules
 *
 * Returns:
 * <-1 on failure with reason written to failureBuffer
 *  -1 on case when all the arguments are static arithmetic const
 *  >0 on suceess
 */
static int
_infer_simple_arithmetic_type(struct hdql_Query ** args
        , struct hdql_ValueTypes * types
        , char * failureBuffer, size_t failureBufferSize
        , hdql_ValueTypeCode_t * rTypeCode
        ) {
    hdql_ValueTypeCode_t logicTC = hdql_types_get_type_code(types, "bool");
    /* iterate over queries, find out appropriate type and check we can use it
     * in arithmetics */
    size_t nArgs = 0;
    bool allIsStaticConst = true;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        if(!hdql_attr_def_is_atomic(qAD)) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                        , "argument #%zu is not of atomic type", nArgs+1 );
            return -2;
        }
        if(!hdql_attr_def_is_static_const_value(qAD)) allIsStaticConst = false;
        /* promote result type code */
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        assert(atf->arithTypeCode != 0x0);
        if(logicTC == atf->arithTypeCode) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                    , "argument #%zu is of logic type"
                    , nArgs
                    );
            return -4;
        }
        if(0x0 != *rTypeCode) {
            if(*rTypeCode == atf->arithTypeCode) continue;  /* no promotion/conversion need */
            hdql_ValueTypeCode_t newRtypeCode = hdql_types_numeric_promote(types, *rTypeCode, atf->arithTypeCode);
            if(0x0 == newRtypeCode) {
                if(failureBufferSize)
                    snprintf( failureBuffer, failureBufferSize
                        , "can not combine %s and %s into arithmetic type (at argument #%zu)"
                        , *rTypeCode ? hdql_types_get_type(types, *rTypeCode)->name : "(unset)"
                        , atf->arithTypeCode ? hdql_types_get_type(types, atf->arithTypeCode)->name : "(unknown)"
                        , nArgs
                        );
                return -3;
            }
            *rTypeCode = newRtypeCode;
        } else {
            *rTypeCode = atf->arithTypeCode;
        }
    }
    assert(0 != *rTypeCode);
    if(allIsStaticConst) return -1;
    return nArgs;
}

/* Result type inference for monoids expecting integer only values (bitwise
 * operations):
 * - takes integral types only (no bool, float, double)
 * - result type obeys usual promotion rules
 *
 * Returns:
 * <-1 on failure with reason written to failureBuffer
 *  -1 on case when all the arguments are static arithmetic const
 *  >0 on suceess
 */
static int
_infer_integer_only_type(struct hdql_Query ** args
        , struct hdql_ValueTypes * types
        , char * failureBuffer, size_t failureBufferSize
        , hdql_ValueTypeCode_t * rTypeCode
        ) {
    hdql_ValueTypeCode_t floatTC = hdql_types_get_type_code(types, "float")
                       , doubleTC = hdql_types_get_type_code(types, "double");
    /* iterate over queries, find out appropriate type and check we can use it
     * in arithmetics */
    size_t nArgs = 0;
    bool allIsStaticConst = true;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        if(!hdql_attr_def_is_atomic(qAD)) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                        , "argument #%zu is not of atomic type", nArgs+1 );
            return -2;
        }
        if(!hdql_attr_def_is_static_const_value(qAD)) allIsStaticConst = false;
        /* promote result type code */
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        assert(atf->arithTypeCode != 0x0);
        if( (0 != floatTC  && atf->arithTypeCode == floatTC)
         || (0 != doubleTC && atf->arithTypeCode == doubleTC)) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                        , "argument #%zu is of floating point type", nArgs+1 );
            return -3;
        }
        if(0x0 != *rTypeCode) {
            if(*rTypeCode == atf->arithTypeCode) continue;  /* no promotion/conversion need */
            hdql_ValueTypeCode_t newRtypeCode = hdql_types_numeric_promote(types, *rTypeCode, atf->arithTypeCode);
            if(0x0 == newRtypeCode) {
                if(failureBufferSize)
                    snprintf( failureBuffer, failureBufferSize
                        , "can not combine %s and %s into integer type (at argument #%zu)"
                        , *rTypeCode ? hdql_types_get_type(types, *rTypeCode)->name : "(unset)"
                        , atf->arithTypeCode ? hdql_types_get_type(types, atf->arithTypeCode)->name : "(unknown)"
                        , nArgs
                        );
                return -4;
            }
            *rTypeCode = newRtypeCode;
        } else {
            *rTypeCode = atf->arithTypeCode;
        }
    }
    assert(*rTypeCode != 0x0);
    assert( (0 == floatTC  || *rTypeCode != floatTC) || (0 == doubleTC || *rTypeCode != doubleTC));
    if(allIsStaticConst) return -1;
    return nArgs;
}

/* Checks, that all the argument queries result in a numeric or logic values.
 * Used for all()/any()/none() monoids.
 * - takes numerical and boolean
 * - result type is boolean
 *
 * Returns:
 * <-1 on failure with reason written to failureBuffer
 *  -1 on case when all the arguments are static arithmetic const
 *  >0 on suceess
 */
static int
_infer_check_atomic_type_to_logic(struct hdql_Query ** args
        , struct hdql_ValueTypes * types
        , char * failureBuffer, size_t failureBufferSize
        , hdql_ValueTypeCode_t * rTypeCode
        ) {
    /* retrieve logic type */
    hdql_ValueTypeCode_t boolTC = hdql_types_get_type_code(types, "bool");
    if(0x0 == boolTC) {
        if(failureBufferSize)
            snprintf( failureBuffer, failureBufferSize
                    , "no \"bool\" type defined in the evaluation context");
        return -2;
    }
    /* iterate over queries, make converters */
    size_t nArgs = 0;
    bool allIsStaticConst = true;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        if(!hdql_attr_def_is_atomic(qAD)) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                        , "argument #%zu is not of atomic type", nArgs+1 );
            return -2;
        }
        if(!hdql_attr_def_is_static_const_value(qAD)) allIsStaticConst = false;
    }
    assert(0 != *rTypeCode);
    if(allIsStaticConst) return -1;
    return nArgs;
}

static int
_infer_check_atomic_type_to_logic_as_logic(struct hdql_Query ** args
        , struct hdql_ValueTypes * types
        , char * failureBuffer, size_t failureBufferSize
        , hdql_ValueTypeCode_t * rTypeCode
        ) {
    *rTypeCode = hdql_types_get_type_code(types, "bool");
    if(0x0 == *rTypeCode) {
        strncpy(failureBuffer, "no \"bool\" type defined", failureBufferSize);
        return -10;
    }
    return _infer_check_atomic_type_to_logic(args, types, failureBuffer,
                failureBufferSize, rTypeCode);
}

/* Trivial result retrieve -- used for monoids when internal state is the
 * results itself (sum, product, bitwise convolutions) */
static hdql_Datum_t _trivial_retrive_result(hdql_Datum_t r
        , hdql_ValueTypeCode_t rTypeCode, hdql_Context_t context) {
    ((void) rTypeCode);
    ((void) context);
    return r;
}

/*
 * Instantiation (function constructor)
 */

struct hdql_AttrDef *
hdql_func_helper__try_monoid(
          struct hdql_Query ** args, void * userdata
        , char * failureBuffer, size_t failureBufferSize
        , hdql_Context_t context
        ) {
    assert(userdata);
    MonoidRecords_t * monoidRecords = (MonoidRecords_t *) userdata;
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
    hdql_ValueTypeCode_t rTypeCode = 0x0;

    int rc = monoidRecords->infer_result_type(args, types
                , failureBuffer, failureBufferSize
                , &rTypeCode
                );
    if(rc < -1) { return NULL; }
    if(rc == -1) {
        /* TODO: is there any practical value for that? E.g. sum(1.23, 4.56) */
        if(failureBufferSize)
            snprintf( failureBuffer, failureBufferSize
                    , "handling all static arguments is not implemented" );
        return NULL;
    }
    size_t nArgs = (size_t) rc;

    /* find properly typed monoid definition */
    const MonoidDefinition_t * monoidDefPtr = NULL;
    if('*' != *monoidRecords->records[0].resultTypeName) {
        for(MonoidRecord_t * mrec = monoidRecords->records; '\0' != *mrec->resultTypeName; ++mrec) {
            hdql_ValueTypeCode_t code = hdql_types_get_type_code(types, mrec->resultTypeName);
            if(code != rTypeCode) continue;
            monoidDefPtr = &mrec->definition;
            break;
        }
    } else {
        monoidDefPtr = &monoidRecords->records[0].definition;
    }
    if(!monoidDefPtr) {
        if(failureBufferSize) {
            const struct hdql_ValueInterface * rTypeIFace = hdql_types_get_type(types, rTypeCode);
            snprintf( failureBuffer, failureBufferSize
                    , "no appropriately typed monoid defined for inferred"
                      " result type \"%s\""
                    , (rTypeIFace && rTypeIFace->name) ? rTypeIFace->name : "???"
                    );
        }
        return NULL;
    }

    /* allocate function "definition data" */
    assert(nArgs > 0);
    SMADefData_t * dd = hdql_alloc(context, SMADefData_t);
    dd->monoidDef = monoidDefPtr;
    dd->nQueries = nArgs;
    dd->queries = (struct hdql_Query **) hdql_context_alloc(context, sizeof(struct hdql_Query *)*nArgs);
    dd->converters = (hdql_TypeConverter *) hdql_context_alloc(context, sizeof(hdql_TypeConverter)*nArgs);
    bzero(dd->converters, sizeof(hdql_TypeConverter)*nArgs);
    dd->instantiate_result = hdql_create_value;  // TODO: should be overridden by fsum(),fmean(), etc
    dd->retrieve_result = _trivial_retrive_result;  // TODO: should be overridden by fsum(),fmean(), etc

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
                if(failureBufferSize)
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
    if(monoidRecords->overrideRType == NULL) {
        dd->rTypeCode = rTypeCode;
    } else {
        dd->rTypeCode = hdql_types_get_type_code(types, monoidRecords->overrideRType);
        if(0x0 == dd->rTypeCode) {
            if(failureBufferSize)
                snprintf( failureBuffer, failureBufferSize
                    , "no type \"%s\" defined in the context to use as monoid result"
                    , monoidRecords->overrideRType
                    );
            goto onFailCleanup;
        }
    }

    /* form interface */
    struct hdql_ScalarAttrInterface iface;
    iface.definitionData = (hdql_Datum_t) dd;
    iface.instantiate = _monoid__instantiate;
    iface.dereference = _monoid__dereference;
    iface.reset       = _monoid__reset;
    iface.destroy     = _monoid__destroy;

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
    assert(dd->monoidDef);
    return r;
onFailCleanup:
    hdql_context_free(context, (hdql_Datum_t) dd->converters);
    hdql_context_free(context, (hdql_Datum_t) dd->queries);
    hdql_context_free(context, (hdql_Datum_t) dd);
    return NULL;
}

/* 
 * Monoid definition 
 */

#define _M_implement_sum(suffix, type)  \
static void _sum_ ## suffix ## _set_neutral(hdql_Datum_t d)  {  *((type *)  d) = 0; }  \
static int  _sum_ ## suffix ## _operation(hdql_Datum_t r, hdql_Datum_t v)  \
    { *((type *) r) += *((type *) v); return 0; }

#define _M_implement_prod(suffix, type)  \
static void _prod_ ## suffix ## _set_neutral(hdql_Datum_t d)  {  *((type *)  d) = 1; }  \
static int  _prod_ ## suffix ## _operation(hdql_Datum_t r, hdql_Datum_t v)  \
    { *((type *) r) *= *((type *) v); return 0; }

#define _M_implement_bAND(suffix, type)  \
static void _bAND_ ## suffix ## _set_neutral(hdql_Datum_t d)  {  *((type *)  d) = ~((type) 0x0); }  \
static int  _bAND_ ## suffix ## _operation(hdql_Datum_t r, hdql_Datum_t v)  \
    { if(!!(*((type *) r) &= *((type *) v))) return 0; return INT_MAX; }

#define _M_implement_bOR(suffix, type)  \
static void _bOR_ ## suffix ## _set_neutral(hdql_Datum_t d)  {  *((type *)  d) =  ((type) 0x0); }  \
static int  _bOR_ ## suffix ## _operation(hdql_Datum_t r, hdql_Datum_t v)  \
    { if( ((type)~0) != (*((type *) r) |= *((type *) v))); return 0; return INT_MAX; }

#define _M_implement_bXOR(suffix, type)  \
static void _bXOR_ ## suffix ## _set_neutral(hdql_Datum_t d)  {  *((type *)  d) =  ((type) 0x0); }  \
static int  _bXOR_ ## suffix ## _operation(hdql_Datum_t r, hdql_Datum_t v)  \
    { *((type *) r) ^= *((type *) v); return 0; }

#define _M_for_each_integer_type(m) \
    m(i8,   int8_t  ) \
    m(ui8,  uint8_t ) \
    m(i16,  int16_t ) \
    m(ui16, uint16_t) \
    m(i32,  int32_t ) \
    m(ui32, uint32_t) \
    m(i64,  int64_t ) \
    m(ui64, uint64_t)

#define _M_for_each_fp_type(m) \
    m(float,    float ) \
    m(double,   double)

/* sum() */
_M_for_each_integer_type(_M_implement_sum);
_M_for_each_fp_type(_M_implement_sum);
/* prod() */
_M_for_each_integer_type(_M_implement_prod);
_M_for_each_fp_type(_M_implement_prod);

/* bitwise convolutions */
_M_for_each_integer_type(_M_implement_bAND);
_M_for_each_integer_type(_M_implement_bOR);
_M_for_each_integer_type(_M_implement_bXOR);

/* all (logic AND-convolution */
static void _all_set_neutral(hdql_Datum_t d) {
    *((HDQL_ARITH_BOOL_TYPE *) d) = true;
}

static int  _all_operation(hdql_Datum_t r, hdql_Datum_t v) {
    if(!!(*((HDQL_ARITH_BOOL_TYPE *) r) &= *((HDQL_ARITH_BOOL_TYPE *) v)))
        return 0;
    return INT_MAX;  /* no need for further checks (at least one is false) */
}

/* any (logic OR-convolution */
static void _any_set_neutral(hdql_Datum_t d) {
    *((HDQL_ARITH_BOOL_TYPE *) d) = false;
}

static int  _any_operation(hdql_Datum_t r, hdql_Datum_t v) {
    if(!(*((HDQL_ARITH_BOOL_TYPE *) r) |= *((HDQL_ARITH_BOOL_TYPE *) v)))
        return 0;
    return INT_MAX;  /* no need for further checks (at least one is true) */
}

/* count */
static void _count_set_neutral(hdql_Datum_t d) {
    *((HDQL_ARITH_INT_TYPE *) d) = 0;
}

static int  _count_operation(hdql_Datum_t r, hdql_Datum_t v) {
    if(*((HDQL_ARITH_BOOL_TYPE *) v))
        ++*((HDQL_ARITH_INT_TYPE *) r);
    return 0;
}

/*
 * Register function
 */

int
hdql_functions_add_monoids(struct hdql_Functions * functions) {
    /* sum */
    static const MonoidRecords_t _sumArithMonoidRecords = {
        .records = {
            #define _M_implement_record(suffix, type) \
            { #type , { _sum_ ## suffix ## _set_neutral, _sum_ ## suffix ## _operation }  },
            _M_for_each_integer_type(_M_implement_record)
            _M_for_each_fp_type(_M_implement_record)
            #undef _M_implement_record
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_simple_arithmetic_type,
        .overrideRType = NULL
    };
    /* product */
    static const MonoidRecords_t _prodArithMonoidRecords = {
        .records = {
            #define _M_implement_record(suffix, type) \
            { #type , { _prod_ ## suffix ## _set_neutral, _prod_ ## suffix ## _operation }  },
            _M_for_each_integer_type(_M_implement_record)
            _M_for_each_fp_type(_M_implement_record)
            #undef _M_implement_record
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_simple_arithmetic_type,
        .overrideRType = NULL
    };
    /* bitwise AND monoid */
    static const MonoidRecords_t _bANDMonoidRecords = {
        .records = {
            #define _M_implement_record(suffix, type) \
            { #type , { _bAND_ ## suffix ## _set_neutral, _bAND_ ## suffix ## _operation }  },
            _M_for_each_integer_type(_M_implement_record)
            #undef _M_implement_record
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_integer_only_type,
        .overrideRType = NULL
    };
    /* bitwise OR monoid */
    static const MonoidRecords_t _bORMonoidRecords = {
        .records = {
            #define _M_implement_record(suffix, type) \
            { #type , { _bOR_ ## suffix ## _set_neutral, _bOR_ ## suffix ## _operation }  },
            _M_for_each_integer_type(_M_implement_record)
            #undef _M_implement_record
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_integer_only_type,
        .overrideRType = NULL
    };
    /* bitwise XOR monoid */
    static const MonoidRecords_t _bXORMonoidRecords = {
        .records = {
            #define _M_implement_record(suffix, type) \
            { #type , { _bXOR_ ## suffix ## _set_neutral, _bXOR_ ## suffix ## _operation }  },
            _M_for_each_integer_type(_M_implement_record)
            #undef _M_implement_record
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_integer_only_type,
        .overrideRType = NULL
    };
    /* all */
    static const MonoidRecords_t _allMonoidRecords = {
        .records = {
            { "*", { _all_set_neutral, _all_operation } },
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_check_atomic_type_to_logic_as_logic,
        .overrideRType = NULL
    };
    /* any */
    static const MonoidRecords_t _anyMonoidRecords = {
        .records = {
            { "*", { _any_set_neutral, _any_operation } },
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_check_atomic_type_to_logic_as_logic,
        .overrideRType = NULL
    };
    /* count */
    static const MonoidRecords_t _countMonoidRecords = {
        .records = {
            { "*", { _count_set_neutral, _count_operation } },
            { "", {NULL, NULL} }
        },
        .infer_result_type = _infer_check_atomic_type_to_logic_as_logic,
        .overrideRType = "uint64_t"
    };

    int rc;
    rc = hdql_functions_define(functions, "sum"
            , hdql_func_helper__try_monoid
            , (void *) &_sumArithMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "prod"
            , hdql_func_helper__try_monoid
            , (void *) &_prodArithMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "bAND"
            , hdql_func_helper__try_monoid
            , (void *) &_bANDMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "bOR"
            , hdql_func_helper__try_monoid
            , (void *) &_bORMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "bXOR"
            , hdql_func_helper__try_monoid
            , (void *) &_bXORMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "all"
            , hdql_func_helper__try_monoid
            , (void *) &_allMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "any"
            , hdql_func_helper__try_monoid
            , (void *) &_anyMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;

    rc = hdql_functions_define(functions, "count"
            , hdql_func_helper__try_monoid
            , (void *) &_countMonoidRecords );
    if(HDQL_ERR_CODE_OK != rc) return rc;
    return 0;
}

