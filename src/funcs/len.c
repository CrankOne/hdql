#include "hdql/attr-def.h"
#include "hdql/query.h"
#include "hdql/context.h"
#include "hdql/types.h"

#include <string.h>
#include <assert.h>

/* TODO: generally, we can anticipate optional `length`/`empty` method(s)
 *       for the collection interface in future to enhance performance of these
 *       methods.
 */

typedef struct {
    struct hdql_Query * query;
} LenEmptyFuncDefData_t;

typedef struct {
    uint64_t counter;
    hdql_Bool_t isEmpty;
} LenEmptyFuncDynData_t;


static hdql_Datum_t
_len_empty__new_dyn_data
            ( hdql_Datum_t newOwner
            , const struct hdql_Datum *defData_
            , hdql_Context_t context
            ) {
    ((void) newOwner);  /* owner unused here */
    LenEmptyFuncDynData_t *dynData = hdql_alloc(context, LenEmptyFuncDynData_t);
    return (struct hdql_Datum *) dynData;
}

static hdql_Datum_t
_len__reset ( hdql_Datum_t newOwner
            , hdql_Datum_t dynData_
            , const struct hdql_Datum *defData_
            , struct hdql_Key * key
            , hdql_Context_t context
            ) {
    assert(defData_);
    const LenEmptyFuncDefData_t *defData = hdql_cast(context, const LenEmptyFuncDefData_t, defData_);
    LenEmptyFuncDynData_t *dynData = hdql_cast(context, LenEmptyFuncDynData_t, dynData_);
    dynData->counter = 0;
    for( hdql_Datum_t r = hdql_query_reset(defData->query, newOwner, key, context)
       ; r
       ; r = hdql_query_get(defData->query, NULL, context), ++(dynData->counter)) {}
    return (hdql_Datum_t) &dynData->counter;
}

static hdql_Datum_t
_empty__reset ( hdql_Datum_t newOwner
            , hdql_Datum_t dynData_
            , const struct hdql_Datum *defData_
            , struct hdql_Key * key
            , hdql_Context_t context
            ) {
    assert(defData_);
    const LenEmptyFuncDefData_t *defData = hdql_cast(context, const LenEmptyFuncDefData_t, defData_);
    LenEmptyFuncDynData_t *dynData = hdql_cast(context, LenEmptyFuncDynData_t, dynData_);
    dynData->counter = 0;
    dynData->isEmpty = true;
    if(hdql_query_reset(defData->query, newOwner, key, context))
        dynData->isEmpty = false;
    return (hdql_Datum_t) &dynData->isEmpty;
}

static void
_len_empty__destroy
            ( hdql_Datum_t dynData_
            , const struct hdql_Datum *defData_
            , hdql_Context_t context
            ) {
    if(!defData_) return;
    if(!dynData_) return;
    LenEmptyFuncDynData_t *dynData = hdql_cast(context, LenEmptyFuncDynData_t, dynData_);
    hdql_context_free(context, (hdql_Datum_t) dynData);
}


static void
_transient_dtr__len_empty(hdql_Datum_t dd_, hdql_Context_t context) {
    if(!dd_) return;
    const LenEmptyFuncDefData_t *dd = hdql_cast(context, const LenEmptyFuncDefData_t, dd_);
    if(dd->query)
        hdql_query_destroy(dd->query, context);
    hdql_context_free(context, dd_);
}

struct hdql_AttrDef *
hdql_func_helper__try_len_empty(
          struct hdql_Query ** args, void * userdata
        , char * failureBuffer, size_t failureBufferSize
        , hdql_Context_t context
        ) {
    assert(userdata);
    char nm = *((const char *) userdata);
    assert(nm == 'e' || nm == 'l');
    if(args[0] == NULL) {
        if(failureBuffer)
            strncpy(failureBuffer, "empty argument, one expected", failureBufferSize);
        return NULL;
    }
    if(args[1] != NULL) {
        if(failureBuffer)
            strncpy(failureBuffer, "multiple arguments, one expected", failureBufferSize);
        return NULL;
    }
    //const struct hdql_AttrDef *qAD_ = hdql_query_top_attr(*args)
    //                        , *qAD  = hdql_attr_def_top_attr(qAD_);
    if(hdql_query_is_fully_scalar(*args)) {
        if(failureBuffer)
            strncpy(failureBuffer, "scalar argument", failureBufferSize);
        return NULL;
    }

    LenEmptyFuncDefData_t * dd = (LenEmptyFuncDefData_t *)
                hdql_context_alloc(context, sizeof(LenEmptyFuncDefData_t));
    dd->query = args[0];

    /* form interface */
    struct hdql_ScalarAttrInterface iface;
    iface.definitionData = (hdql_Datum_t) dd;
    iface.new_dyn_data = _len_empty__new_dyn_data;
    iface.reset = nm == 'l'
                ? _len__reset
                : _empty__reset
                ;
    iface.destroy_dyn_data = _len_empty__destroy;

    /* create (transient) attribute definition */
    struct hdql_AtomicTypeFeatures typeInfo;
    typeInfo.isReadOnly = 0x1;

    struct hdql_ValueTypes * types = hdql_context_get_types(context);

    typeInfo.arithTypeCode = hdql_types_get_type_code(types, nm == 'l' ? "uint64_t" : "bool");
    assert(typeInfo.arithTypeCode);
    struct hdql_AttrDef * r = hdql_attr_def_create_atomic_scalar(&typeInfo
            , &iface
            , 0x0
            , NULL
            , context);
    if(r) {
        hdql_attr_def_set_transient(r, _transient_dtr__len_empty);
    }
    return r;
}
