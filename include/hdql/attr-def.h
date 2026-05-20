#ifndef H_HDQL_ATTR_DEF_H
#define H_HDQL_ATTR_DEF_H

#include "hdql/types.h"
#include "hdql/value.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_Compound;  /* fwd */
struct hdql_Query;  /* fwd */
struct hdql_Key;  /* fwd */

/**\brief Interface to scalar attribute definition
 *
 * Scalar attributes obeys simple lifecycle:
 *  1. Upon query initialization, the supplementary dynamic data gets
 *     instantiated (`instantiate()`).
 *  2. Upon applying the query, the attribute gets re-set (`reset()`) with an
 *     owning datum instance.
 *  3. Upon retrieving the value, the attribute gets
 *     dereferenced (`dereference()`).
 *  4. Once corresponding query destroyed, a `destroy()` gets called to delete
 *     dynamic data.
 *
 * Note the following important features:
 *  - scalar attribute can emit the key;
 *  - scalar attribute can result in nothing.
 * */
struct hdql_ScalarAttrInterface {
    /** Supplementary data for getter, can be NULL */
    hdql_Datum_t definitionData;
    /**\brief Instantiate dynamic data for scalar attribute
     *
     * This method is optional (can be NULL). Should instantiate
     * dynamic data which is used locally to hold transient items like
     * filtering query results, selection index cache, etc.
     *
     * \return NULL on fatal error
     *
     * \todo Add "enable key retrieval" option.
     * */
    hdql_Datum_t (*instantiate)( hdql_Datum_t newOwner
                               , const hdql_Datum_t defData
                               , hdql_Context_t context
                               );
    /** Scalar item dereference callback, required
     *
     * \return pointer to associated datum or NULL
     *
     * Raises an error (`HDQL_ERR_NO_KEY_SUPPORT), if \p key is not NULL,
     * but keys were not ordered at `create()`.
     *
     * \note The call to dereference *must be idempotent* meaning that in
     *       between of `reset()` calls, the returned value must not change.
     *       Practically, that means that any calculus done upon call of
     *       dereference has to be cached/stored and invalidated only on
     *       next `reset()` call (dynamic data is an appropriate place to
     *       accomodate this persistency, if needed).
     * */
    hdql_Datum_t (*dereference)( hdql_Datum_t root  // owning object
                               , hdql_Datum_t dynData  // allocated with `instantiate()`
                               , const hdql_Datum_t defData  // may be NULL
                               , hdql_Context_t
                               );
    /**\brief Called in case of owner change, shall return new/re-initialized
     *        dynamic data
     *
     * Set to NULL when `instantiate()` is NULL, otherwise required.
     *
     * \return updated or reallocated dynamic data
     */
    hdql_Datum_t (*reset)( hdql_Datum_t newOwner
                 , hdql_Datum_t prevDynData
                 , const hdql_Datum_t defData
                 , struct hdql_Key * key // may be NULL
                 , hdql_Context_t
                 );
    /**\brief Should destroy selection supplementary data for scalar
     *        attribute, can be NULL */
    void (*destroy)( hdql_Datum_t dynData
                   , const hdql_Datum_t defData
                   , hdql_Context_t
                   );
};

/**\brief Collection keys list allocation callback
 *
 * Although keys are explicitly typed, their length can depend on the
 * definition data for synthetic definitions (composed during compilation --
 * forwarding queries, bound attributes, etc).
 * This callback is used to allocate key lists when key type code is not set.
 *
 * \todo Add a dedicated method to support individual element retrieval, as
 *       required by #18
 * */
typedef int (*hdql_ReserveKeysListCallback_t)( struct hdql_Key *,
            const hdql_Datum_t defData, hdql_Context_t );

/**\brief Interface to collection attribute (foreign column or array) */
struct hdql_CollectionAttrInterface {
    /**\brief Static definition data for collection interface */
    hdql_Datum_t definitionData;
    /**\brief Allocates new iterator object
     *
     * Mandatory. Iterator object initialization is not needed at this step.
     *
     * \return NULL on a fatal error, iterator dynamic data otherwise. */
    hdql_It_t      (*create)        ( hdql_Datum_t owner
                                    , const hdql_Datum_t defData
                                    , hdql_SelectionArgs_t
                                    , hdql_Context_t
                                    );
    /**\brief Dereferences iterator object, returning NULL if it is not
     *        possible (no items available) */
    hdql_Datum_t   (*dereference)   ( hdql_It_t );
    /** Should advance iterator object */
    hdql_It_t      (*advance)       ( hdql_It_t, struct hdql_Key * key );
    /**\brief Should reset iterator object
     *
     * Should not return NULL except for unrecoverable errors. Iterator
     * for empty collection should exist (but, probably, in a special
     * state).
     *
     * Provided key is set on the first element, otherwise should be kept
     * intact. Key may not be provided (NULL). */
    hdql_It_t       (*reset)        ( hdql_It_t
                                    , hdql_Datum_t newOwner
                                    , const hdql_Datum_t defData
                                    , hdql_SelectionArgs_t
                                    , struct hdql_Key *
                                    , hdql_Context_t
                                    );
    /** Should delete iterator object */
    void           (*destroy)       ( hdql_It_t
                                    , hdql_Context_t
                                    );

