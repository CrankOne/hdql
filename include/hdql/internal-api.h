#ifndef H_HDQL_INTERNAL_API_H
#define H_HDQL_INTERNAL_API_H 1

#include <stdbool.h>
#include <stdlib.h>

/* These functions are used within the library and not supposed to be exported
 * to the public API */

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_ValueTypes;
struct hdql_Context;
struct hdql_AttrDef;

/* from src/values.cc */
struct hdql_ValueTypes * _hdql_value_types_table_create(struct hdql_ValueTypes *, struct hdql_Context *);
void _hdql_value_types_table_destroy(struct hdql_ValueTypes *, struct hdql_Context *);

struct hdql_Constants * _hdql_constants_create(struct hdql_Constants *, struct hdql_Context *);
void _hdql_constants_destroy(struct hdql_Constants *, struct hdql_Context *);

/* from src/operations.cc */
struct hdql_Operations * _hdql_operations_create(struct hdql_Operations *, struct hdql_Context *);
void _hdql_operations_destroy(struct hdql_Operations *, struct hdql_Context *);

/* from src/functions.cc */
struct hdql_Functions * _hdql_functions_create(struct hdql_Functions *, struct hdql_Context *);
void _hdql_functions_destroy(struct hdql_Functions *, struct hdql_Context *);

/* from src/converters.cc */
struct hdql_Converters * _hdql_converters_create(struct hdql_Converters *, struct hdql_Context *);
void _hdql_converters_destroy(struct hdql_Converters *, struct hdql_Context *);

/* from src/rangen.cc */
struct hdql_RandGen * _hdql_randgen_create(struct hdql_RandGen *, struct hdql_Context *);
void _hdql_randgen_destroy(struct hdql_RandGen *, struct hdql_Context *);

/* from src/attr-def.c */
bool hdql__attr_def_is_fwd_query(const struct hdql_AttrDef * ad);
struct hdql_Query * hdql__attr_def_fwd_query(const struct hdql_AttrDef * ad);

/* from src/query-key.c */
struct hdql_Key * hdql__key_get_list_bgn(struct hdql_Key * k);
struct hdql_Key * hdql__key_get_list_at(struct hdql_Key * k, size_t);
struct hdql_Key * hdql__keys_next(struct hdql_Key * k);
struct hdql_Key * hdql__keys_prev(struct hdql_Key * k);
bool hdql__key_is_terminal(const struct hdql_Key * k);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_INTERNAL_API_H */
