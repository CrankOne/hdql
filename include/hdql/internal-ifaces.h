#ifndef H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H
#define H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H 1

#include "hdql/types.h"
#include "hdql/attr-def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Forwarding queries
 * */

/** Interface for queries evaluation to attributes to scalar forwarding queries */
HDQL_API extern const struct hdql_ScalarAttrInterface        _hdql_gScalarFwdQueryIFace;
/** Interface for forwarding queries iterating over collections */
HDQL_API extern const struct hdql_CollectionAttrInterface    _hdql_gCollectionFwdQueryIFace;

/*
 * Binary arithmetic operations
 * */

/** Definition data for arithmetic operation access interfaces*/
struct hdql_ArithOpDefData {
    struct hdql_Query * args[2];
    const struct hdql_OperationEvaluator * evaluator;
};

HDQL_API extern const struct hdql_ScalarAttrInterface        _hdql_gScalarArithOpIFace;
HDQL_API extern const struct hdql_CollectionAttrInterface    _hdql_gCollectionArithOpIFace;

HDQL_API int
hdql_reserve_arith_op_collection_key(struct hdql_Key * key
        , const hdql_Datum_t dd_
        , hdql_Context_t context);

/*
 * Filtered compound collection
 **/

HDQL_API extern const struct hdql_ScalarAttrInterface _hdql_gFilteredCompoundIFace;

/*
 * Binding queries, bound attributes and bound compound interface
 **/

/** Attribute, bound to a query (to be finalized with definition data) */
HDQL_API extern const struct hdql_ScalarAttrInterface        _hdql_gBoundQueryIFace;
/** Returns new instance of bound value interface's definition data */
HDQL_API hdql_Datum_t hdql_bound_value_interface_definition_data_init(struct hdql_Query *, hdql_Context_t);
/** Destroys instance of bound value interface's definition data */
HDQL_API void hdql_bound_value_interface_definition_data_destroy(hdql_Datum_t d, hdql_Context_t ctx);

/** Interface to a collection of bound compounds */
HDQL_API extern const struct hdql_CollectionAttrInterface    _hdql_gBindingCompoundCollectionIFace;
/** Definition data for `_hdql_gBindingCompoundCollectionIFace` */
struct hdql_BindingCompoundCollectionDefData {
    struct hdql_Compound * vCompound;  /* has to have at least one bound attribute */
    struct hdql_Query * filterQuery;  /* optional */
};

/** Instantiates bound compound collection definition data */
HDQL_API hdql_Datum_t hdql_bound_compound_collection_interface_definition_data_init(
              struct hdql_Compound * vCompound
            , struct hdql_Query * filterQuery
            , hdql_Context_t context
        );

HDQL_API void hdql_bound_compound_collection_interface_definition_data_destroy(hdql_Datum_t d, hdql_Context_t ctx);

HDQL_API int hdql_bound_compound_key_reserve(struct hdql_Key *, const hdql_Datum_t defData_, hdql_Context_t ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_INTERNAL_INTERFACE_IMPLEMS_H */
