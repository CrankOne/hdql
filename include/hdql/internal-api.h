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

/* from src/compound.c */
const struct hdql_Allocator *hdql__context_get_allocator(struct hdql_Context *);
struct hdql_Compounds *hdql__compounds_create(struct hdql_Compounds *, struct hdql_Context *);
void hdql__compounds_destroy(struct hdql_Compounds *c, struct hdql_Context *);
struct hdql_Compound *hdql__compound_new(const char * name, struct hdql_Context * context);
struct hdql_Compound *hdql__virtual_compound_new(const struct hdql_Compound *parent, struct hdql_Context * context);

/* from src/context.c */
int hdql__context_add_virtual_compound(struct hdql_Context *context, struct hdql_Compound *);

#ifdef __cplusplus
}  // extern "C"
#endif

/* A crude way to define usual type promotion rules, as used in C. */
#define hdql_M_for_each_arith_types_pair(m) \
    m(  int8_t,   uint8_t,  int16_t ) \
    m(  int8_t,   int16_t,  int16_t ) \
    m(  int8_t,  uint16_t,  int32_t ) \
    m(  int8_t,   int32_t,  int32_t ) \
    m(  int8_t,  uint32_t,  int64_t ) \
    m(  int8_t,   int64_t,  int64_t ) \
    m(  int8_t,  uint64_t, uint64_t ) \
    m(  int8_t,     float,    float ) \
    m(  int8_t,    double,   double ) \
    m( uint8_t,   int16_t,  int16_t ) \
    m( uint8_t,  uint16_t, uint16_t ) \
    m( uint8_t,   int32_t,  int32_t ) \
    m( uint8_t,  uint32_t, uint32_t ) \
    m( uint8_t,   int64_t,  int64_t ) \
    m( uint8_t,  uint64_t, uint64_t ) \
    m( uint8_t,     float,    float ) \
    m( uint8_t,    double,   double ) \
    m( int16_t,  uint16_t,  int32_t ) \
    m( int16_t,   int32_t,  int32_t ) \
    m( int16_t,  uint32_t,  int64_t ) \
    m( int16_t,   int64_t,  int64_t ) \
    m( int16_t,  uint64_t, uint64_t ) \
    m( int16_t,     float,    float ) \
    m( int16_t,    double,   double ) \
    m(uint16_t,   int32_t,  int32_t ) \
    m(uint16_t,  uint32_t, uint32_t ) \
    m(uint16_t,   int64_t,  int64_t ) \
    m(uint16_t,  uint64_t, uint64_t ) \
    m(uint16_t,     float,    float ) \
    m(uint16_t,    double,   double ) \
    m( int32_t,  uint32_t,  int64_t ) \
    m( int32_t,   int64_t,  int64_t ) \
    m( int32_t,  uint64_t, uint64_t ) \
    m( int32_t,     float,    float ) \
    m( int32_t,    double,   double ) \
    m(uint32_t,   int64_t,  int64_t ) \
    m(uint32_t,  uint64_t, uint64_t ) \
    m(uint32_t,     float,    float ) \
    m(uint32_t,    double,   double ) \
    m( int64_t,  uint64_t, uint64_t ) \
    m( int64_t,     float,    float ) \
    m( int64_t,    double,   double ) \
    m(uint64_t,     float,    float ) \
    m(uint64_t,    double,   double ) \
    m(   float,    double,   double )

#endif  /* H_HDQL_INTERNAL_API_H */
