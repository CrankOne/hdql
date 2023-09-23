#include "events-struct.hh"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <cstring>
#include <set>
#include <unordered_map>

#include "hdql/helpers/compounds.hh"

#if defined(BUILD_GT_UTEST) && BUILD_GT_UTEST  // BasicInterfacesTests {{{

namespace hdql {
namespace test {
/// Some primitive selection type for tests
typedef std::pair<size_t, size_t> SimpleRangeSelection;

static SimpleRangeSelection
_compile_simple_selection(const char * expression_) {
    assert(expression_);
    assert('\0' != *expression_);
    const std::string expression(expression_);
    size_t n = expression.find(':');
    SimpleRangeSelection r;
    std::string fromStr = expression.substr(0, n);
    if(fromStr != "...") {
        r.first = std::stoul(fromStr);
    } else {
        r.first = std::numeric_limits<decltype(r.first)>::min();
    }
    if(std::string::npos != n) {
        std::string toStr = expression.substr(n+1);
        if(toStr != "...") {
            r.second = std::stoul(toStr);
        } else {
            r.second = std::numeric_limits<decltype(r.second)>::max();
        }
    } else {
        r.second = r.first + 1;
    }
    return r;
}

}  // namespace ::hdql::test

namespace helpers {
// implement traits specialization for selection type
// - array of arithmetic type
//      * uses element index as an iterator
//      * benefits from advance being forwarded to traits as on reset() sets
//        index to the beginning of range if possible
template<typename T, size_t N>
struct SelectionTraits<test::SimpleRangeSelection, T[N]> {
    //using iterator = typename T::iterator;
    static size_t advance( T * owner
            , const test::SimpleRangeSelection * sel
            , size_t current) {
        if(current >= N) return N;
        if(sel) {
            if(current >= sel->second) return N;
            assert(current >= sel->first);
            return ++current;
        } else {
            return ++current;
        }
    }

    static size_t reset( T & owner
                       , const test::SimpleRangeSelection * sel
                       , size_t current) {
        if(sel) {
            return sel->first;
        }
        return 0;
    }

    static test::SimpleRangeSelection *
    compile( const char * strexpr
                , const hdql_Datum_t defData
                , hdql_Context & context) {
        hdql_Datum_t buf = hdql_context_alloc(&context, sizeof(test::SimpleRangeSelection));
        return new (buf) test::SimpleRangeSelection(test::_compile_simple_selection(strexpr));
    }

    static void destroy( test::SimpleRangeSelection * selPtr
                       , const hdql_Datum_t defData
                       , hdql_Context & context ) {
        selPtr->test::SimpleRangeSelection::~SimpleRangeSelection();
        hdql_context_free(&context, reinterpret_cast<hdql_Datum_t>(selPtr));
    }
};
// - unordered map
//      * implements simple advance-and-check strategy as keys can have
//        arbitrary layout within unordered container
template<typename KeyT, typename ValueT>
struct SelectionTraits< test::SimpleRangeSelection, std::unordered_map<KeyT, ValueT> > {
    using Container = std::unordered_map<KeyT, ValueT>;
    using iterator = typename Container::iterator;
    static iterator advance( Container & owner
            , const test::SimpleRangeSelection * sel
            , iterator it) {
        if(!sel) {
            if(it != owner.end()) ++it;
            return it;
        }
        do {
            ++it;
        } while( it != owner.end()
            && (it->first < sel->first || it->first >= sel->second)
            );
        return it;
    }

    static iterator reset( Container & owner
                       , const test::SimpleRangeSelection * sel
                       , iterator current) {
        if(NULL == sel) return owner.begin();
        auto it = owner.begin();
        while( it != owner.end()
            && (it->first < sel->first || it->first >= sel->second)
            ) {
            ++it;
        }
        return it;
    }

    static test::SimpleRangeSelection *
    compile( const char * strexpr
           , const hdql_Datum_t defData
           , hdql_Context & context) {
        hdql_Datum_t buf = hdql_context_alloc(&context, sizeof(test::SimpleRangeSelection));
        return new (buf) test::SimpleRangeSelection(test::_compile_simple_selection(strexpr));
    }

