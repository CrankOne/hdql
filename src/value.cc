#include "hdql/value.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/query.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST
#   include <gtest/gtest.h>
#endif

#ifndef HDQL_TIER_BITSIZE
/**\brief Defines number of bits used to store the type table's tier */
#   define HDQL_TIER_BITSIZE 4
#endif

#if HDQL_VALUE_TYPEDEF_CODE_BITSIZE <= HDQL_TIER_BITSIZE
/* One have to fix it either by increasing `HDQL_VALUE_TYPEDEF_CODE_BITSIZE`
 * in the project's types.h, or by decreasing `HDQL_TIER_BITSIZE` specified
 * for this file */
#   error("Bit size of type table tier is greater than type code bitsize.")
#endif

static const hdql_ValueTypeCode_t gMaxTypeID = HDQL_VALUE_TYPE_CODE_MAX;
static const hdql_ValueTypeCode_t gMaxTypeIDInTier
        = ((0x1 << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE)) - 1);
static const hdql_ValueTypeCode_t gTierMask
        = HDQL_VALUE_TYPE_CODE_MAX & (~gMaxTypeIDInTier);

static uint8_t
_get_tier_number(hdql_ValueTypeCode_t code) {
    return (code & gTierMask) >> (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
}

struct hdql_ValueTypes {
    // name-to-code index is not unique, it contains aliases
    std::unordered_map<std::string, hdql_ValueTypeCode_t> _codeByName;
    // code-to-item index, unique, does not account for tier
    std::unordered_map<hdql_ValueTypeCode_t, hdql_ValueInterface> _itemByCode;
    // tier code, max is 32 (4 bits)
    const uint8_t _tier;
    hdql_ValueTypes * _parent;
public:
    hdql_ValueTypes(hdql_ValueTypes * parent=nullptr)
        : _tier(parent ? parent->_tier + 1 : 1)
        , _parent(parent)
        {
        if((((hdql_ValueTypeCode_t) _tier) << 10) > gMaxTypeID) {
            throw std::runtime_error("Can't create more"
                    " children HDQL types tables" );
        }
    }

    int define(const hdql_ValueInterface & iface) {
        if(NULL == iface.name || *iface.name == '\0') {
            return -1;  // bad name
        }
        size_t nItems = _itemByCode.size();
        if(nItems >= gMaxTypeIDInTier) {
            return -2;  // IDs exceed for tier
        }
        hdql_ValueTypeCode_t tCode = static_cast<hdql_ValueTypeCode_t>(nItems);
        tCode |= ((hdql_ValueTypeCode_t) _tier) << (HDQL_VALUE_TYPEDEF_CODE_BITSIZE - HDQL_TIER_BITSIZE);
        auto ir = _codeByName.emplace(iface.name, tCode);
        if(!ir.second) return -3;  // duplicating name
        auto iir = _itemByCode.emplace(tCode, iface);
        if(!iir.second) {
            _codeByName.erase(ir.first);
            return -4;  // internal logic error
        }
        return 0;
    }

    int define_alias(hdql_ValueTypeCode_t tgtCode, const char * alias) {
        assert(0 != tgtCode);  // we assume user code controlled it
        uint8_t tierNo = _get_tier_number(tgtCode);
        assert(tierNo);
        if(tierNo > _tier) return -1;  // type is from children context
        if(tierNo < _tier) {  // aliasing parent's type requested
            assert(_parent);  // logic error (tierNo must be zero in case this assertion failed)
            // assure type exist in parent tables
            auto vti = _parent->get_by_code(tgtCode);
            if(!vti) return -2;  // no target type
            auto ir = _codeByName.emplace(alias, tgtCode);
            if(ir.second) return HDQL_ERR_CODE_OK;
            if(ir.first->second != tgtCode) return -3;  // collision
            return 1;  // repeatative insertion
        }
        auto it = _itemByCode.find(tgtCode);
        if(_itemByCode.end() == it) return -2;  // no type to alias
        auto ir = _codeByName.emplace(alias, tgtCode);
        if(!ir.second) {
            return ir.second == tgtCode ? 1 : -3;  // repeatative / collision
        }
        return HDQL_ERR_CODE_OK;
    }

    hdql_ValueTypeCode_t get_code_by_name(const char * name) const {
        auto it = _codeByName.find(name);
        if(_codeByName.end() == it) {
            if(!_parent) return 0x0;
            return hdql_types_get_type_code(_parent, name);
        }
        return it->second;
    }

    const hdql_ValueInterface * get_by_name(const char * name) const {
        auto it = _codeByName.find(name);
        if(_codeByName.end() == it) {
            if(!_parent) return NULL;
            return hdql_types_get_type_by_name(_parent, name);
        }
        #ifndef NDEBUG
        {
            uint8_t tierNo = _get_tier_number(it->second);
            assert(tierNo == _tier);
        }
        #endif
        auto iit = _itemByCode.find(it->second);
        assert(iit != _itemByCode.end());
        return &(iit->second);
    }

    const hdql_ValueInterface * get_by_code(hdql_ValueTypeCode_t code) const {
        uint8_t tierNo = _get_tier_number(code);
        if(tierNo == _tier) {
            auto it = _itemByCode.find(code);
            if(_itemByCode.end() != it) return &(it->second);
            return NULL;
        }
        // otherwise, do parent lookup
        if(tierNo > _tier) {
            throw std::runtime_error("Logic error: type of descendant context requested.");
        }
        return _parent->get_by_code(code);
    }
};  // struct hdql_ValueTypes

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST
namespace test {
TEST(CommonTypes, BasicTypeDifinitionWorks) {
    hdql_ValueTypes vts;
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);
    EXPECT_EQ(vts.get_code_by_name("TypeThree"), 0x0);
    EXPECT_STREQ(vts.get_by_name("TypeOne")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("TypeTwo")->name, iface2.name);
    EXPECT_FALSE(vts.get_by_name("TypeThree"));
}  // TEST(CommonTypes, BasicTypeDifinitionWorks)

TEST(CommonTypes, AliasedTypeDifinitionWorks) {
    hdql_ValueTypes vts;
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);

    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne"),  0);
    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne2"), 0);
    EXPECT_EQ(vts.define_alias(vts.get_code_by_name("TypeTwo"), "AliasTwo"),  0);

    EXPECT_EQ(vts.get_code_by_name("TypeThree"), 0x0);
    EXPECT_STREQ(vts.get_by_name("TypeOne")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("TypeTwo")->name, iface2.name);
    EXPECT_FALSE(vts.get_by_name("TypeThree"));

    EXPECT_STREQ(vts.get_by_name("AliasOne")->name,  iface1.name);
    EXPECT_STREQ(vts.get_by_name("AliasOne2")->name, iface1.name);
    EXPECT_STREQ(vts.get_by_name("AliasTwo")->name,  iface2.name);
}  // TEST(CommonTypes, AliasedTypeDifinitionWorks)