    /**\brief If not NULL, shall produce key selection using externally-defined
     *        parser
     *
     * \p expr is the expression of user-defined grammar (expression is
     * forwarded from HDQL parser as is), \p definitionData is this
     * collection's definition data and current query evaluation context is
     * provided as \p ctx.
     * */
    hdql_SelectionArgs_t (*compile_selection)( const char * expr
                                             , const hdql_Datum_t definitionData
                                             , hdql_Context_t ctx );
    /**\brief Destroys selection object */
    void (*free_selection)( const hdql_Datum_t definitionData
                          , hdql_SelectionArgs_t
                          , hdql_Context_t ctx
                          );
};  /* struct hdql_CollectionAttrInterface */

/**\brief Compound's attribute definition descriptor
 *
 * The *attribute definition* in HDQL is a special descriptive object, defining
 * how certain value is accessed within an owning entity. The attribute's type
 * (atomic or compound, scalar or collection, forwarding or direct query) is
 * the subject of this item. Any compound type is fully defined as a set of
 * this attribute definitions (see `hdql_Compound`).
 *
 * In terms of data access interface may define either:
 *  - (if `isAtomic` is set) -- atomic attribute, i.e. attribute of simple
 *    arithmetic type; `typeInfo.atomic` struct instance defines
 *    data access interface
 *  - (if `isAtomic` is not set) -- a compound attribute, i.e. attribute
 *    consisting of multiple attributes (atomic or compounds);
 *    `typeInfo.compound` shall be used to access the data in term of
 *    sub-attributes.
 *  - (if `isFwdQuery` is set) -- value retrieved using collection interface
 *    based on query object provided instead of atomic or compound interface
 * In terms of how the data is associated to owning object:
 *  - (if `isCollection` is set) -- a collection attribute that should be
 *    accessed via iterator, which lifecycle is steered by `interface.collection`
 *  - (if `isCollection` is not set) -- a scalar attribute (optionally)
 *    providing value which can be retrieved (or set) using
 *    `interface.scalar` interface.
 *
 * There are also two orthogonal properties:
 * - a static value
 * - forwarding queries
 *
 * A very special case is the *forwarding query attribute definition* that is
 * created during HDQL expression interpretation to be combined within a
 * runtime-created compound types (so-called *virtual compounds*). In this
 * case, current attribute definition instance atomic/compound and
 * scalar/collection features are inherited from forwarding result.
 *
 * Prohibited combinations:
 *  - isFwdQuery && staticValue -- "static" values do not need an owning
 *    instance
 * */
struct hdql_AttrDef;  /* opaque type */

typedef const struct hdql_AttrDef * hdql_AttrDef_t;

/**\brief Atomic type definition
 *
 * Special features of an attribute of arithmetic type.
 *
 * \todo De-struct and put into attribute definitions as separate properties.
 * */
struct hdql_AtomicTypeFeatures {
    /** If set, value can not be assigned*/
    hdql_ValueTypeCode_t isReadOnly:1;
    /** Refers to value type interface */
    hdql_ValueTypeCode_t arithTypeCode:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
};

/**\brief Creates attribute definition for new atomic scalar attribute
 *
 * Instance gets allocated within the context, \p typeInfo and \p interface
 * instances are copied.
 * */
HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_atomic_scalar(
          struct hdql_AtomicTypeFeatures        * typeInfo
        , struct hdql_ScalarAttrInterface       * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t
        );

HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_atomic_collection(
          struct hdql_AtomicTypeFeatures        * typeInfo
        , struct hdql_CollectionAttrInterface   * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t
        );

HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_compound_scalar(
          struct hdql_Compound                  * typeInfo
        , struct hdql_ScalarAttrInterface       * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t
        );

HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_compound_collection(
          struct hdql_Compound                  * typeInfo
        , struct hdql_CollectionAttrInterface   * interface
        , hdql_ValueTypeCode_t                    keyTypeCode
        , hdql_ReserveKeysListCallback_t          keyIFace
        , hdql_Context_t
        );

