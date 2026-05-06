#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/function.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <string.h>
#include <assert.h>

typedef struct {
    struct hdql_Query ** queries;
    hdql_TypeConverter * converters;
    size_t nQueries;
} AggFuncSumDefData_t;

struct hdql_AttrDef *
hdql_func_helper__try_instantiate_sum(
          struct hdql_Query ** args, void * userdata
        , char * failureBuffer, size_t failureBufferSize
        , hdql_Context_t context
        ) {
    /* Sum type deduction should follow the rules:
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
     * in arithmetics */
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        if(hdql_attr_def_is_atomic(qAD)) {
            snprintf( failureBuffer, failureBufferSize
                    , "argument %zu is not of atomic type", nArgs );
            return NULL;
        }
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
            }
        } else {
            rTypeCode = atf->arithTypeCode;
        }
    }
    assert(false);  /* TODO */
    /* allocate function "definition data" */
    AggFuncSumDefData_t * dd = hdql_alloc(context, AggFuncSumDefData_t);
    dd->nQueries = nArgs;
    dd->queries = (struct hdql_Query **) hdql_context_alloc(context, sizeof(struct hdql_Query *)*nArgs);
    dd->converters = (hdql_TypeConverter *) hdql_context_alloc(context, sizeof(hdql_TypeConverter)*nArgs);;
    /* assign queries and set up n-th type converter, if needed */
    nArgs = 0;
    for(struct hdql_Query **q = args; *q != NULL; ++q, ++nArgs) {
        const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*q)
                                , *qAD  = hdql_attr_def_top_attr(qAD_);
        const struct hdql_AtomicTypeFeatures * atf = hdql_attr_def_atomic_type_info(qAD);
        /* get result type code */
        if(atf->arithTypeCode == rTypeCode) {
            /* conversion is not needed */
            dd->converters[nArgs] = NULL;
            continue;
        }
        dd->converters[nArgs] = hdql_converters_get(converters, rTypeCode, atf->arithTypeCode);
        if(!dd->converters[nArgs]) {
        }
    }
}