    static void destroy( test::SimpleRangeSelection * selPtr
                       , const hdql_Datum_t defData
                       , hdql_Context & context ) {
        selPtr->test::SimpleRangeSelection::~SimpleRangeSelection();
        hdql_context_free(&context, reinterpret_cast<hdql_Datum_t>(selPtr));
    }
};
// - vector
//      * an example of benefit provided by advance being forwarded to traits
//      * may directly set an iterator to the beginning of selected range thus
//        omitting unnecessary advance-and-check loop
template<typename ValueT>
struct SelectionTraits< test::SimpleRangeSelection, std::vector<ValueT> > {
    using Container = std::vector<ValueT>;
    using iterator = typename Container::iterator;
    static iterator advance( Container & owner
            , const test::SimpleRangeSelection * sel
            , iterator it) {
        if(!sel) {
            if(it != owner.end()) ++it;
            return it;
        }
        if(owner.end() == it) return it;
        if(static_cast<size_t>(std::distance(owner.begin(), it)) < sel->second) ++it;
        return it;
    }

    static iterator reset( Container & owner
                       , const test::SimpleRangeSelection * sel
                       , iterator current) {
        if(NULL == sel)
            return owner.begin();
        if(owner.size() < sel->first) return owner.end();  // sleection range left boundary exceeds vec's size
        auto it = owner.begin();
        std::advance(it, sel->first);
        return it;
    }

    static test::SimpleRangeSelection *
    compile( const char * strexpr
           , const hdql_Datum_t defData
           , hdql_Context & context) {
        hdql_Datum_t buf = hdql_context_alloc(&context, sizeof(test::SimpleRangeSelection));
        return new (buf) test::SimpleRangeSelection(test::_compile_simple_selection(strexpr));
    }

    static void destroy( test::SimpleRangeSelection * selPtr
                       , const hdql_Datum_t defData
                       , hdql_Context & context ) {
        selPtr->test::SimpleRangeSelection::~SimpleRangeSelection();
        hdql_context_free(&context, reinterpret_cast<hdql_Datum_t>(selPtr));
    }
};
}  // namespace ::hdql::helpers
}  // namespace hdql

//                                          __________________________________
// _______________________________________/ Tests for C++ templated interfaces

//
// Scalar attribute
TEST(CppTemplatedInterfaces, ScalarAttributeAccess) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::time
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::RawData::time>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface = hdql::helpers::IFace<&hdql::test::RawData::time>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( rawDataCompound
                              , "time"
                              , ad);
    }
    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(rawDataCompound, "time");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_TRUE(hdql_attr_def_is_scalar(ad));
    ASSERT_FALSE(hdql_attr_def_is_collection(ad));

    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    struct hdql::test::RawData rawDataInstance = { .time = 1.23
        , .samples = {1, 2, 3, 4} };

    const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(ad);
    // we expect scalar attribute access interface to have only
    // one method set
    ASSERT_FALSE(iface->definitionData);
    ASSERT_FALSE(iface->instantiate);
    ASSERT_TRUE(iface->dereference);
    ASSERT_FALSE(iface->destroy);
    ASSERT_FALSE(iface->reset);

    // dereference without a key
    hdql_Datum_t result = iface->dereference( reinterpret_cast<hdql_Datum_t>(&rawDataInstance)
            , NULL  // no dynamic data for simple scalar attribute
            , NULL  // key to (reserved) pointer, optional
            , iface->definitionData
            , context
            );

    ASSERT_TRUE(result);
    ASSERT_EQ( *reinterpret_cast<decltype(hdql::test::RawData::time) *>(result)
             , rawDataInstance.time );

    // dereference 2nd time with a key (won't be set)
    hdql_CollectionKey key;
    key.code = 0x0;
    bzero(&key.pl, sizeof(key.pl));
    result = iface->dereference( reinterpret_cast<hdql_Datum_t>(&rawDataInstance)
            , NULL  // no dynamic data for simple scalar attribute
            , NULL  // key to (reserved) pointer, optional
            , iface->definitionData
            , context
            );
    EXPECT_EQ(key.code, 0x0);
    EXPECT_FALSE(key.pl.datum);
    ASSERT_TRUE(result);
    ASSERT_EQ( *reinterpret_cast<decltype(hdql::test::RawData::time) *>(result)
             , rawDataInstance.time );

    hdql_compound_destroy(rawDataCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}}

