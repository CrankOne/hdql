#ifndef H_HDQL_CONTEXT_H
#define H_HDQL_CONTEXT_H

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HDQL_TYPES_DEBUG
#   ifdef __cplusplus
#       define hdql_alloc(ctx, type) \
            reinterpret_cast<type *>(hdql_context_alloc_typed(ctx, sizeof(type), #type))
#       define hdql_cast(ctx, type, ptr) \
            reinterpret_cast<type *>(hdql_context_check_type(ctx, reinterpret_cast<hdql_Datum_t>(ptr), #type))
#   else /* __cplusplus */
#       define hdql_alloc(ctx, type) \
            ((type *) hdql_context_alloc_typed(ctx, sizeof(type), #type))
#       define hdql_cast(ctx, type, ptr) \
            ((type *) hdql_context_check_type(ctx, ptr, #type))
#   endif  /* __cplusplus */
#else  /* HDQL_TYPES_DEBUG */
#   ifdef __cplusplus
#       define hdql_alloc(ctx, type) \
            reinterpret_cast< type *>(hdql_context_alloc(ctx, sizeof(type)))
#       define hdql_cast(ctx, type, ptr) \
            reinterpret_cast<type *>(ptr)
#   else /* __cplusplus */
#       define hdql_alloc(ctx, type) \
            ((type *) (hdql_context_alloc(ctx, sizeof(type))))
#       define hdql_cast(ctx, type, ptr) \
            ((type *) ptr)
#   endif  /* __cplusplus */
#endif /* HDQL_TYPES_DEBUG */

/**\brief When set, context duplicates every error push to stderr
 *
 * By default pushed errors get accumulated in context's queue. Normally, to
 * reveal these messages, user code must retrieve them at some point, but that
 * might be missed or just tedious. This flag enables printing of every pushed
 * error to stderr (in addition to keeping messages pushed in queue). */
#define HDQL_CTX_PRINT_PUSH_ERROR 0x1

/**\brief When set, makes the context to maintain its own random generator
 *
 * Affects only non-root context (the root one always maintains a random
 * generator instance). */
#define HDQL_CTX_LOCAL_RANDGEN 0x2

struct hdql_AttrDef;
struct hdql_Compound;
struct hdql_Operations;
struct hdql_ValueTypes;
struct hdql_Func;
struct hdql_Constants;
struct hdql_Query;
struct hdql_RandGen;

/**\brief Creates new HDQL context
 *
 * Context collects information on various symbols and definitions available
 * during execution.
 *
 * \todo types table will be modified during the lifecycle with subquery types,
 *       so one should anticipate copying of the types table to have some sort
 *       of reentrant constant types table.
 * */
HDQL_API hdql_Context_t hdql_context_create(uint32_t flags);

/**\brief Used for C-types allocations */
HDQL_API hdql_Datum_t hdql_context_alloc(hdql_Context_t, size_t);

#ifdef HDQL_TYPES_DEBUG
HDQL_API hdql_Datum_t hdql_context_alloc_typed(hdql_Context_t, size_t, const char *);
HDQL_API hdql_Datum_t hdql_context_check_type(hdql_Context_t, hdql_Datum_t, const char *);
#endif

/**\brief Used to free C-type allocations */
HDQL_API int hdql_context_free(hdql_Context_t, hdql_Datum_t);


/**\brief Returns pointer to value types table */
HDQL_API struct hdql_ValueTypes * hdql_context_get_types(hdql_Context_t ctx);

/**\brief Returns pointer to operations table */
HDQL_API struct hdql_Operations * hdql_context_get_operations(hdql_Context_t ctx);

/**\brief Returns functions definitions dictionary */
HDQL_API struct hdql_Functions * hdql_context_get_functions(hdql_Context_t ctx);

/**\brief Returns table of atomic type conversions */
HDQL_API struct hdql_Converters * hdql_context_get_conversions(hdql_Context_t ctx);

/**\brief Returns constant values definitions table */
HDQL_API struct hdql_Constants * hdql_context_get_constants(hdql_Context_t ctx);

/**\brief Returns context random generator instance*/
HDQL_API struct hdql_RandGen * hdql_context_get_randgen(hdql_Context_t ctx);

/**\brief Used by parser routines to create virtual compound types */
HDQL_API void hdql_context_add_virtual_compound(hdql_Context_t, struct hdql_Compound * );

/**\brief Used to keep error details in case of unwidning errors */
HDQL_API void hdql_context_err_push(hdql_Context_t, hdql_Err_t, const char * format, ...);

/**\brief Destroys HDQL expression evaluation context */
HDQL_API void hdql_context_destroy(hdql_Context_t);

/**\brief Creates descendant context
 *
 * Descendant context provides access to whatever its parent context instance
 * provides (types, functions, constants, etc), plus its own entites. Destroyed
 * children won't affect parents.
 *
 * Context inheritance model is useful for frameworks maintaining large
 * contexts with common definitions along with smaller children short-lived
 * ones, thus helping to avoid potentially expensive
 * context creation/destruction. Child context should be destroyed with
 * same `hdql_context_destroy()` function.
 * */
HDQL_API hdql_Context_t hdql_context_create_descendant(hdql_Context_t, uint32_t flags);

/**\brief Creates new function operational instance using known definition
 *
 * \note Implemented in `function.cc` */
HDQL_API int
hdql_context_new_function( struct hdql_Context * ctx
        , const char * name
        , struct hdql_Query ** argQueries
        , struct hdql_Func ** dest
        );

/*                                                              ______________
 * ___________________________________________________________/ Variadic data
 */

/**\brief Allocates "variadic datum"
 *
 * \returns NULL on allocation failure.
 * */
HDQL_API hdql_Datum_t hdql_context_variadic_datum_alloc(hdql_Context_t, uint32_t usedSize, uint32_t preallocSize);

/**\brief Returns used size of "variadic datum" in bytes
 *
 * \returns `UINT32_MAX` on error
 * */
HDQL_API uint32_t hdql_context_variadic_datum_size(hdql_Context_t, hdql_Datum_t datumPtr);

/**\brief Re-allocates variadic datum ptr
 *
 * \returns null pointer on error pushing error description in the context
 * */
HDQL_API hdql_Datum_t hdql_context_variadic_datum_realloc(hdql_Context_t, hdql_Datum_t, uint32_t);

/**\brief Deletes variadic datum */
HDQL_API void hdql_context_variadic_datum_free(hdql_Context_t, hdql_Datum_t);

/*                                               _____________________________
 * ____________________________________________/ Custom arbitrary user's data
 */

/**\brief Binds pointer with context instance
 *
 * Adds custom data pointer to context instance to be further retrieved in
 * user functions by name. Returns `-1` on name collision, `0` on success.
 * Ownership is NOT delegated. */
HDQL_API int hdql_context_custom_data_add(hdql_Context_t, const char *, void *);

/**\brief Returns user data pointer by name 
 *
 * Returns NULL if name not found. */
HDQL_API void * hdql_context_custom_data_get(hdql_Context_t, const char *);

/**\brief Erases custom data entry from context
 *
 * Caveat: erasing is performed only within current context ancestry level. If
 * custom data item was added to parent context, this function will not erase
 * it (and returns -1).
 *
 * Return 0 on success, -1 if no custom data found for such name.
 * */
HDQL_API int hdql_context_custom_data_erase(hdql_Context_t, const char *);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* H_HDQL_CONTEXT_H */
