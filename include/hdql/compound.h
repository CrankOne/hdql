#ifndef H_HDQL_COMPOUND_H
#define H_HDQL_COMPOUND_H

#include "hdql/types.h"
#include "hdql/value.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_AttributeDefinition;  /* fwd */
struct hdql_CollectionKey;  /* fwd */
struct hdql_Query; /* fwd */
struct hdql_Function;  /* fwd */

/**\brief Atomic type definition */
struct hdql_AtomicTypeFeatures {
    /** If set, value can not be assigned*/
    hdql_ValueTypeCode_t isReadOnly:1;
    /** Refers to value type interface */
    hdql_ValueTypeCode_t arithTypeCode:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
    /* ... */
};

/**\brief A compound type definition
 *
 * Compound type definition is built from individual attributes definitions.
 * Every attr.def. is referenced by name or integer index and has defined
 * access interface. Access interface can be of scalar type or collection type.
 * */
struct hdql_Compound;

/**\brief Interface to scalar attribute definition (may be of compound type) */
struct hdql_ScalarAttrInterface {
    /** Supplementary data for getter, can be NULL */
    hdql_Datum_t suppData;
    /** Scalar item dereference callback, required */
    hdql_Datum_t (*dereference)(hdql_Datum_t, hdql_Context_t, hdql_Datum_t);
    /** Free supplementary data callback, can be NULL */
    void (*free_supp_data)(hdql_Datum_t, hdql_Context_t);
    // ... TODO: key management
    // ... TODO: seter method?
};

#if 0
/**\brief Custom key interface, used when key type code is not set
 *
 * This functions get called for collection entities when key
 * type can not be defined as atomic HDQL type and needs to be
 * composed from multiple items. */
struct hdql_CustomCollectionKeyInterface {
    /** Custom key allocation function */
    int (*reserve_key)(hdql_SelectionArgs_t, hdql_Datum_t * dest, hdql_Context_t);
    /** Custom key copy function */
    int (*copy_key)(hdql_SelectionArgs_t, hdql_Datum_t * dest, hdql_Context_t);
    /** Custom key delete function */
    int (*destroy_key)(hdql_SelectionArgs_t, hdql_Datum_t   dest, hdql_Context_t);
};
#endif

/**\brief Interface to collection attribute (foreign column or array) */
struct hdql_CollectionAttrInterface {
    /**Collection key type code*/
    hdql_ValueTypeCode_t keyTypeCode;
    /** Should allocate new iterator object. Initialization not needed
     * (instance immediately gets forwarded to `reset()`) */
    hdql_It_t      (*create)        (hdql_Datum_t, hdql_SelectionArgs_t, hdql_Context_t);
    /** Should dereference iterator object AND return NULL if it is not
     * possible (no items available) */
    hdql_Datum_t   (*dereference)   (hdql_Datum_t, hdql_It_t, hdql_Context_t);
    /** Should advance iterator object */
    hdql_It_t      (*advance)       (hdql_Datum_t, hdql_SelectionArgs_t, hdql_It_t, hdql_Context_t);
    /** Should reset iterator object */
    void           (*reset)         (hdql_Datum_t, hdql_SelectionArgs_t, hdql_It_t, hdql_Context_t);
    /** Should delete iterator object */
    void           (*destroy)       (hdql_It_t, hdql_Context_t);
    /** Should extract key value from iterator and copy value. Destination
     * data allocated with respect to `keyTypeCode` */
    void           (*get_key)       (hdql_Datum_t, hdql_It_t, struct hdql_CollectionKey *, hdql_Context_t);

    /** Selection compiler
     *
     * Used to produce selection object based on user's selection
     * expression (usually involves various externally-defined parsers) */
    hdql_SelectionArgs_t (*compile_selection)( const char * expr
        , const struct hdql_AttributeDefinition *
        , hdql_Context_t ctx
        , char * errBuf, size_t errBufLen);
    /**\brief Destroys selection object */
    void (*free_selection)(hdql_Context_t ctx, hdql_SelectionArgs_t);
};

extern const struct hdql_CollectionAttrInterface gSubQueryInterface;