//
// Array of atomic values attribute, no selection
TEST(CppTemplatedInterfaces, AtomicArrayAttributeAccessNoSelection) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::RawData::samples>::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::RawData::samples>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( rawDataCompound
                              , "samples"
                              , ad);
    }
    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(rawDataCompound, "samples");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_FALSE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_collection(ad));

    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    struct hdql::test::RawData rawDataInstance[2] = {
          { .time = 1.23, .samples = {1, 2, 3, 4} }
        , { .time = 3.21, .samples = {11, 12, 13, 14} }
        };

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->create);
    ASSERT_TRUE(iface->dereference);
    ASSERT_TRUE(iface->advance);
    ASSERT_TRUE(iface->reset);
    ASSERT_TRUE(iface->destroy);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(valTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_CollectionKey key;
    key.isList = false;
    key.code = keyTypeCode;
    key.pl.datum = hdql_context_alloc(context, keyIFace->size);
    ASSERT_TRUE(key.pl.datum);
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_EQ(kCode, key.code);
    }

    hdql_It_t it;
    const size_t nSamples = sizeof(hdql::test::RawData::samples)/sizeof(*hdql::test::RawData::samples);
    for(int i = 0; i < 4; ++i) {
        struct hdql::test::RawData & root = rawDataInstance[i%2];
        if(0 == i) {
            it = iface->create( reinterpret_cast<hdql_Datum_t>( &root )
                              , iface->definitionData
                              , NULL
                              , context );
        }
        it = iface->reset( it, reinterpret_cast<hdql_Datum_t>( &root )
                         , iface->definitionData
                         , NULL
                         , context );
        ASSERT_TRUE(it);
        for(size_t j = 0; j < nSamples; ++j) {
            hdql_Datum_t value = iface->dereference(it, (i >> 1) ? &key : NULL );  // (i/2) ? key : NULL);
            EXPECT_EQ(*reinterpret_cast<std::remove_reference<decltype(*hdql::test::RawData::samples)>::type *>(value)
                    , root.samples[j] );
            if(i >> 1) {
                EXPECT_EQ(*reinterpret_cast<size_t*>(key.pl.datum), ((size_t) j));
            }

            it = iface->advance(it);
            ASSERT_TRUE(it);
        }
    }
    iface->destroy(it, context);
    hdql_context_free(context, key.pl.datum);

    hdql_compound_destroy(rawDataCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}} TEST(CppTemplatedInterfaces, AtomicArrayAttributeAccessNoSelection)

