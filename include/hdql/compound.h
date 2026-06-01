#ifndef H_HDQL_COMPOUND_H
#define H_HDQL_COMPOUND_H

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_Key;  /* fwd */
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

/**\brief Creates new "static" compound type
 *
 * Creates compound defined by user's schema and adds it to the context's
 * compounds index. \p name and \p typeID gets copied using context allocation.
 * \p typeID can be null.
 *
 * \p rc can't be null and used to communicate error code.
 *
 * \returns New named account instance or NULL if error occured.
 *
 * */
HDQL_API struct hdql_Compound *
hdql_compound_create(const char * name, struct hdql_Context * context
        , int *rc
        , void *typeID, size_t typeIDSize);

/**\brief Creates virtual compound type
 *
 * Creates nameless compound living in current context. Usually
 * defined by queries representing group of attributes based on forwarding
 * queries to non-virtual compounds.
 *
 * \returns New named account instance or NULL if memory error
 *          occured.
 *
 * \todo Check, what's the point of postponing context ownership.
 * */
HDQL_API struct hdql_Compound *
hdql_virtual_compound_create(const struct hdql_Compound * parent, struct hdql_Context * context);

/**\breif Destroys virtual compound instance
 *
 * Destroys transient attributes defined within a virtual compound.
 *
 * \returns HDQL_ERR_CODE_OK on proper cleanup, HDQL_ERR_CODE_OK on failure.
 *
 * \todo Handle allocator failures.
 * */
HDQL_API int hdql_virtual_compound_destroy(struct hdql_Compound *, struct hdql_Context * ctx);

/**\breif Returns parent type of virtual compound */
HDQL_API const struct hdql_Compound * hdql_virtual_compound_get_parent(const struct hdql_Compound * self);

/**\brief Returns true if compound is virtual */
HDQL_API bool hdql_compound_is_virtual(const struct hdql_Compound * compound);

/**\brief Returns true if virtual compound is bound */
HDQL_API bool hdql_virtual_compound_is_bound(const struct hdql_Compound * compound);

/**\brief Returns true if both compounds are of the same type
 *
 * \todo Reserved for possible elaboration in the future.
 * */
HDQL_API bool hdql_compound_is_same(const struct hdql_Compound * compoundA, const struct hdql_Compound * compoundB);

/**\brief Adds attribute to compound type definition
 *
 * Attribute name and attribute definition struct instance are copied.
 *
 * \returns
 * - HDQL_ERR_CODE_OK on sucess;
 * - HDQL_ERR_NAME_COLLISION "name collision";
 * - HDQL_ERR_BAD_ARGUMENT if name NULL or empty;
 * - HDQL_ERR_MEMORY on memory errors;
 * - HDQL_ERR_GENERIC on unspecified error with hash table.
 * */
HDQL_API int hdql_compound_add_attr( struct hdql_Compound * instance
                          , const char * attrName
                          , struct hdql_AttrDef * attrDef
                          );

/**\brief Retrieves attribute definition by name
 *
 * Recursive -- looks in parent's attributes if not found in current. */
HDQL_API const struct hdql_AttrDef *
hdql_compound_get_attr( const struct hdql_Compound *, const char * name );

/**\brief Returns (type) name of the compound object
 *
 * \note for virtual compounds always returns NULL
 * */
HDQL_API const char * hdql_compound_get_name(const struct hdql_Compound *);

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
HDQL_API char *
hdql_compound_get_full_type_str( const struct hdql_Compound * c
        , char * buf, size_t bufSize
        );
/**\brief Deletes compound type */
HDQL_API int hdql_compound_destroy(struct hdql_Compound *, hdql_Context_t context);

/**\brief Returns number of compound attributes
 *
 * \note Returned is the only number of current attributes, not including
 *       parent's, if any */
HDQL_API size_t hdql_compound_get_nattrs(const struct hdql_Compound *);

/**\brief Returns number of compound attributes, includeing all parents */
HDQL_API size_t hdql_compound_get_nattrs_recursive(const struct hdql_Compound * c);

/**\brief Retrieves names list of compound attributes 
 *
 * Array of pounters \p dest must be at least of `hdql_compound_get_nattrs()`
 * length. Pointers to attribute name strings are managed by compound (i.e.
 * user code is not responsible for freeing them).
 * */
HDQL_API void hdql_compound_get_attr_names(const struct hdql_Compound * c, const char ** dest);

/**\brief Retrieves names list of all compound attributes, including inherited
 *
 * Array of pounters \p dest must be at least of `hdql_compound_get_nattrs_recursive()`
 * length. Pointers to attribute name strings are managed by compound (i.e.
 * user code is not responsible for freeing them).
 * */
HDQL_API void hdql_compound_get_attr_names_recursive(const struct hdql_Compound * c, const char ** dest);

/*\brief Iterates over all attributes within a compound with a callback
 *
 * Provided callback should return 0 to proceed iteration, otherwise loop
 * stops.
 *
 * \returns number of attributes iterated.
 * */
HDQL_API size_t
hdql_compound_for_each_own_attribute(const struct hdql_Compound * C
        , int (*cllb)(const char *, size_t, const struct hdql_AttrDef *, void *)
        , void * userdata);


/**\brief Lists of context-bound non-virtual compound type definitions
 *
 * Maintains two indexes: by-name and by-type-id to provide fast lookup
 * for compound type. Note, that by-type-id index is auxiliary one,
 * defined to cope with C++ helper.
 *
 * Instance is bound to context, see `struct hdql_Context`.
 * */
struct hdql_Compounds;

/**\brief Registers new compound by name
 *
 * Appends compounds table with new named entry, optionally annotated
 * with "type id" information.
 *
 * \return
 * - HDQL_ERR_NAME_COLLISION if eponymous compound was already
 *   registered in the current context.
 * - HDQL_ERR_MEMORY in case of memory error
 * - HDQL_ERR_CODE_OVERRIDDEN when the compound was added, but has
 *   overridden entry with same type ID.
 * - HDQL_ERR_GENERIC in case of unspecified error of hash table.
 * - HDQL_ERR_CODE_OK when done.
 * */
HDQL_API int
hdql_compounds_add(struct hdql_Compounds *compounds
        , const char *name, struct hdql_Compound *c
        , void *typeID, size_t typeIDSize
        );

/**\brief Finds and returns compound by name
 *
 * \returns NULL if not found.
 * */
HDQL_API const struct hdql_Compound *
hdql_compounds_get_by_name(const struct hdql_Compounds *compounds
        , const char *name );

/**\brief Finds and returns compound by type ID
 *
 * \returns NULL if not found.
 * */
HDQL_API const struct hdql_Compound *
hdql_compounds_get_by_type_id(const struct hdql_Compounds *compounds
        , const void *typeID, size_t typeIDLen );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_COMPOUND_H */
