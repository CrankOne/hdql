#ifndef H_HDQL_VALUE_H
#define H_HDQL_VALUE_H

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_CollectionKey;  /* fwd */

/**\brief Numeric type used to identify value type 
 *
 * Note that upper limit for type code is defined not by this type size, but
 * rather by macro `HDQL_VALUE_TYPEDEF_CODE_BITSIZE` */
typedef uint16_t hdql_ValueTypeCode_t;

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
    int (*copy)(hdql_Datum_t dest, const hdql_Datum_t src, size_t, hdql_Context_t);

    /* TODO: delete in favor of conversion funcs */
    hdql_Bool_t (*get_as_logic)(const hdql_Datum_t);
    void (*set_as_logic)(hdql_Datum_t, hdql_Bool_t);

    hdql_Int_t (*get_as_int)(const hdql_Datum_t);
    void (*set_as_int)(hdql_Datum_t, hdql_Int_t);

    hdql_Flt_t (*get_as_float)(const hdql_Datum_t);
    void (*set_as_float)(hdql_Datum_t, hdql_Flt_t);

    int (*get_as_string)(const hdql_Datum_t, char * buf, size_t bufSize, hdql_Context_t);
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
int hdql_types_define(struct hdql_ValueTypes *, const struct hdql_ValueInterface *);

/**\brief Defines type alias */
int hdql_types_alias(struct hdql_ValueTypes *, const char *, hdql_ValueTypeCode_t);

/**\brief Retrieves data type definition by index
 *
 * Returns pointer to type definition on success or NULL if not type with such
 * index was defined. */
const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc);

/**\brief Retrieves data type definition by name
 *
 * Returns pointer to type definition on success or NULL on failure. */
const struct hdql_ValueInterface * hdql_types_get_type_by_name(const struct hdql_ValueTypes *, const char *);

/**\brief Retrieves data type definition by code
 *
 * Returns pointer to type definition on success or NULL on failure. */
hdql_ValueTypeCode_t hdql_types_get_type_code(const struct hdql_ValueTypes *, const char *);

/**\brief Useful function to add standard C/C++ types to table
 *
 * By default HDQL does not add these types in the table, but user code
 * probably will want to have it. */
int hdql_value_types_table_add_std_types(struct hdql_ValueTypes * vt);

hdql_Datum_t hdql_create_value(hdql_ValueTypeCode_t, hdql_Context_t);
int hdql_destroy_value(hdql_ValueTypeCode_t, hdql_Datum_t, hdql_Context_t);

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

/**\brief Adds new value conversion */
int
hdql_converters_add( struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   , hdql_TypeConverter cnvf
                   );

/**\brief Returns value converter function or NULL if conversion is forbidden */
hdql_TypeConverter
hdql_converters_get( struct hdql_Converters *cnvs
                   , hdql_ValueTypeCode_t to
                   , hdql_ValueTypeCode_t from
                   );

/**\brief Adds standard conversion functions*/
void
hdql_converters_add_std(struct hdql_Converters *cnvs, struct hdql_ValueTypes * vts);

/*                                                            ________________
 * _________________________________________________________/ Constant values
 */

struct hdql_Constants;

enum hdql_ExternValueType {
    hdql_kExternValUndefined = 0,
    hdql_kExternValIntType = 1,
    hdql_kExternValFltType = 2,
};

int
hdql_constants_define_float(struct hdql_Constants *, const char *, hdql_Flt_t);

int
hdql_constants_define_int(struct hdql_Constants *, const char *, hdql_Int_t);

enum hdql_ExternValueType
hdql_constants_get_value(struct hdql_Constants *, const char * name, hdql_Datum_t *);

/*                                                    ________________________
 * _________________________________________________/ External dynamic values
 */

// ...

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_VALUE_H */
