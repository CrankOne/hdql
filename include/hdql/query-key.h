#ifndef H_HDQL_QUERY_KEY_H
#define H_HDQL_QUERY_KEY_H

#include "hdql/query.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Identifies an element within a collection, opaque.
 *
 * HDQL *collection key* structure identifies a record
 * within a collection. The underlying structure can either refer to a
 * user-specific datum, be a placeholder ("empty" key), refer to a list (for
 * composite keys) or refer to one-of (union) list of keys.
 *
 * Keys may have "label" assigned to them to accomplish runtime reference
 * to the values kept by key during evaluation.
 * */
struct hdql_Key;

typedef struct hdql_Key * hdql_Key_t;

/** Returns true if key is empty, i.e. does not keep any data at all
 *
 * Default state of key created with `hdql_key_new()`. This items are either
 * designated to be extended with "reserve" functions ot used often as
 * placeholders for padding in lists or unions.
 * */
HDQL_API bool hdql_key_is_empty(const struct hdql_Key *);

/**\brief Allocates empty key
 *
 * Uses context to allocate, then forwards to `hdql_key_mark_empty()` to
 * initialize, resulting in an empty key item.
 * */
HDQL_API hdql_Key_t hdql_key_new(struct hdql_Context *);

/**\brief Recursively frees allocated key
 *
 * User may check return type for debugging.
 * \return HDQL_ERR_CODE_OK if suceed
 * \return HDQL_ERR_CONTEXT_INCOMPLETE if context is missing types table.
 *
 * Additionally, forwards return code from key type interface's `destroy()`
 * method.
 */
HDQL_API int hdql_key_destroy(hdql_Key_t, struct hdql_Context *);

/**\brief Marks (re-initializes) key as empty
 *
 * Marks key as one not containing any meaningful data: no datum, no list, no
 * union. Does not destroy the previously associated data. */
HDQL_API void hdql_key_mark_empty(hdql_Key_t);

/**\brief Labels key instance
 *
 * Annotates key instance with the label.
 *
 * \note It is assumed that key item is not yet labeled.
 * \note Label memory assumed to be managed externally.
 * */
HDQL_API void hdql_key_set_label(hdql_Key_t key, const char * label);

/**\brief Retrurns, whether the key item is labeled */
HDQL_API bool hdql_key_is_labeled(hdql_Key_t);

/**\brief Retrurns key label or NULL */
HDQL_API const char * hdql_key_get_label(hdql_Key_t);

/* Datum keys
 */

/**\brief Returns true if key is designated for empty (not the empty, list or union) 
 *
 * \note This function returns `false` for "empty" key.
 */
HDQL_API bool hdql_key_is_datum(const struct hdql_Key *);

/**\brief Designates key to store datum
 *
 * Key expected to be empty. The datum can be NULL. */
HDQL_API void hdql_key_set_datum(hdql_Key_t, hdql_ValueTypeCode_t, hdql_Datum_t);

/** Returns datum key type code */
HDQL_API hdql_ValueTypeCode_t hdql_key_datum_get_type_code(const struct hdql_Key *);

/** Returns key datum */
HDQL_API hdql_Datum_t hdql_key_datum_get(const struct hdql_Key *);

/* Keys list
 */

/** Returns true if key is list (list can be empty) */
HDQL_API bool hdql_key_is_list(const struct hdql_Key *);

/**\brief Marks empty key as list with given number of entries
 *
 * \return HDQL_ERR_GENERIC if key is not empty
 * \return HDQL_ERR_CODE_OK otherwise */
HDQL_API int hdql_key_mark_as_list(hdql_Key_t k, size_t, struct hdql_Context * );

/**\brief Retrieves length of keys list
 *
 * \returns HDQL_ERR_GENERIC if key is not a list, HDQL_ERR_CODE_OK otherwise
 * */
HDQL_API size_t hdql_key_get_list_len(const struct hdql_Key * k);

/**\brief Sets n-th element of key list
 *
 * If given \p element is a simple key, ownership of corresponding datum, keys
 * list or union is transferred (swapped) and element can be destroyed.
 *
 * \note Boundary is not checked.
 *
 * \return HDQL_ERR_GENERIC if key is not a list
 * \return HDQL_ERR_CODE_OK on success
 * */
HDQL_API int hdql_key_swap_list_item(hdql_Key_t k, size_t, hdql_Key_t element);

/**\brief Returns n-th element of key list
 *
 * \note Boundary is not checked.
 *
 * \return NULL if key is not a list */
HDQL_API hdql_Key_t hdql_key_get_list_item(hdql_Key_t k, size_t);

/* Union key
 */

/** Returns true if key is union (can be trivial) */
HDQL_API bool hdql_key_is_union(const struct hdql_Key *);

/* Query key API
 */

/**\brief Reserves key for the query
 *
 * Reserves an entire key chain and must be called on pre-allocated array of
 * keys. The result is always a list of keys.
 * */
HDQL_API int hdql_key_reserve_for_query(struct hdql_Query * q
        , hdql_Key_t k, struct hdql_Context *context);

/**\brief Copies one key into another
 *
 * \note Both key has to have same topology (allocated from same query). */
HDQL_API int hdql_key_copy_value( hdql_Key_t dest
                   , const struct hdql_Key * src
                   , hdql_Context_t ctx );

/**\brief Prints key structure as an ASCII tree */
HDQL_API void hdql_key_print_tree(const struct hdql_Key *k
        , FILE *outFile, struct hdql_Context * context);

/* Flat key view
 */

/**\brief Returns length of array, containing only the meaningful keys */
HDQL_API size_t
hdql_key_flat_view_size( const struct hdql_Key * key
                       , hdql_Context_t ctx
                       );

/**\brief Populates array of pointers to meaningful keys
 *
 * The \p flatViews here contains only pointers from \p key subtree that refers
 * to non-empty datum. \p flatViews must be at least
 * of `hdql_key_flat_view_size()` length.
 *
 * The goal is to provide efficient and deterministic way to collapse redundant
 * and multileveled keys subtree into short excertp, useful for applications.
 *
 * \note Unions in \p key subtree results in NULL datum in the \p flatViews
 *       data during key updates.
 * */
HDQL_API int
hdql_key_flat_view_populate(struct hdql_Key * key, hdql_Key_t * flatView);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_KEY_H */
