#include "hdql/value.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/query.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST
#   include <gtest/gtest.h>
#endif

struct hdql_ValueTypes {
    std::vector<hdql_ValueInterface> types;
    // Note: by-name index is not unique, it contains aliases
    std::unordered_map<std::string, hdql_ValueTypeCode_t> idxByName;

    // TODO: to support parent one has to rewrite routines as code is no more
    // just an index in vector...
    hdql_ValueTypes * parent;
    hdql_ValueTypes() : parent(nullptr) {}
};

static const size_t gMaxTypeID = HDQL_VALUE_TYPE_CODE_MAX - 1;

extern "C" int
hdql_types_define( hdql_ValueTypes * vt
        , const hdql_ValueInterface * vti
        ) {
    assert(vt);
    assert(vti);
    if(!vti->name) return -1;
    if('\0' == *(vti->name)) return -1;
    // ... other checks for "name"?

    //size_t cSize = 0;
    //{
    //    for(hdql_ValueTypes * cVTypes = vt; cVTypes; cVTypes = cVTypes->parent) {
    //        cSize += vt->types.size();
    //    }
    //}
    if(vt->types.size() == gMaxTypeID) return -3;

    vt->types.push_back(*vti);
    assert(vt->types.size() < gMaxTypeID);
    assert(vt->types.size() <= std::numeric_limits<hdql_ValueTypeCode_t>::max());

    hdql_ValueTypeCode_t tc = static_cast<hdql_ValueTypeCode_t>(vt->types.size());
    auto ir = vt->idxByName.emplace(vti->name, tc);
    if(!ir.second) {
        vt->types.pop_back();
        return -2;
    }
    vt->types.back().name = ir.first->first.c_str();

    return tc;
}

/**\brief Defines type alias */
extern "C" int hdql_types_alias(
          struct hdql_ValueTypes * vt
        , const char * alias
        , hdql_ValueTypeCode_t code
        ) {
    if(NULL == vt) return -1;  // null types table argument
    if(NULL == alias || *alias == '\0') return -2;  // bad type alias name
    if(0x0 == code) return -3;  // bad aliased type code
    if(code > vt->types.size()) return -4;
    auto ir = vt->idxByName.emplace(alias, code);
    return ir.first->second;  // bad aliased type code
}

extern "C" const struct hdql_ValueInterface *
hdql_types_get_type_by_name(const struct hdql_ValueTypes * vt, const char * nm) {
    auto it = vt->idxByName.find(nm);
    if(vt->idxByName.end() == it) return NULL;
    assert(it->second <= vt->types.size());
    return &(vt->types.at(it->second - 1));
}

extern "C" const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc) {
    if(0 == tc) return NULL;
    if(tc > vt->types.size()) return NULL;
    return &(vt->types.at(tc - 1));
}

extern "C" hdql_ValueTypeCode_t
hdql_types_get_type_code(const struct hdql_ValueTypes * vt, const char * name) {
    auto it = vt->idxByName.find(name);
    if(vt->idxByName.end() == it) return 0x0;
    return it->second;
}

// NOT exposed to public header
extern "C" struct hdql_ValueTypes *
_hdql_value_types_table_create(hdql_Context_t ctx) {
    // todo: use ctx-based alloc? Seem to be used mostly during interpretation
    // stage, and if it won't affect performance, there is no need to keep
    // it in fast access area
    return new hdql_ValueTypes;
}

// NOT exposed to public header
extern "C" void
_hdql_value_types_table_destroy(struct hdql_ValueTypes * vt) {
    // todo: use ctx-based free, see note for _hdql_value_types_table_create()
    assert(vt);
    delete vt;
}

extern "C"  hdql_Datum_t
hdql_create_value(hdql_ValueTypeCode_t tc, hdql_Context_t ctx) {
    hdql_ValueTypes * vtx = hdql_context_get_types(ctx);
    assert(vtx);
    if(NULL == vtx) return NULL;
    const struct hdql_ValueInterface * vti = hdql_types_get_type(vtx, tc);
    assert(vti);
    if(NULL == vti) return NULL;
    if(vti->size > 0) {
    if(vti->size <= 0) return NULL;
        // const-size data type
        hdql_Datum_t r = hdql_context_alloc(ctx, vti->size);
        if(vti->init) {
            int rc = vti->init(r, vti->size, ctx);
            if(rc != 0) {
                fprintf( stderr, "Failed to initialize HDQL type instance \"%s\": %d"
                       , vti->name, rc );
                hdql_context_free(ctx, r);
                return NULL;
            }
        }
        return r;
    } else {
        // variadic size data type
        assert(0);  // TODO
        // ...
    }
}