//
// Array of atomic values attribute, no selection
TEST(CppTemplatedInterfaces, AtomicArrayAttributeAccessWithSelection) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace< &hdql::test::RawData::samples
                                  , hdql::test::SimpleRangeSelection
                                  >::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace< &hdql::test::RawData::samples
                                  , hdql::test::SimpleRangeSelection
                                  >::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( rawDataCompound
                              , "samples"
                              , ad);
    }
    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(rawDataCompound, "samples");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_FALSE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_collection(ad));

    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    ASSERT_FALSE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    struct hdql::test::RawData rawDataInstance[2] = {
          { .time = 1.23, .samples = {1, 2, 3, 4} }
        , { .time = 3.21, .samples = {11, 12, 13, 14} }
        };

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->create);
    ASSERT_TRUE(iface->dereference);
    ASSERT_TRUE(iface->advance);
    ASSERT_TRUE(iface->reset);
    ASSERT_TRUE(iface->destroy);
    ASSERT_TRUE(iface->compile_selection);
    ASSERT_TRUE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(valTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_CollectionKey key;
    key.isList = false;
    key.code = keyTypeCode;
    key.pl.datum = hdql_context_alloc(context, keyIFace->size);
    ASSERT_TRUE(key.pl.datum);
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_EQ(kCode, key.code);
    }

    hdql_SelectionArgs_t selArgs = iface->compile_selection("2:4", iface->definitionData, context);
    ASSERT_TRUE(selArgs);

    hdql_It_t it;
    for(int i = 0; i < 4; ++i) {
        struct hdql::test::RawData & root = rawDataInstance[i%2];
        if(0 == i) {
            it = iface->create( reinterpret_cast<hdql_Datum_t>( &root )
                              , iface->definitionData
                              , selArgs
                              , context );
        }
        it = iface->reset( it, reinterpret_cast<hdql_Datum_t>( &root )
                         , iface->definitionData
                         , selArgs
                         , context );
        ASSERT_TRUE(it);
        for(size_t j = 2; j < 4; ++j) {
            hdql_Datum_t value = iface->dereference(it, (i >> 1) ? &key : NULL );  // (i/2) ? key : NULL);
            EXPECT_EQ(*reinterpret_cast<std::remove_reference<decltype(*hdql::test::RawData::samples)>::type *>(value)
                    , root.samples[j] );
            if(i >> 1) {
                EXPECT_EQ(*reinterpret_cast<size_t*>(key.pl.datum), ((size_t) j));
            }

            it = iface->advance(it);
            ASSERT_TRUE(it);
        }
    }
    iface->destroy(it, context);
    hdql_context_free(context, key.pl.datum);

    iface->free_selection(iface->definitionData, selArgs, context);
    hdql_compound_destroy(rawDataCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}} TEST(CppTemplatedInterfaces, AtomicArrayAttributeAccessWithSelection)

