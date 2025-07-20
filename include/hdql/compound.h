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

/**\brief Retrieves attribute definition by name
 *
 * Recursive by default -- i.e. it will look in parent's attributes if not
 * found in current. */
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

/**\brief Returns number of compound attributes, includeing all parents */
size_t hdql_compound_get_nattrs_recursive(const struct hdql_Compound * c);

/**\brief Retrieves names list of compound attributes 
 *
 * Array of pounters \p dest must be at least of `hdql_compound_get_nattrs()`
 * length. Pointers to attribute name strings are managed by compound (i.e.
 * user code is not responsible for freeing them).
 * */
void hdql_compound_get_attr_names(const struct hdql_Compound * c, const char ** dest);

/**\brief Retrieves names list of all compound attributes, including inherited
 *
 * Array of pounters \p dest must be at least of `hdql_compound_get_nattrs_recursive()`
 * length. Pointers to attribute name strings are managed by compound (i.e.
 * user code is not responsible for freeing them).
 * */
void hdql_compound_get_attr_names_recursive(const struct hdql_Compound * c, const char ** dest);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_COMPOUND_H */