extern "C" int
hdql_destroy_value(hdql_ValueTypeCode_t tc, hdql_Datum_t r, hdql_Context_t ctx) {
    hdql_ValueTypes * vtx = hdql_context_get_types(ctx);
    if(NULL == vtx) return -1;
    const struct hdql_ValueInterface * vti = hdql_types_get_type(vtx, tc);
    if(NULL == vti) return -2;
    if(0 >= vti->size) return -3;
    if(vti->destroy) {
        int rc = vti->destroy(r, vti->size, ctx);
        if(rc != 0) {
            fprintf( stderr, "Failed to properly destroy HDQL type instance \"%s\": %d"
                   , vti->name, rc );
            return -4;
        }
    }
    hdql_context_free(ctx, r);
    return 0;
}

//
// Standard arithmetic types

namespace hdql {

template<typename T, typename=void>
struct StrTraits;

template<typename T>
struct StrTraits<T, typename std::enable_if<std::is_integral<T>::value>::type > {
    static int to_string(const T& value, char * buf, size_t bufSize) {
        assert(buf);
        assert(bufSize > 1);
        size_t nUsed = snprintf(buf, bufSize, "%ld", static_cast<long int>(value));
        if(nUsed >= bufSize) return -1;
        return 0;
    }
    static int from_string(T& value, const char * strexpr) {
        char * endptr = NULL;
        long int v = strtol(strexpr, &endptr, 0);
        if(NULL == endptr || *endptr != '\0') return -1;
        if(v < std::numeric_limits<T>::min()) return -2;
        if(v > std::numeric_limits<T>::max()) return -3;
        value = static_cast<T>(v);
        return 0;
    }
};

template<>
struct StrTraits<long unsigned int, void> {
    static int to_string(const long unsigned int & value, char * buf, size_t bufSize) {
        assert(buf);
        assert(bufSize > 1);
        size_t nUsed = snprintf(buf, bufSize, "%lu", value);
        if(nUsed >= bufSize) return -1;
        return 0;
    }
    static int from_string(long unsigned int & value, const char * strexpr) {
        char * endptr = NULL;
        long int v = strtol(strexpr, &endptr, 0);
        if(NULL == endptr || *endptr != '\0') return -1;
        if(v < 0) return -2;  // todo: properly account for [un]signed long ([u]int64_t)
        value = static_cast<long int>(v);
        return 0;
    }
};

template<typename T>
struct StrTraits<T, typename std::enable_if<std::is_floating_point<T>::value>::type > {
    static int to_string(const T& value, char * buf, size_t bufSize) {
        assert(buf);
        assert(bufSize > 1);
        size_t nUsed = snprintf(buf, bufSize, "%e", static_cast<double>(value));
        if(nUsed >= bufSize) return -1;
        return 0;
    }
    static int from_string(T& value, const char * strexpr) {
        char * endptr = NULL;
        double v = strtod(strexpr, &endptr);
        if(NULL == endptr || *endptr != '\0') return -1;
        if(v < std::numeric_limits<T>::min()) return -2;
        if(v > std::numeric_limits<T>::max()) return -3;
        value = static_cast<T>(v);
        return 0;
    }
};

template<typename T>
struct StdArithInterface {
    static const size_t size = sizeof(T);

    static int copy(hdql_Datum_t dest, const hdql_Datum_t src, size_t byteLen, hdql_Context_t) {
        assert(byteLen == sizeof(T));
        memcpy(dest, src, byteLen);
        return 0;
    }

    static hdql_Bool_t get_as_bool(const hdql_Datum_t d_)
        { return *reinterpret_cast<const T*>(d_) ? 0x1 : 0x0; }
    static void set_as_bool(hdql_Datum_t d_, hdql_Bool_t newVal)
        { *reinterpret_cast<T*>(d_) = newVal; }