/**\brief Compound's attribute definition descriptor
 *
 * Defines, how certain attribute has to be accessed within a compound type.
 * In terms of data access interface may define either:
 *  - (if `isAtomic` is set) -- atomic attribute, i.e. attribute of simple
 *    arithmetic type; `typeInfo.atomic` struct instance defines
 *    data access interface
 *  - (if `isAtomic` is not set) -- a compound attribute, i.e. attribute
 *    consisting of multiple attributes (atomic or compounds);
 *    `typeInfo.compound` shall be used to access the data in term of
 *    sub-attributes.
 *  - (if `isSubQuery` is set) -- value retrieved using collection interface
 *    based on query object provided instead of atomic or compound interface
 * In terms of how the data is associated to owning object:
 *  - (if `isCollection` is set) -- a collection attribute that should be
 *    accessed via iterator, which lifecycle is steered by `interface.collection`
 *  - (if `isCollection` is not set) -- a scalar attribute (optionally)
 *    providing value which can be retrieved (or set) using
 *    `interface.scalar` interface.
 * Prohibited combinations:
 *  - isSubQuery && !isCollection -- subqueries are always iterable collections,
 *    (yet subquery type must never be a terminal one)
 *  - isSubQuery && staticValue -- "static" values do not need an owning
 *    instance
 * */
struct hdql_AttributeDefinition {
    /** If set, it attribute is of atomic type (int, float, etc), otherwise 
     * it is a compound */
    unsigned int isAtomic:1;
    /** If set, attribute is collection of values indexed by certain key,
     * otherwise it is a scalar (a single value) */
    unsigned int isCollection:1;
    /** If set, attribute should be interpreted as a sub-query */
    unsigned int isSubQuery:1;
    /** If set, attribute has no owner:
     *  0x1 -- constant value (parser should calculate simple arithmetics)
     *  0x2 -- externally set variable (no arithmetics on parsing) */
    unsigned int staticValueFlags:2;
    /** Defines access interface for a value */
    union {
        struct hdql_ScalarAttrInterface     scalar;
        struct hdql_CollectionAttrInterface collection;
    } interface;
    /** Defines value type features */
    union {
        struct hdql_AtomicTypeFeatures   atomic;
        struct hdql_Compound           * compound;
        struct hdql_Query              * subQuery;
        struct {
            hdql_Datum_t datum;
            hdql_ValueTypeCode_t typeCode;
        } staticValue;
    } typeInfo;
};

void hdql_attribute_definition_init(struct hdql_AttributeDefinition *);

/**\brief Creates new "static" compound type */
struct hdql_Compound * hdql_compound_new(const char * name);
/**\brief Creates new "virtual" compound type */
struct hdql_Compound * hdql_virtual_compound_new(const struct hdql_Compound * parent, struct hdql_Context * ctx);
/**\breif Destroys virtual compound instance*/
void hdql_virtual_compound_destroy(struct hdql_Compound *, struct hdql_Context * ctx);
/**\breif Returns parent type of virtual compound */
const struct hdql_Compound * hdql_virtual_compound_get_parent(const struct hdql_Compound * self);
/**\brief Returns true if compound is virtual */
int hdql_compound_is_virtual(const struct hdql_Compound * compound);

/**\brief Adds attribute to compound type definition
 *
 * Attribute name and attribute definition struct instance are copied.
 *
 * \returns   0 on sucess
 * \returns  -1 "name collision"
 * \returns  -2 "index collision"
 * \returns -11 if name NULL or empty
 * \returns -21 "coll. interface definition is not full"
 * \returns -22 "coll. attrs with key retrieval interface must provide key type"
 * \returns -23 "coll. attrs with(out) selectors must provide both ctr and dtr"
 * */
int hdql_compound_add_attr( struct hdql_Compound * instance
                          , const char * attrName
                          , hdql_AttrIdx_t attrIdx
                          , const struct hdql_AttributeDefinition * attrDef
                          );
/**\brief Retrieves attribute definition by name */
const struct hdql_AttributeDefinition *
hdql_compound_get_attr( const struct hdql_Compound *, const char * name );
/**\brief Returns (type) name of the compound object */
const char * hdql_compound_get_name(const struct hdql_Compound *);
/**\brief Deletes compound type */
void hdql_compound_destroy(struct hdql_Compound *);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_COMPOUND_H */