TEST(CommonTypes, AliasedTypeDifinitionWithParentWorks) {
    hdql_ValueTypes vts;
    hdql_ValueInterface iface1 = {.name = "TypeOne"}
                      , iface2 = {.name = "TypeTwo"};
    EXPECT_EQ(vts.define(iface1), 0);
    EXPECT_EQ(vts.define(iface2), 0);
    EXPECT_NE(vts.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vts.get_code_by_name("TypeTwo"), 0x0);

    hdql_ValueTypes vtsDesc(&vts);
    hdql_ValueInterface iface3 = {.name = "TypeTwo"}  // overrides parent's
                      , iface4 = {.name = "TypeThreeInDesc"};
    EXPECT_EQ(vtsDesc.define(iface3), 0);
    EXPECT_EQ(vtsDesc.define(iface4), 0);

    // it is legal to define alias to parent's type in children
    EXPECT_EQ(vtsDesc.define_alias(vts.get_code_by_name("TypeOne"), "AliasOne"),  0);
    EXPECT_EQ(vtsDesc.define_alias(vtsDesc.get_code_by_name("TypeTwo"), "AliasTwo"),  0);
    
    EXPECT_NE(vts.get_by_name("TypeTwo"), vtsDesc.get_by_name("TypeTwo"));
    EXPECT_STREQ(vtsDesc.get_by_name("TypeTwo")->name, vtsDesc.get_by_name("TypeTwo")->name);

    EXPECT_NE(vtsDesc.get_code_by_name("TypeOne"), 0x0);
    EXPECT_NE(vtsDesc.get_code_by_name("TypeTwo"), 0x0);
    EXPECT_EQ(vtsDesc.get_code_by_name("TypeThree"), 0x0);
    EXPECT_NE(vtsDesc.get_code_by_name("TypeTwo"), vts.get_code_by_name("TypeTwo"));
}  // TEST(CommonTypes, BasicTypeDifinitionWorks)
}  // namespace test
#endif


extern "C" int
hdql_types_define( hdql_ValueTypes * vt
        , const hdql_ValueInterface * vti
        ) {
    assert(vt);
    return vt->define(*vti);
}

extern "C" int hdql_types_alias(
          struct hdql_ValueTypes * vt
        , const char * alias
        , hdql_ValueTypeCode_t code
        ) {
    return vt->define_alias(code, alias);
}

extern "C" const struct hdql_ValueInterface *
hdql_types_get_type_by_name(const struct hdql_ValueTypes * vt, const char * nm) {
    return vt->get_by_name(nm);
}

extern "C" const struct hdql_ValueInterface *
hdql_types_get_type(const struct hdql_ValueTypes * vt, hdql_ValueTypeCode_t tc) {
    assert(vt);
    return vt->get_by_code(tc);
}

extern "C" hdql_ValueTypeCode_t
hdql_types_get_type_code(const struct hdql_ValueTypes * vt, const char * name) {
    return vt->get_code_by_name(name);
}


//                                          __________________________________
// _______________________________________/ Context-private types table mgmnt

// NOT exposed to public header
extern "C" struct hdql_ValueTypes *
_hdql_value_types_table_create(struct hdql_ValueTypes * parent, hdql_Context_t ctx) {
    // todo: use ctx-based alloc? Seem to be used mostly during interpretation
    // stage, and if it won't affect performance, there is no need to keep
    // it in fast access area
    return new hdql_ValueTypes(parent);
}

