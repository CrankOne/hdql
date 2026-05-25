#ifndef H_HDQL_CONTEXT_H
#define H_HDQL_CONTEXT_H

#include "hdql/types.h"
#include "hdql/util/allocator.h"

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
HDQL_API hdql_Context_t hdql_context_create(uint32_t flags
        , const struct hdql_Allocator *allocator);

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

/**\brief Used by parser routines to create virtual compound types
 *
 * \return HDQL_ERR_CODE_OK on success, HDQL_ERR_MEMORY on failure.
 * */
HDQL_API int hdql_context_add_virtual_compound(hdql_Context_t, struct hdql_Compound * );

/**\brief Used to keep error details in case of unwidning errors
 *
 * \return HDQL_ERR_MEMORY on memory failure, otherwise HDQL_ERR_CODE_OK.
 * */
HDQL_API int hdql_context_err_push(hdql_Context_t, hdql_Err_t, const char * format, ...);

/**\brief Retrurns true if error stack is not empty */
HDQL_API bool hdql_context_has_errors(hdql_Context_t);

/**\brief Iterates callback over errors starting from the most recent one 
 *
 * Callback returning non-zero will terminate the iteration loop.
 * \returns code returned by last callback or zero.
 * */
HDQL_API int hdql_context_for_every_error(hdql_Context_t
        , int (*callback)(int rc, const char *msg, void *userdata)
        , void *userdata );

/**\brief Removes errors associated with the given context instance */
HDQL_API int hdql_context_wipe_errors(hdql_Context_t);

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

/**\brief Registers data by name in the context
 *
 * \return
 * - HDQL_ERR_CODE_OK when new entry is added normally;
 * - HDQL_ERR_MEMORY when adding failed due to no memory;
 * - HDQL_ERR_NAME_COLLISION when added entry overwrote existing and \p
 *   overwrite was not set;
 * - HDQL_ERR_CODE_OVERRIDDEN when added entry overwrote existing and \p
 *   overwrite was set;
 * - HDQL_ERR_GENERIC otherwise (undefined code returned from container
 *  operation).
 * */
HDQL_API int
hdql_context_custom_data_add(hdql_Context_t context
        , const char * name, void * ptr, bool overwrite);

/**\brief Creates new function operational instance using known definition
 *
 * \note Implemented in `function.cc` */
HDQL_API int
hdql_context_new_function( struct hdql_Context * ctx
        , const char * name
        , struct hdql_Query ** argQueries
        , struct hdql_Func ** dest
        );

/**\brief Returns custom data by name or `NULL`*/
void *
hdql_context_custom_data_get( hdql_Context_t context
                            , const char * name
                            );

/**\brief Removes custom data entry from context by name
 *
 * \p unwind makes the function to recursively unwind contexts if none found
 * in current.
 *
 * \returns 
 *  - HDQL_ERR_CODE_OK on success
 *  - HDQL_ERR_UNKNOWN_ATTRIBUTE when no attribute with such name found
 *  - HDQL_ERR_GENERIC otherwise (undefined code returned from container
 *    operation)
 * */
int
hdql_context_custom_data_erase( hdql_Context_t context
                              , const char * name
                              , bool unwind
                              );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* H_HDQL_CONTEXT_H */