//
// Map of compound values attribute
TEST(CppTemplatedInterfaces, MapCompoundAttributeAccessNoSelection) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", context);
    compounds.emplace(typeid(hdql::test::Hit), hitCompound);
    // Add some attribute to hit compound to distinguish hits
    {  // Hit::energyDeposition
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( hitCompound
                              , "energyDeposition"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace<&hdql::test::Track::hits>::type_info(valTypes, compounds);
        assert(typeInfo == hitCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Track::hits>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "uint32_t");  // TODO: DetID_t
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( trackCompound
                              , "hits"
                              , ad);
    }


    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(trackCompound, "hits");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_FALSE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_collection(ad));

    ASSERT_FALSE(hdql_attr_def_is_atomic(ad));
    ASSERT_TRUE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    std::shared_ptr<hdql::test::Hit>
        hit1 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=1.23}),
        hit2 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=2.34}),
        hit3 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=3.45});

    hdql::test::Track track;
    track.hits.emplace(101, hit1);
    track.hits.emplace(102, hit1);
    track.hits.emplace(201, hit2);
    track.hits.emplace(301, hit3);

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->create);
    ASSERT_TRUE(iface->dereference);
    ASSERT_TRUE(iface->advance);
    ASSERT_TRUE(iface->reset);
    ASSERT_TRUE(iface->destroy);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(valTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_CollectionKey key;
    key.isList = false;
    key.code = keyTypeCode;
    key.pl.datum = hdql_context_alloc(context, keyIFace->size);
    ASSERT_TRUE(key.pl.datum);
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(valTypes, "unsigned int");
        ASSERT_EQ(kCode, key.code);
    }

    std::set<hdql::test::DetID_t> hitOccurencies;
    hdql_It_t it = iface->create( reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, NULL, context );
    for( iface->reset(it, reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, NULL, context)
       ;
       ; it = iface->advance(it) ) {
        // try to dereference iterator (may fail)
        hdql_Datum_t hit_ = iface->dereference(it, &key);
        if(!hit_) break;  // if dereference failed, sequence is depleted
        // add key to counts
        assert(key.pl.datum);
        hdql::test::DetID_t hitID = *reinterpret_cast<hdql::test::DetID_t*>(key.pl.datum);
        if(hitID == 101 || hitID == 102) {
            EXPECT_EQ(reinterpret_cast<hdql::test::Hit*>(hit_), hit1.get());  // no implicit copies
        }
        auto ir = hitOccurencies.insert(hitID);
        ASSERT_TRUE(ir.second);
    }
    EXPECT_EQ(hitOccurencies.size(), 4);

    // test hits were met as expected
    auto iit = hitOccurencies.find(101);
    EXPECT_NE(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(102);
    EXPECT_NE(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(201);
    EXPECT_NE(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(301);
    EXPECT_NE(hitOccurencies.end(), iit);

    iface->destroy(it, context);

    hdql_context_free(context, key.pl.datum);

    hdql_compound_destroy(trackCompound, context);
    hdql_compound_destroy(hitCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}} TEST(CppTemplatedInterfaces, MapCompoundAttributeAccessNoSelection)

TEST(CppTemplatedInterfaces, MapCompoundAttributeAccessWithSelection) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", context);
    compounds.emplace(typeid(hdql::test::Hit), hitCompound);
    // Add some attribute to hit compound to distinguish hits
    {  // Hit::energyDeposition
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( hitCompound
                              , "energyDeposition"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace< &hdql::test::Track::hits
                                  , hdql::test::SimpleRangeSelection  // < note sel type
                                  >::type_info(valTypes, compounds);
        assert(typeInfo == hitCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace< &hdql::test::Track::hits
                                  , hdql::test::SimpleRangeSelection  // < note sel type
                                  >::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "uint32_t");  // TODO: DetID_t
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( trackCompound
                              , "hits"
                              , ad);
    }


    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(trackCompound, "hits");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_FALSE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_collection(ad));

    ASSERT_FALSE(hdql_attr_def_is_atomic(ad));
    ASSERT_TRUE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    std::shared_ptr<hdql::test::Hit>
        hit1 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=1.23}),
        hit2 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=2.34}),
        hit3 = std::make_shared<hdql::test::Hit>(hdql::test::Hit{.energyDeposition=3.45});

    hdql::test::Track track;
    track.hits.emplace(101, hit1);  // not selected
    track.hits.emplace(102, hit1);  // selected
    track.hits.emplace(201, hit2);  // selected
    track.hits.emplace(301, hit3);  // not selected

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->create);
    ASSERT_TRUE(iface->dereference);
    ASSERT_TRUE(iface->advance);
    ASSERT_TRUE(iface->reset);
    ASSERT_TRUE(iface->destroy);
    ASSERT_TRUE(iface->compile_selection);
    ASSERT_TRUE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(valTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_CollectionKey key;
    key.isList = false;
    key.code = keyTypeCode;
    key.pl.datum = hdql_context_alloc(context, keyIFace->size);
    ASSERT_TRUE(key.pl.datum);
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(valTypes, "unsigned int");
        ASSERT_EQ(kCode, key.code);
    }

    // create selection
    hdql_SelectionArgs_t selArgs = iface->compile_selection("102:250", iface->definitionData, context);
    ASSERT_TRUE(selArgs);

    std::set<hdql::test::DetID_t> hitOccurencies;
    hdql_It_t it = iface->create( reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, selArgs, context );
    for( iface->reset(it, reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, selArgs, context)
       ;
       ; it = iface->advance(it) ) {
        // try to dereference iterator (may fail)
        hdql_Datum_t hit_ = iface->dereference(it, &key);
        if(!hit_) break;  // if dereference failed, sequence is depleted
        // add key to counts
        assert(key.pl.datum);
        hdql::test::DetID_t hitID = *reinterpret_cast<hdql::test::DetID_t*>(key.pl.datum);
        auto ir = hitOccurencies.insert(hitID);
        ASSERT_TRUE(ir.second);
    }
    EXPECT_EQ(hitOccurencies.size(), 2);

    // test hits were met as expected
    auto iit = hitOccurencies.find(101);
    EXPECT_EQ(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(102);
    EXPECT_NE(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(201);
    EXPECT_NE(hitOccurencies.end(), iit);
    iit = hitOccurencies.find(301);
    EXPECT_EQ(hitOccurencies.end(), iit) << "Hit #301 passed the selection, while it must not";

    iface->free_selection(iface->definitionData, selArgs, context);

    iface->destroy(it, context);

    hdql_context_free(context, key.pl.datum);

    hdql_compound_destroy(trackCompound, context);
    hdql_compound_destroy(hitCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}} TEST(CppTemplatedInterfaces, MapCompoundAttributeAccessWithSelection)

//
// Vector of compound values attribute
TEST(CppTemplatedInterfaces, VectorCompoundAttributeAccess) {  // {{{
    // Instantiate context
    hdql_Context_t context = hdql_context_create();
    hdql_ValueTypes * valTypes = hdql_context_get_types(context);
    assert(valTypes);
    hdql_value_types_table_add_std_types(valTypes);
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * trackCompound = hdql_compound_new("Track", context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);

    struct hdql_Compound * eventCompound = hdql_compound_new("Event", context);
    compounds.emplace(typeid(hdql::test::Event), eventCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace<&hdql::test::Event::tracks>::type_info(valTypes, compounds);
        assert(typeInfo == trackCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Event::tracks>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface
                , context );
        hdql_compound_add_attr( trackCompound
                              , "tracks"
                              , ad);
    }


    // Retrieve attribute definition from a compound by name
    const hdql_AttrDef * ad = hdql_compound_get_attr(trackCompound, "tracks");
    ASSERT_TRUE(ad);  // attribute resolved

    ASSERT_FALSE(hdql_attr_def_is_scalar(ad));
    ASSERT_TRUE(hdql_attr_def_is_collection(ad));

    ASSERT_FALSE(hdql_attr_def_is_atomic(ad));
    ASSERT_TRUE(hdql_attr_def_is_compound(ad));

    ASSERT_TRUE(hdql_attr_def_is_direct_query(ad));
    ASSERT_FALSE(hdql_attr_def_is_fwd_query(ad));

    ASSERT_FALSE(hdql_attr_def_is_static_value(ad));

    std::shared_ptr<hdql::test::Track>
        track1 = std::make_shared<hdql::test::Track>(hdql::test::Track{.ndf=1}),
        track2 = std::make_shared<hdql::test::Track>(hdql::test::Track{.ndf=2}),
        track3 = std::make_shared<hdql::test::Track>(hdql::test::Track{.ndf=3});

    hdql::test::Event event;
    event.tracks.push_back(track1);
    event.tracks.push_back(track2);
    event.tracks.push_back(track3);
    event.tracks.push_back(track1);

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->create);
    ASSERT_TRUE(iface->dereference);
    ASSERT_TRUE(iface->advance);
    ASSERT_TRUE(iface->reset);
    ASSERT_TRUE(iface->destroy);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(valTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_CollectionKey key;
    key.isList = false;
    key.code = keyTypeCode;
    key.pl.datum = hdql_context_alloc(context, keyIFace->size);
    ASSERT_TRUE(key.pl.datum);
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(valTypes, "size_t");
        ASSERT_EQ(kCode, key.code);
    }

    std::set<size_t> trackOccurences;
    hdql_It_t it = iface->create( reinterpret_cast<hdql_Datum_t>(&event), iface->definitionData, NULL, context );
    for( iface->reset(it, reinterpret_cast<hdql_Datum_t>(&event), iface->definitionData, NULL, context)
       ;
       ; it = iface->advance(it) ) {
        // try to dereference iterator (may fail)
        hdql_Datum_t track_ = iface->dereference(it, &key);
        if(!track_) break;  // if dereference failed, sequence is depleted
        // add key to counts
        assert(key.pl.datum);
        size_t trackNo = *reinterpret_cast<size_t*>(key.pl.datum);
        ASSERT_LT(trackNo, 4);
        if(trackNo == 0 || trackNo == 3) {
            EXPECT_EQ(reinterpret_cast<hdql::test::Track*>(track_), track1.get());  // no implicit copies
        } else if(trackNo == 1) {
            EXPECT_EQ(reinterpret_cast<hdql::test::Track*>(track_), track2.get());
        } else if(trackNo == 2) {
            EXPECT_EQ(reinterpret_cast<hdql::test::Track*>(track_), track3.get());
        }
        auto ir = trackOccurences.insert(trackNo);
        ASSERT_TRUE(ir.second);
    }
    EXPECT_EQ(trackOccurences.size(), 4);

    iface->destroy(it, context);
    hdql_context_free(context, key.pl.datum);

    hdql_compound_destroy(trackCompound, context);
    hdql_compound_destroy(eventCompound, context);

    // delete context
    hdql_context_destroy(context);
}  // }}} TEST(CppTemplatedInterfaces, VectorCompoundAttributeAccess)

