define(`_M_func', $1_$2_$3)dnl
define(`_M_copy_func', $1_copy)
/* 
 * _M_T
 * */
static hdql_Bool_t _M_func(get, _M_T, as_bool  )(const struct hdql_Datum * d_)         { return *((const _M_T *) (d_));   }
static        void _M_func(set, _M_T, from_bool)(hdql_Datum_t d_, hdql_Bool_t newVal)  { *((_M_T *) (d_)) = newVal;       }
static hdql_Int_t _M_func(get, _M_T, as_int  )(const struct hdql_Datum * d_)           { return *((const _M_T *) (d_));   }
static       void _M_func(set, _M_T, from_int)(hdql_Datum_t d_, hdql_Int_t newVal)     { *((_M_T *) (d_)) = newVal;       }
static hdql_Flt_t _M_func(get, _M_T, as_float  )(const struct hdql_Datum * d_)         { return *((const _M_T *) (d_));   }
static       void _M_func(set, _M_T, from_float)(hdql_Datum_t d_, hdql_Flt_t newVal) { *((_M_T *) (d_)) = newVal;       }

static int _M_func(get, _M_T, as_string)(const struct hdql_Datum *d_, char *buf, size_t bufSize, hdql_Context_t context) {
    ((void) context);  /* TODO: will provide formatting manipulators in future */
    if(0 == bufSize) return HDQL_ERR_MEMORY;
    ifdef(`_M_to_string', _M_to_string, `int nUsed = snprintf(buf, bufSize, _M_fmt, *((const _M_T *) d_));')
    if(nUsed < 0) return HDQL_ERR_CONVERSION;
    if((size_t)nUsed >= bufSize) return HDQL_ERR_MEMORY;
    return HDQL_ERR_CODE_OK;
}

static int _M_func(set, _M_T, from_string)(hdql_Datum_t d_, const char *strexpr, hdql_Context_t) {
    _M_from_string
}

static int _M_copy_func(_M_T)(hdql_Datum_t dest, const struct hdql_Datum * src, size_t byteLen, hdql_Context_t context) {
    ((void) context);
    assert(byteLen == sizeof(_M_T));
    memcpy(dest, src, byteLen);
    return 0;
}