    static hdql_Int_t get_as_int(const hdql_Datum_t d_)
        { return static_cast<hdql_Int_t>(*reinterpret_cast<const T*>(d_)); }
    static void set_as_int(hdql_Datum_t d_, hdql_Int_t newVal)
        { *reinterpret_cast<T*>(d_) = newVal; }

    static hdql_Flt_t get_as_float(const hdql_Datum_t d_)
        { return static_cast<hdql_Flt_t>(*reinterpret_cast<const T*>(d_)); }
    static void set_as_float(hdql_Datum_t d_, hdql_Flt_t newVal)
        { *reinterpret_cast<T*>(d_) = newVal; }

    static int get_as_string( const hdql_Datum_t d_, char * buf, size_t bufSize, hdql_Context_t)
        { return StrTraits<T>::to_string(*reinterpret_cast<const T*>(d_), buf, bufSize); }
    static int set_from_string(hdql_Datum_t d_, const char * strexpr, hdql_Context_t)
        { return StrTraits<T>::from_string(*reinterpret_cast<T*>(d_), strexpr); }
};

#if 1
#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST
namespace test {
TEST(CommonTypes, ShortIntegerParsingWorks) {
    // tests that basic parsing works and detects value boundaries (leving
    // original value intact)
    int16_t v;
    int rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "32767", NULL);
    EXPECT_EQ(rc, 0);  // ok
    EXPECT_EQ(v, 32767);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "32768", NULL);
    EXPECT_NE(rc, 0);  // overflow
    EXPECT_EQ(v, 32767);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "-32768", NULL);
    EXPECT_EQ(rc, 0);  // ok
    EXPECT_EQ(v, -32768);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "-32769", NULL);
    EXPECT_NE(rc, 0);  // underflow
    EXPECT_EQ(v, -32768);
    rc = StdArithInterface<int16_t>::set_from_string(reinterpret_cast<hdql_Datum_t>(&v), "345foo", NULL);
    EXPECT_NE(rc, 0);  // extra symbols on the tail (considered as error)
    EXPECT_EQ(v, -32768);
}

TEST(CommonTypes, ShortIntegerConversionsWorks) {
    // tests that basic conversions works and detects value boundaries (leving
    // original value intact)
    int16_t v = std::numeric_limits<int16_t>::max();
    char bf[32];
    StdArithInterface<int16_t>::get_as_string(reinterpret_cast<hdql_Datum_t>(&v)
            , bf, sizeof(bf), NULL);
    EXPECT_STREQ(bf, "32767");
    // TODO: ... set as over-/underflow
}
}  // namespace
#endif
#endif
}  // namespace hdql

// TODO: whether or not current platform supports these types may change, we
// have to support it
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
    // ... other standard types?


namespace {
int
_str_init( hdql_Datum_t
         , size_t
         , hdql_Context_t
         ) {
    assert(0);  // TODO
}

int
_str_destroy( hdql_Datum_t
            , size_t
            , hdql_Context_t
            ) {
    assert(0);  // TODO
}

int
_str_copy( hdql_Datum_t dest
          , const hdql_Datum_t src
          , size_t
          , hdql_Context_t
          ) {
    assert(0);  // TODO
}
}  // anonymous namespace

