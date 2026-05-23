#ifndef H_HDQL_VALUE_H
#define H_HDQL_VALUE_H

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Type table
 *
 * Collects information about defined types. */
struct hdql_ValueTypes;

/**\brief Defines interface for data type
 *
 * Instances of this struct defines how HDQL has to interact with datum of
 * certain type.
 *
 * \note Any callback pointer here can be NULL indicating that type does not
 *       support eponymous operation.
 * */
struct hdql_ValueInterface {
    /**\brief Human-readable type name, used to uniquely identify type in
     *        various contexts
     *
     * This name most of the time matches C/C++ type name and is often used by
     * user code and helpers to retrieve type code.
     *
     * \note Types can be aliased with `hdql_types_alias()`
     * */
    const char * name;

    /**\brief Static size of the datum of type
     *
     * May be set to zero for dynamically-sized types.
     *
     * \todo Dynamic-sized type definitions needs to be tested */
    size_t size:48;
    /**\brief If set, type is of variadic size */
    size_t isVariadic:1;
    /**\brief Type datum constructor */
    int (*init)(hdql_Datum_t, size_t, hdql_Context_t);
    /**\brief Type datum destructor */
    int (*destroy)(hdql_Datum_t, size_t, hdql_Context_t);
    /**\brief Type datum copy function */
    int (*copy)(hdql_Datum_t dest, const struct hdql_Datum * src, size_t, hdql_Context_t);

    /* TODO: delete in favor of conversion funcs */
    hdql_Bool_t (*get_as_logic)(const struct hdql_Datum *);
    void (*set_as_logic)(hdql_Datum_t, hdql_Bool_t);

    hdql_Int_t (*get_as_int)(const struct hdql_Datum *);
    void (*set_as_int)(hdql_Datum_t, hdql_Int_t);

    hdql_Flt_t (*get_as_float)(const struct hdql_Datum *);
    void (*set_as_float)(hdql_Datum_t, hdql_Flt_t);

    /* TODO: re-consider this (seemed to be useful) */
    int (*get_as_string)(const struct hdql_Datum *, char * buf, size_t bufSize, hdql_Context_t);
    int (*set_from_string)(hdql_Datum_t, const char *, hdql_Context_t);
};

/**\brief Defines new data type
 *
 * Creates *copy* of given interface instance (including name).
 *
 * Returns positive non-zero number on success that is equal to registered type
 * code (so it can be safely casted to `hdql_ValueTypeCode_t`). Returns -1 if
 * `name` is not of permitted format, -2 if a type with such name has been
 * already defined, -3 if maximum number of permitted type definitions exceed. */
HDQL_API int hdql_types_define(struct hdql_ValueTypes *, const struct hdql_ValueInterface *);

/**\brief Defines type alias */
HDQL_API int hdql_types_alias(struct hdql_ValueTypes *, const char *, hdql_ValueTypeCode_t);

/**\brief Retrieves data type definition by index
 *
 * Returns pointer to type definition on success or NULL if not type with such
 * index was defined. */
HDQL_API const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc);

/**\brief Retrieves data type definition by name
 *
 * Returns pointer to type definition on success or NULL on failure. */
HDQL_API const struct hdql_ValueInterface * hdql_types_get_type_by_name(const struct hdql_ValueTypes *, const char *);

/**\brief Retrieves data type definition by code
 *
 * Returns pointer to type definition on success or NULL on failure. */
HDQL_API hdql_ValueTypeCode_t hdql_types_get_type_code(const struct hdql_ValueTypes *, const char *);

/**\brief Useful function to add standard C/C++ types to table
 *
 * By default HDQL does not add these types in the table, but user code
 * probably will want to have it. */
HDQL_API int hdql_value_types_table_add_std_types(struct hdql_ValueTypes * vt);

HDQL_API hdql_Datum_t hdql_create_value(hdql_ValueTypeCode_t, hdql_Context_t);
HDQL_API int hdql_destroy_value(hdql_ValueTypeCode_t, hdql_Datum_t, hdql_Context_t);

/*                                                      ______________________
 * ___________________________________________________/ Data type convertsion
 */

/**\brief Callback type for value conversion
 *
 * Should convert value from `src` to `dest` for assumed types and return
 * `HDQL_ERR_CODE_OK` on success. On failure `HDQL_ERR_CONVERSION` must be
 * returned and ptrs kept intact. */