#endif  // }}} defined(BUILD_GT_UTEST) && BUILD_GT_UTEST

namespace hdql {
namespace test {

helpers::Compounds
define_compound_types(hdql_Context_t context) {
    #if 1
    helpers::CompoundTypes types(context);
    types.new_compound<RawData>("RawData")
        .attr<&RawData::time>("time")
        .attr<&RawData::samples, SimpleRangeSelection>("samples")
    .end_compound()
    .new_compound<Hit>("Hit")
        .attr<&Hit::energyDeposition>("energyDeposition")
        .attr<&Hit::time>("time")
        .attr<&Hit::x>("x")
        .attr<&Hit::y>("y")
        .attr<&Hit::z>("z")
        .attr<&Hit::rawData>("rawData")
    .end_compound()
    .new_compound<Track>("track")
        .attr<&Track::chi2>("chi2")
        .attr<&Track::ndf>("ndf")
        .attr<&Track::pValue>("pValue")
        .attr<&Track::hits, SimpleRangeSelection>("hits")
    .end_compound()
    .new_compound<Event>("Event")
        .attr<&Event::eventID>("eventID")
        .attr<&Event::hits, SimpleRangeSelection>("hits")
        .attr<&Event::tracks, SimpleRangeSelection>("tracks")
    .end_compound();
    return types;
    #else
    hdql::helpers::Compounds compounds;
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", context);
    compounds.emplace(typeid(RawData), rawDataCompound);
    {  // RawData::time
        struct hdql_AtomicTypeFeatures typeInfo = helpers::IFace<&RawData::time>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface = helpers::IFace<&RawData::time>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( rawDataCompound
                              , "time"
                              , ad);
    }
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo = helpers::IFace<&RawData::samples>::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface = helpers::IFace<&RawData::samples>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(valTypes, "size_t");
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , context );
        hdql_compound_add_attr( rawDataCompound
                              , "samples"
                              , ad);
    }

    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", context);
    compounds.emplace(typeid(Hit), hitCompound);
    {  // Hit::RawData
        struct hdql_Compound * typeInfo = helpers::IFace<&Hit::rawData>::type_info(valTypes, compounds);
        struct hdql_ScalarAttrInterface iface = helpers::IFace<&Hit::rawData>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_scalar(
                  typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( hitCompound
                              , "rawData"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", context);
    compounds.emplace(typeid(Track), trackCompound);
    {  // Tracks::hits
        struct hdql_Compound * typeInfo = helpers::IFace<&Track::hits>::type_info(valTypes, compounds);
        struct hdql_CollectionAttrInterface iface = helpers::IFace<&Track::hits>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , context
                );
        hdql_compound_add_attr( trackCompound
                              , "hits"
                              , ad);
    }
    #endif
}

}  // namespace ::hdql::test
}  // namespace hdql