extern "C" int
hdql_value_types_table_add_std_types(hdql_ValueTypes * vt) {
    int hadErrors = 0;
    #define _M_add_std_type(nm, ctp) \
    { \
        hdql_ValueInterface vti; \
        vti.name = nm; \
        vti.init = NULL; \
        vti.destroy = NULL; \
        vti.size = hdql::StdArithInterface<ctp>::size; \
        vti.isVariadic = 0x0; \
        vti.copy = &hdql::StdArithInterface<ctp>::copy; \
        vti.get_as_logic = &hdql::StdArithInterface<ctp>::get_as_bool; \
        vti.set_as_logic = &hdql::StdArithInterface<ctp>::set_as_bool; \
        vti.get_as_int = &hdql::StdArithInterface<ctp>::get_as_int; \
        vti.set_as_int = &hdql::StdArithInterface<ctp>::set_as_int; \
        vti.get_as_float = &hdql::StdArithInterface<ctp>::get_as_float; \
        vti.set_as_float = &hdql::StdArithInterface<ctp>::set_as_float; \
        vti.get_as_string = &hdql::StdArithInterface<ctp>::get_as_string; \
        vti.set_from_string = &hdql::StdArithInterface<ctp>::set_from_string; \
        int rc = hdql_types_define(vt, &vti); \
        if(rc < 1) { hadErrors = 1; } \
    }
    _M_hdql_for_each_std_type(_M_add_std_type);
    #undef _M_add_std_type
    if(hadErrors) return hadErrors;
    // add aliases
    
    #define _M_add_alias(stdType, alias) { \
        assert(sizeof(stdType) == sizeof(alias)); \
        hdql_ValueTypeCode_t tc = hdql_types_get_type_code(vt, #stdType); \
        assert(0x0 != tc); \
        hadErrors = hdql_types_alias(vt, #alias, tc) > 0 ? 0 : 1; \
        assert(0 == hadErrors); \
    }

    _M_add_alias(uint8_t, unsigned char);
    _M_add_alias(int8_t, char);
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
    {
        hdql_ValueInterface vti;
        vti.name = "string";
        vti.init = _str_init;
        vti.destroy = _str_destroy;
        vti.copy = _str_copy;
        //int rc = hdql_types_define(vt, &vti);
        //if(rc < 1) { hadErrors = 1; }
    }

    #if 0
    // TODO: derive it somehow, it can be wrong...
    hdql_ValueTypeCode_t stdUCharTypeCode = hdql_types_get_type_code(vt, "uint8_t");
    assert(0 != stdUCharTypeCode);
    assert(sizeof(unsigned char) == sizeof(uint8_t));
    hadErrors = hdql_types_alias(vt, "unsigned char", stdUCharTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: derive it somehow, it can be wrong...
    hdql_ValueTypeCode_t stdIntTypeCode = hdql_types_get_type_code(vt, "int32_t");
    assert(0 != stdIntTypeCode);
    assert(sizeof(int) == sizeof(int32_t));
    hadErrors = hdql_types_alias(vt, "int", stdIntTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: derive it somehow, it can be wrong...
    hdql_ValueTypeCode_t stdShortTypeCode = hdql_types_get_type_code(vt, "int16_t");
    assert(0 != stdShortTypeCode);
    assert(sizeof(short) == sizeof(int16_t));
    hadErrors = hdql_types_alias(vt, "short", stdShortTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: derive it somehow, it can be wrong...
    hdql_ValueTypeCode_t stdUIntTypeCode = hdql_types_get_type_code(vt, "uint32_t");
    assert(0 != stdUIntTypeCode);
    assert(sizeof(unsigned int) == sizeof(uint32_t));
    hadErrors = hdql_types_alias(vt, "unsigned int", stdUIntTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: derive it somehow, it can be wrong...
    hdql_ValueTypeCode_t stdULongIntTypeCode = hdql_types_get_type_code(vt, "uint64_t");
    assert(0 != stdULongIntTypeCode);
    assert(sizeof(size_t) == sizeof(uint64_t));
    hadErrors = hdql_types_alias(vt, "unsigned long", stdULongIntTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);
    hadErrors = hdql_types_alias(vt, "size_t",        stdULongIntTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: depends on macro definition
    hdql_ValueTypeCode_t hdqlIntTypeCode = hdql_types_get_type_code(vt, "int64_t");
    assert(0 != hdqlIntTypeCode);
    assert(sizeof(hdql_Int_t) == sizeof(int64_t));
    hadErrors = hdql_types_alias(vt, "hdql_Int_t", hdqlIntTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: depends on macro definition
    hdql_ValueTypeCode_t hdqlFltTypeCode = hdql_types_get_type_code(vt, "double");
    assert(0 != hdqlFltTypeCode);
    assert(sizeof(hdql_Flt_t) == sizeof(double));
    hadErrors = hdql_types_alias(vt, "hdql_Flt_t", hdqlFltTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);

    // TODO: supported only starting from C99
    hdql_ValueTypeCode_t hdqlBoolTypeCode = hdql_types_get_type_code(vt, "bool");
    assert(0 != hdqlBoolTypeCode);
    hadErrors = hdql_types_alias(vt, "hdql_Bool_t", hdqlBoolTypeCode) > 0 ? 0 : 1;
    assert(0 == hadErrors);
    #endif

    return hadErrors;
}