typedef int
(*hdql_TypeConverter)( struct hdql_Datum * __restrict__ dest
                     , struct hdql_Datum * __restrict__ src
                     );

/**\brief Dictionary of value conversion functions */
struct hdql_Converters;

/**\brief Adds new value conversion
 *
 * \returns
 *  HDQL_ERR_CODE_OK new converter added;
 *  HDQL_ERR_MEMORY can't add new record because of memory issue;
 *  HDQL_ERR_NAME_COLLISION converter with such name already exists.
 *
 * \todo Consider more elaborated algorithm, relying on preserving the sorted
 *       order. Currently suboptimal since linear search is used to detect
 *       collisions.
 * */
HDQL_API int
hdql_converters_add( struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   , hdql_TypeConverter cnvf
                   , hdql_Context_t
                   );

/**\brief Returns value converter function or NULL if conversion is forbidden
 *
 * Recursively expands converters till root. */
HDQL_API hdql_TypeConverter
hdql_converters_get( const struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   );

/* \brief Computes conversion chain
 *
 * Among existing converters (if any), calculates the shortest conversion
 * chain.
 * */
//int
//hdql_converters_get_chained(
//                     const struct hdql_Converters *cnvs
//                   , hdql_ValueTypeCode_t to
//                   , hdql_ValueTypeCode_t from
//                   , hdql_TypeConverterList * conversions
//                   , hdql_Context_t context
//                   );

/**\brief Adds standard conversion functions*/
HDQL_API void
hdql_converters_add_std(struct hdql_Converters *cnvs, struct hdql_ValueTypes * vts, hdql_Context_t);

/*                                                  __________________________
 * _______________________________________________/ Arithmetic types promotion
 */

/**\brief Implements usual arithmetic conversions
 *
 * Having two arithmetic types \p aType and \p bType, returns a type, following
 * result type. This function is not used directly for binary or unary
 * arithmetics (instead, a table for possible combinations is generated), but
 * required by functions. */
HDQL_API hdql_ValueTypeCode_t
hdql_types_numeric_promote(const struct hdql_ValueTypes *
        , hdql_ValueTypeCode_t, hdql_ValueTypeCode_t);

/* 
 * This is rather internal interface used by hdql_ValueTypes' routines. Should
 * be hidden in future, once we'll move to pure C implem.
 */

/** Type promotion table instance */
struct hdql_ArithTypePromotionTable;  /* opaque */

/** Instantiates type promotion table */
HDQL_API struct hdql_ArithTypePromotionTable *
hdql_arith_type_promotion_create(hdql_Context_t context);

/** Destroys type promotion table */
HDQL_API void
hdql_arith_type_promotion_destroy(hdql_Context_t context
        , struct hdql_ArithTypePromotionTable *);

/**\brief Initializes type promotion table with C-like conversion rules
 *
 * Expects standard types are available in the value types table. The
 * conversion rules are hardcoded (yet, the codes are obtained dynamically. */
HDQL_API int
hdql_arith_type_promotion_rebuild(const struct hdql_ValueTypes * vt,
        struct hdql_ArithTypePromotionTable * t);

/** implements logic of hdql_types_numeric_promote(), except for cache mgmnt */
HDQL_API hdql_ValueTypeCode_t
hdql_arith_type_promote(const struct hdql_ArithTypePromotionTable * t
        , hdql_ValueTypeCode_t a, hdql_ValueTypeCode_t b
        );

/*                                                            ________________
 * _________________________________________________________/ Constant values
 */

struct hdql_Constants;

enum hdql_ExternValueType {
    hdql_kExternValUndefined = 0,
    hdql_kExternValIntType = 1,
    hdql_kExternValFltType = 2,
};

HDQL_API int hdql_constants_define_float(struct hdql_Constants *, const char *, hdql_Flt_t);

HDQL_API int hdql_constants_define_int(struct hdql_Constants *, const char *, hdql_Int_t);

HDQL_API enum hdql_ExternValueType hdql_constants_get_value(struct hdql_Constants *, const char * name, hdql_Datum_t *);

HDQL_API int hdql_constants_define_standard_math(struct hdql_Constants *);

/*                                                    ________________________
 * _________________________________________________/ External dynamic values
 */

// ... TODO: LNG23

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_VALUE_H */
