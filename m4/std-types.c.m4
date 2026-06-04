dnl
dnl TODO: support for formatting hints, probably implemented as HT in the
dnl context, requested by type or smth...
dnl

#include "hdql/value.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <assert.h>

define(`_M_parse_int', `    char *endptr = NULL;
    long r = strtol(strexpr, &endptr, 0);
    if(endptr == strexpr || (!endptr) || *endptr != 0) return HDQL_ERR_CONVERSION;
    if(r < $2) return HDQL_ERR_CONVERSION;
    if(r > $3) return HDQL_ERR_CONVERSION;
    *(($1 *) d_) = ($1) r;
    return HDQL_ERR_CODE_OK;')dnl
dnl
define(`_M_parse_flt', `    char *endptr = NULL;
    double r = strtod(strexpr, &endptr);
    if(endptr == strexpr || (!endptr) || *endptr != 0) return HDQL_ERR_CONVERSION;
    if(r < $2) return HDQL_ERR_CONVERSION;
    if(r > $3) return HDQL_ERR_CONVERSION;
    *(($1 *) d_) = ($1) r;
    return HDQL_ERR_CODE_OK;')dnl
dnl
dnl Boolean type is slightly special regarding how its string form is handled
define(`_M_T', bool)dnl
define(`_M_to_string', `char *dst = stpncpy(buf, *((bool *) d_) ? "true": "false", bufSize);
    int nUsed=dst - buf;')dnl
define(`_M_from_string', `if(0 == strcmp(strexpr, "true")) {
        *((bool *) d_) = true;
        return HDQL_ERR_CODE_OK;
    } else if(0 == strcmp(strexpr, "false")) {
        *((bool *) d_) = false;
        return HDQL_ERR_CODE_OK;
    }
    return HDQL_ERR_CONVERSION;')dnl
dnl
include(types/implem-std-type-mtds.m4)


define(`_M_T', int8_t)dnl
undefine(`_M_to_string')dnl
define(`_M_fmt', `"%d"')dnl
define(`_M_from_string', _M_parse_int(_M_T, INT8_MIN, INT8_MAX))dnl
include(types/implem-std-type-mtds.m4)

define(`_M_T', uint8_t)dnl
define(`_M_fmt', `"%d"')dnl
define(`_M_from_string', _M_parse_int(_M_T, 0, UINT8_MAX))dnl
include(types/implem-std-type-mtds.m4)


define(`_M_T', int16_t)dnl
define(`_M_fmt', `"%d"')dnl
define(`_M_from_string', _M_parse_int(_M_T, INT16_MIN, INT16_MAX))dnl
include(types/implem-std-type-mtds.m4)

define(`_M_T', uint16_t)dnl
define(`_M_fmt', `"%d"')dnl
define(`_M_from_string', _M_parse_int(_M_T, 0, UINT16_MAX))dnl
include(types/implem-std-type-mtds.m4)


define(`_M_T', int32_t)dnl
define(`_M_fmt', `"%d"')dnl
define(`_M_from_string', _M_parse_int(_M_T, INT32_MIN, INT32_MAX))dnl
include(types/implem-std-type-mtds.m4)

define(`_M_T', uint32_t)dnl
define(`_M_fmt', `"%u"')dnl
define(`_M_from_string', _M_parse_int(_M_T, 0, UINT32_MAX))dnl
include(types/implem-std-type-mtds.m4)


define(`_M_T',  int64_t)dnl
define(`_M_fmt', `"%ld"')dnl
define(`_M_from_string', `    char *endptr = NULL;
    *((_M_T *) d_) = strtol(strexpr, &endptr, 0);
    if(endptr == strexpr || (!endptr) || *endptr != 0) return HDQL_ERR_CONVERSION;
    return HDQL_ERR_CODE_OK;')dnl
include(types/implem-std-type-mtds.m4)

define(`_M_T', uint64_t)dnl
define(`_M_fmt', `"%lu"')dnl
define(`_M_from_string', `    char *endptr = NULL;
    *((_M_T *) d_) = strtoul(strexpr, &endptr, 0);
    if(endptr == strexpr || (!endptr) || *endptr != 0) return HDQL_ERR_CONVERSION;
    return HDQL_ERR_CODE_OK;')dnl
include(types/implem-std-type-mtds.m4)


define(`_M_T', float)dnl
define(`_M_fmt', `"%.4e"')dnl
define(`_M_from_string', _M_parse_flt(_M_T, FLT_MIN, FLT_MAX))dnl
include(types/implem-std-type-mtds.m4)

define(`_M_T', double)dnl
define(`_M_fmt', `"%.8e"')dnl
define(`_M_from_string', _M_parse_flt(_M_T, DBL_MIN, DBL_MAX))dnl
include(types/implem-std-type-mtds.m4)

/*
 * String
 * */

int
str_init( hdql_Datum_t
         , size_t
         , hdql_Context_t
         ) {
    assert(0);  // TODO
}

int
str_destroy( hdql_Datum_t
            , size_t
            , hdql_Context_t
            ) {
    assert(0);  // TODO
}

int
str_copy( hdql_Datum_t dest
          , const struct hdql_Datum * src
          , size_t
          , hdql_Context_t
          ) {
    assert(0);  // TODO
}

#define _M_hdql_for_each_std_type(m)        \
    m( "bool",      bool )                  \
    m( "int8_t",    int8_t )                \
    m( "uint8_t",   uint8_t )               \
    m( "int16_t",   int16_t )               \
    m( "uint16_t",  uint16_t )              \
    m( "int32_t",   int32_t )               \
    m( "uint32_t",  uint32_t )              \
    m( "int64_t",   int64_t )               \
    m( "uint64_t",  uint64_t )              \
    m( "float",     float )                 \
    m( "double",    double )                \
    /* ... */

int
hdql_value_types_table_add_std_types(struct hdql_ValueTypes * vt) {
    int nItem = 0;
    #define _M_add_std_type(nm, ctp) \
    {   ++nItem; \
        struct hdql_ValueInterface vti; \
        vti.name = nm; \
        vti.init = NULL; \
        vti.destroy = NULL; \
        vti.size = sizeof(ctp); \
        vti.isVariadic = 0x0; \
        vti.copy = & ctp ## _copy; \
        vti.get_as_logic = &get_ ## ctp ## _as_bool; \
        vti.set_as_logic = &set_ ## ctp ## _from_bool; \
        vti.get_as_int = &get_ ## ctp ## _as_int; \
        vti.set_as_int = &set_ ## ctp ## _from_int; \
        vti.get_as_float = &get_ ## ctp ## _as_float; \
        vti.set_as_float = &set_ ## ctp ## _from_float; \
        vti.get_as_string = &get_ ## ctp ## _as_string; \
        vti.set_from_string = &set_ ## ctp ## _from_string; \
        int rc = hdql_types_define(vt, &vti, NULL); \
        if(rc != HDQL_ERR_CODE_OK) { return rc; } \
    }
    _M_hdql_for_each_std_type(_M_add_std_type);
    #undef _M_add_std_type

    // add aliases
    #define _M_add_alias(stdType, alias) { ++nItem; \
        assert(sizeof(stdType) == sizeof(alias)); \
        hdql_ValueTypeCode_t tc = hdql_types_get_type_code(vt, #stdType); \
        assert(0x0 != tc); \
        int rc = hdql_types_alias(vt, #alias, tc) >= 0 ? 0 : 1; \
        if(rc < HDQL_ERR_CODE_OK) return rc; \
    }

    _M_add_alias(uint8_t, unsigned char);
    _M_add_alias(int8_t, char);
    _M_add_alias(int8_t, signed char);
    _M_add_alias(uint16_t, unsigned short);
    _M_add_alias(int16_t, short);
    _M_add_alias(uint32_t, unsigned int);
    _M_add_alias(int32_t, int);
    _M_add_alias(uint64_t, unsigned long);
    _M_add_alias(uint64_t, size_t);
    _M_add_alias(int64_t, long);
    _M_add_alias(int64_t, hdql_Int_t);
    _M_add_alias(double, hdql_Flt_t);
    _M_add_alias(bool, hdql_Bool_t);
    // string is variable-sized type
    {   ++nItem;
        struct hdql_ValueInterface vti;
        vti.name = "string";
        vti.init = str_init;
        vti.destroy = str_destroy;
        vti.copy = str_copy;
        int rc = hdql_types_define(vt, &vti, NULL);
        if(rc != HDQL_ERR_CODE_OK) { return rc; }
    }

    return HDQL_ERR_CODE_OK;
}

