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

struct hdql_AttributeDefinition;
struct hdql_Compound;
struct hdql_Operations;
struct hdql_ValueTypes;
struct hdql_Func;
struct hdql_Query;

/**\brief Creates new HDQL context
 *
 * Context collects information on various symbols and definitions available
 * during execution.
 *
 * \todo types table will be modified during the lifecycle with subquery types,
 *       so one should anticipate copying of the types table to have some sort
 *       of reentrant constant types table.
 * */
hdql_Context_t hdql_context_create();

/**\brief Used for C-types allocations */
hdql_Datum_t hdql_context_alloc(hdql_Context_t, size_t);

#ifdef HDQL_TYPES_DEBUG
hdql_Datum_t hdql_context_alloc_typed(hdql_Context_t, size_t, const char *);
hdql_Datum_t hdql_context_check_type(hdql_Context_t, hdql_Datum_t, const char *);
#endif

/**\brief Used to free C-type allocations */
int hdql_context_free(hdql_Context_t, hdql_Datum_t);

/**\brief Returns pointer to value types table */
struct hdql_ValueTypes * hdql_context_get_types( hdql_Context_t ctx );

/**\brief Returns pointer to operations table */
struct hdql_Operations * hdql_context_get_operations( hdql_Context_t ctx );

/**\brief Returns functions definitions dictionary */
struct hdql_Functions * hdql_context_get_functions( hdql_Context_t ctx );

/**\brief Allocates "local attribute" */
struct hdql_AttributeDefinition * hdql_context_local_attribute_create( hdql_Context_t ctx );

/**\brief Deletes "local attribute" */
void hdql_context_local_attribute_destroy( hdql_Context_t ctx, struct hdql_AttributeDefinition * );  // TODO

/**\brief Used by parser routines to create virtual compound types */
void hdql_context_add_virtual_compound(hdql_Context_t, struct hdql_Compound * );

/**\brief Used to keep error details in case of unwidning errors */
void hdql_context_err_push(hdql_Context_t, hdql_Err_t, const char * format, ...);

/**\brief Destroys HDQL expression evaluation context */
void hdql_context_destroy(hdql_Context_t);

/**\brief Creates new function operational instance using known definition
 *
 * \note Implemented in `function.cc` */
int
hdql_context_new_function( struct hdql_Context * ctx
        , const char * name
        , struct hdql_Query ** argQueries
        , struct hdql_Func ** dest
        );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* H_HDQL_CONTEXT_H */
