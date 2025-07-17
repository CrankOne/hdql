#ifndef H_HDQL_COMPOUND_H
#define H_HDQL_COMPOUND_H

#include "hdql/types.h"
#include "hdql/value.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_CollectionKey;  /* fwd */
struct hdql_Query; /* fwd */
struct hdql_Function;  /* fwd */
struct hdql_AttrDef;  /* fwd */

/**\brief A compound type definition
 *
 * Compound type definition is built from individual attributes definitions.
 * Every attr.def. is referenced by name or integer index and has defined
 * access interface. Access interface can be of scalar type or collection type.
 * */
struct hdql_Compound;

/**\brief Creates new "static" compound type */
struct hdql_Compound * hdql_compound_new(const char * name, struct hdql_Context * ctx);
/**\brief Creates new "virtual" compound type */
struct hdql_Compound * hdql_virtual_compound_new(const struct hdql_Compound * parent, struct hdql_Context * ctx);
/**\breif Destroys virtual compound instance*/
void hdql_virtual_compound_destroy(struct hdql_Compound *, struct hdql_Context * ctx);
/**\breif Returns parent type of virtual compound */
const struct hdql_Compound * hdql_virtual_compound_get_parent(const struct hdql_Compound * self);
/**\brief Returns true if compound is virtual */
int hdql_compound_is_virtual(const struct hdql_Compound * compound);
/**\brief Returns true if both compounds are of the same type */
bool hdql_compound_is_same(const struct hdql_Compound * compoundA, const struct hdql_Compound * compoundB);

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
                          , struct hdql_AttrDef * attrDef
                          );
/**\brief Retrieves attribute definition by name */
const struct hdql_AttrDef *
hdql_compound_get_attr( const struct hdql_Compound *, const char * name );
/**\brief Returns (type) name of the compound object */
const char * hdql_compound_get_name(const struct hdql_Compound *);
/**\brief Prints full compound type string
 *
 * Renders compound type string expanding virtual compound type recursively,
 * according to following rules:
 * - if it is not a virtual compound, compound name is returned
 * - if is a virtual compound, its (virtual) attributes names will be listed
 *   in curly brackets within `{...}->` string and then parent is considered
 *   as current, repeating
 * Example outputs:
 *  Foo
 *  {a}->Foo
 *  {a, b}->{c}->Foo
 * */
char *
hdql_compound_get_full_type_str( const struct hdql_Compound * c
        , char * buf, size_t bufSize
        );
/**\brief Deletes compound type */
void hdql_compound_destroy(struct hdql_Compound *, hdql_Context_t context);

/**\brief Returns number of compound attributes
 *
 * \note Returned is the only number of current attributes, not including
 *       parent's, if any */
size_t hdql_compound_get_nattrs(const struct hdql_Compound *);

/**\brief Retrieves names list of compound attributes 
 *
 * Array of pounters \p dest must be at least of `hdql_compound_get_nattrs()`
 * length. Pointers to attribute name strings are managed by compound (i.e.
 * user code is not responsible for freeing them).
 * */
void hdql_compound_get_attr_names(const struct hdql_Compound * c, const char ** dest);

#if 0
typedef unsigned int hdql_AttrFlags_t;

extern const hdql_AttrFlags_t hdql_kAttrIsAtomic;
extern const hdql_AttrFlags_t hdql_kAttrIsCollection;
extern const hdql_AttrFlags_t hdql_kAttrIsSubquery;
extern const hdql_AttrFlags_t hdql_kAttrIsStaticValue;

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
struct hdql_AttrDef {
    #if 0
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
    #endif

    hdql_AttrFlags_t attrFlags;
    /**\brief Key type code, can be zero (unset) */
    hdql_ValueTypeCode_t keyTypeCode:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;

    /** Data access interface for a value */
    union {
        struct hdql_ScalarAttrInterface     scalar;
        struct hdql_CollectionAttrInterface collection;
    } interface;

    /**\brief Key management interface */
    union {
        struct hdql_ScalarKeyInterface * scalar;
        struct hdql_CollectionKeyInterface * collection;
    } keyInterface;

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

void hdql_attribute_definition_init(struct hdql_AttrDef *);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_COMPOUND_H */