/**\brief Create attribute definition forwarding to a query 
 *
 * Forwarding ADs represents a complex query as an attribute definition.
 * For instance, `.tracks.hits` represents hits collection built from hits
 * collection from all the tracks as one set with composite
 * key (<trackID>, <hitID>).
 *
 * This AD differs "full scalar" (like `.rawData.time`) from collections,
 * analysing full chain. For instance query `.hits.x` leads to a scalar, but
 * the entire chain is a collection still.
 *
 * This kind of attribute definitions created within virtual (transient)
 * compounds in the expressions like `{h:=.hits}` (here `h` is the attribute
 * forwarding query). I.e. forwarding attributes are just aliases, in the
 * context of virtual compounds. */
HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_fwd_query(
          struct hdql_Query * subquery
        , hdql_Context_t
        , int * rc
        );

/**\brief Create definition bound to a query
 *
 * Attribute definition created here are bound to a query result managed
 * externally in the expressions like `{h:=*.hits}` (here `h` is the bound
 * attribute), typically within a Cartesian product within the virtual
 * (transient) compound. In this case such AD behaves like a pivot, creating
 * new virtual compound item per every item in the underlying subquery. */
HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_bound(
          struct hdql_Query * subquery
        , hdql_Context_t context
        , int * rc
        );

HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_static_atomic_scalar_value(
          hdql_ValueTypeCode_t valueType
        , const hdql_Datum_t valueDatum
        , hdql_Context_t
        );

HDQL_API struct hdql_AttrDef *
hdql_attr_def_create_dynamic_value(
          hdql_ValueTypeCode_t valueType
        , hdql_Datum_t (*get)(void *, hdql_Context_t)
        , void * userdata
        , hdql_Context_t
        );

/**\brief Sets "transient" flag on this attribute definition
 *
 * Transient attribute definitions are not attached to compounds and must be
 * deleted by owner instances (usually queries). They refer to access
 * interfaces (scalar or collection) with dynamic data written in "definition
 * data" field that must be deleted
 * by such attr. def-s. 
 *
 * The \p dtr must expect definition data as 1st argument and free it using
 * given context instance as 2nd argument. */
HDQL_API void
hdql_attr_def_set_transient(struct hdql_AttrDef *
        , void (*dtr)(hdql_Datum_t, hdql_Context_t) );


HDQL_API bool hdql_attr_def_is_atomic(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_compound(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_scalar(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_collection(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_static_const_value(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_static_external_value(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_transient(hdql_AttrDef_t);
HDQL_API bool hdql_attr_def_is_bound(hdql_AttrDef_t);

/**\brief Returns type code for (optionally static) atomic value */
HDQL_API hdql_ValueTypeCode_t
hdql_attr_def_get_atomic_value_type_code(const hdql_AttrDef_t);

/**\breif Returns key type code or zero */
HDQL_API hdql_ValueTypeCode_t
hdql_attr_def_get_key_type_code(const hdql_AttrDef_t);

HDQL_API const struct hdql_AtomicTypeFeatures *
hdql_attr_def_atomic_type_info(const hdql_AttrDef_t);

HDQL_API const struct hdql_Compound *
hdql_attr_def_compound_type_info(const hdql_AttrDef_t);

HDQL_API const struct hdql_ScalarAttrInterface *
hdql_attr_def_scalar_iface(const hdql_AttrDef_t);

HDQL_API const struct hdql_CollectionAttrInterface *
hdql_attr_def_collection_iface(const hdql_AttrDef_t);

/**\brief Returns ptr to referenced datum instance */
hdql_Datum_t
hdql_attr_def_get_static_value(const hdql_AttrDef_t);

/**\brief Forwards to collection or scalar reserve keys callback, if set
 *
 * \return HDQL_ERR_GENERIC when collection or scalar interface provided
 *         callback, but callback returned NULL.
 * \return HDQL_ERR_CODE_OK otherwise.
 * */
HDQL_API int hdql_attr_def_reserve_key(const hdql_AttrDef_t, struct hdql_Key *, hdql_Context_t);

/**\brief Retrieves attribute definition recursively, until it is not a fwd query
 *
 * Similar to `hdql_query_top_attr()`. */
HDQL_API hdql_AttrDef_t hdql_attr_def_top_attr(const hdql_AttrDef_t);

HDQL_API void hdql_attr_def_destroy(hdql_AttrDef_t, hdql_Context_t);

/**\brief Puts short one-line string describing attribute definition into strbuf
 *
 * Named `top_attr_str` to reflect the fact that it will resolve forwarding
 * queries, not just print plain AD (yet can be used for that). */
HDQL_API int
hdql_top_attr_str( const struct hdql_AttrDef * subj
              , char * strbuf, size_t buflen
              , hdql_Context_t context
              );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_ATTR_DEF_H */