// NOT exposed to public header
extern "C" void
_hdql_value_types_table_destroy(struct hdql_ValueTypes * vt, hdql_Context_t ctx) {
    // todo: use ctx-based free, see note for _hdql_value_types_table_create()
    assert(vt);
    delete vt;
}

//                                              ______________________________
// ___________________________________________/ Typed value creation/deletion

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

//                              ______________________________________________
// ___________________________/ Standard arithmetic types (arithmetic, string)

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
    int nItem = 0;
    #define _M_add_std_type(nm, ctp) \
    {   ++nItem; \
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
        if(rc != HDQL_ERR_CODE_OK) { return - nItem; } \
    }
    _M_hdql_for_each_std_type(_M_add_std_type);
    #undef _M_add_std_type

    // add aliases
    #define _M_add_alias(stdType, alias) { ++nItem; \
        assert(sizeof(stdType) == sizeof(alias)); \
        hdql_ValueTypeCode_t tc = hdql_types_get_type_code(vt, #stdType); \
        assert(0x0 != tc); \
        int rc = hdql_types_alias(vt, #alias, tc) >= 0 ? 0 : 1; \
        if(rc < HDQL_ERR_CODE_OK) return -nItem; \
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
    {   ++nItem;
        hdql_ValueInterface vti;
        vti.name = "string";
        vti.init = _str_init;
        vti.destroy = _str_destroy;
        vti.copy = _str_copy;
        int rc = hdql_types_define(vt, &vti);
        if(rc < HDQL_ERR_CODE_OK) { return -nItem; }
    }

    return HDQL_ERR_CODE_OK;
}

/*                                                            ________________
 * _________________________________________________________/ Constant values
 */

struct ConstValItem {
    hdql_ExternValueType type;
    union {
        hdql_Int_t asInt;
        hdql_Flt_t asFlt;
    } value;

    ConstValItem(hdql_Flt_t v) : type(hdql_kExternValFltType), value{.asFlt = v} {}
    ConstValItem(hdql_Int_t v) : type(hdql_kExternValIntType), value{.asInt = v} {}
};

struct hdql_Constants {
    hdql_Constants * parent;
    std::unordered_map<std::string, ConstValItem> values;

    hdql_Constants(hdql_Constants * parent_) : parent(parent_) {}
};

extern "C" int
hdql_constants_define_float( struct hdql_Constants * consts
                           , const char * name
                           , hdql_Flt_t value ) {
    assert(consts);
    if(!name || '\0' == *name) {  // TODO: check name for general validity
        return HDQL_ERR_BAD_ARGUMENT;
    }

    auto ir = consts->values.emplace(name, ConstValItem(value));
    if( ir.second ) return HDQL_ERR_CODE_OK;
    if(ir.first->second.value.asFlt == value) return HDQL_ERR_CODE_OK;  // same value, not an error
    return HDQL_ERR_NAME_COLLISION;
}

extern "C" int
hdql_constants_define_int( struct hdql_Constants * consts
                         , const char * name
                         , hdql_Int_t value ) {
    assert(consts);
    if(!name || '\0' == *name) {  // TODO: check name for general validity
        return HDQL_ERR_BAD_ARGUMENT;
    }

    auto ir = consts->values.emplace(name, ConstValItem(value));
    if( ir.second ) return HDQL_ERR_CODE_OK;
    if(ir.first->second.value.asInt == value) return HDQL_ERR_CODE_OK;  // same value, not an error
    return HDQL_ERR_NAME_COLLISION;
}


extern "C" hdql_ExternValueType
hdql_constants_get_value( struct hdql_Constants * consts
                        , const char * name
                        , hdql_Datum_t * destPtr
                        ) {
    assert(consts);
    if(NULL == name || '\0' == *name) return hdql_kExternValUndefined;
    assert(destPtr);

    auto it = consts->values.find(name);
    if(consts->values.end() == it) {
        return consts->parent ? hdql_constants_get_value(consts->parent
                , name, destPtr) : hdql_kExternValUndefined;
    }
    if(hdql_kExternValFltType == it->second.type) {
        *destPtr = reinterpret_cast<hdql_Datum_t>(&(it->second.value.asFlt));
    } else if(hdql_kExternValIntType == it->second.type) {
        *destPtr = reinterpret_cast<hdql_Datum_t>(&(it->second.value.asInt));
    }
    #ifndef NDEBUG
    else {
        assert(0);  // unknown constant value type
    }
    #endif
    return it->second.type;
}

// ... TODO: LNG22. Unit tests for const values 

//                                       _____________________________________
// ____________________________________/ Context-private constants table mgmnt

// NOT exposed to public header
extern "C" struct hdql_Constants *
_hdql_constants_create(struct hdql_Constants * parent, struct hdql_Context * ctx) {
    return new hdql_Constants(parent);
}

// NOT exposed to public header
extern "C" void
_hdql_constants_destroy(struct hdql_Constants * consts, struct hdql_Context * ctx) {
    if(NULL == consts || NULL == ctx) return;
    delete consts;
}
