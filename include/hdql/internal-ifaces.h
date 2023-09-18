#ifndef H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H
#define H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H

#include "hdql/attr-def.h"

#ifdef __cplusplus
extern "C" {
#endif



extern const struct hdql_ScalarAttrInterface        _hdql_gScalarFwdQueryIFace;
extern const struct hdql_CollectionAttrInterface    _hdql_gCollectionFwdQueryIFace;

/**\brief Definition data for arithmetic operation access interfaces*/
struct hdql_ArithOpDefData {
    struct hdql_Query * args[2];
    const struct hdql_OperationEvaluator * evaluator;
};

extern const struct hdql_ScalarAttrInterface        _hdql_gScalarArithOpIFace;
extern const struct hdql_CollectionAttrInterface    _hdql_gCollectionArithOpIFace;

struct hdql_CollectionKey *
hdql_reserve_arith_op_collection_key(const hdql_Datum_t defData, hdql_Context_t context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H */
