
// Test very basic query iteration of attributes definitions of various kinds
// (atomic and compound, scalar and collection) created by C++ helpers.
// 
// Simple 1-level queries are tested.
//
// Attribute definitions used in these queries are defined in testing event
// struct compounds.

#include "basic/basic-query.hh"
#include "events-struct.hh"

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query-key.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <cstring>
#include <set>
#include <unordered_map>

#include "hdql/helpers/compounds.hh"
#include "hdql/operations.h"
#include "basic-context.hh"

namespace hdql {
namespace test {

class CppTemplatedInterfaces : public TestingContext {
public:
    void SetUp() override {
        TestingContext::SetUp();
        hdql_value_types_table_add_std_types(_valueTypes);
    }
};

//                                          __________________________________
// _______________________________________/ Tests for C++ templated interfaces

//
// Scalar attribute
TEST_F(CppTemplatedInterfaces, ScalarAttributeAccess) {  // {{{
    
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", _context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::time
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::RawData::time>::type_info(_valueTypes, compounds);
        struct hdql_ScalarAttrInterface iface = hdql::helpers::IFace<&hdql::test::RawData::time>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0  // key type code
                , NULL  // key iface
                , _context
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    struct hdql::test::RawData rawDataInstance = { .time = 1.23
        , .samples = {1, 2, 3, 4} };

    const hdql_ScalarAttrInterface * iface = hdql_attr_def_scalar_iface(ad);
    // we expect scalar attribute access interface to have only
    // one method set
    ASSERT_FALSE(iface->definitionData);
    ASSERT_FALSE(iface->new_dyn_data);
    ASSERT_TRUE(iface->reset);
    ASSERT_FALSE(iface->destroy_dyn_data);

    // dereference without a key
    hdql_Datum_t result = iface->reset( reinterpret_cast<hdql_Datum_t>(&rawDataInstance)
            , NULL  // no dynamic data for simple scalar attribute
            , iface->definitionData
            , NULL  // key to (reserved) pointer, optional
            , _context
            );

    ASSERT_TRUE(result);
    ASSERT_EQ( *reinterpret_cast<decltype(hdql::test::RawData::time) *>(result)
             , rawDataInstance.time );

    // dereference 2nd time with a key (won't be set)
    hdql_Key_t key = hdql_key_new(_context);
    result = iface->reset( reinterpret_cast<hdql_Datum_t>(&rawDataInstance)
            , NULL  // no dynamic data for simple scalar attribute
            , iface->definitionData
            , NULL  // key to (reserved) pointer, optional
            , _context
            );
    EXPECT_TRUE(hdql_key_is_empty(key));
    ASSERT_TRUE(result);
    ASSERT_EQ( *reinterpret_cast<decltype(hdql::test::RawData::time) *>(result)
             , rawDataInstance.time );

    hdql_compound_destroy(rawDataCompound, _context);

    hdql_key_destroy(key, _context);
}  // }}}

//
// Array of atomic values attribute, no selection
TEST_F(CppTemplatedInterfaces, AtomicArrayAttributeAccessNoSelection) {  // {{{
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", _context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::RawData::samples>::type_info(_valueTypes, compounds);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::RawData::samples>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , _context );
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    struct hdql::test::RawData rawDataInstance[2] = {
          { .time = 1.23, .samples = {1, 2, 3, 4} }
        , { .time = 3.21, .samples = {11, 12, 13, 14} }
        };

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->new_iterator);
    ASSERT_TRUE(iface->yield);
    ASSERT_TRUE(iface->reset_iterator);
    ASSERT_TRUE(iface->destroy_iterator);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(_valueTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_Key_t key = hdql_key_new(_context);
    hdql_key_set_datum(key, keyTypeCode, hdql_context_alloc(_context, keyIFace->size));
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_EQ(kCode, hdql_key_datum_get_type_code(key));
    }

    hdql_It_t it;
    const size_t nSamples = sizeof(hdql::test::RawData::samples)/sizeof(*hdql::test::RawData::samples);
    for(int i = 0; i < 4; ++i) {
        struct hdql::test::RawData & root = rawDataInstance[i%2];
        if(0 == i) {
            it = iface->new_iterator( reinterpret_cast<hdql_Datum_t>( &root )
                              , iface->definitionData
                              , _context );
        }
        size_t j = 0;
        hdql_Datum_t value = iface->reset_iterator( it
                , reinterpret_cast<hdql_Datum_t>( &root )
                , iface->definitionData
                , NULL
                , (i >> 1) ? key : NULL
                , _context );
        ASSERT_TRUE(value);
        for(; j < nSamples; ++j) {
            EXPECT_EQ(*reinterpret_cast<std::remove_reference<decltype(*hdql::test::RawData::samples)>::type *>(value)
                    , root.samples[j] );
            if(i >> 1) {
                EXPECT_EQ(*reinterpret_cast<size_t*>(hdql_key_datum_get(key)), ((size_t) j));
            }
            value = iface->yield(it, iface->definitionData
                    , (i >> 1) ? key : NULL
                    , _context
                    );
        }
    }
    iface->destroy_iterator(it, iface->definitionData, _context);
    hdql_key_destroy(key, _context);

    hdql_compound_destroy(rawDataCompound, _context);
}  // }}} TEST_F(CppTemplatedInterfaces, AtomicArrayAttributeAccessNoSelection)

//
// Array of atomic values attribute, no selection
TEST_F(CppTemplatedInterfaces, AtomicArrayAttributeAccessWithSelection) {  // {{{
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compound
    struct hdql_Compound * rawDataCompound = hdql_compound_new("RawData", _context);
    compounds.emplace(typeid(hdql::test::RawData), rawDataCompound);
    // Add attribute to a compound
    {  // RawData::samples[4]
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace< &hdql::test::RawData::samples
                                  , hdql::test::SimpleRangeSelection
                                  >::type_info(_valueTypes, compounds);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace< &hdql::test::RawData::samples
                                  , hdql::test::SimpleRangeSelection
                                  >::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_collection(
                  &typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , _context );
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

    struct hdql::test::RawData rawDataInstance[2] = {
          { .time = 1.23, .samples = {1, 2, 3, 4} }
        , { .time = 3.21, .samples = {11, 12, 13, 14} }
        };

    const hdql_CollectionAttrInterface * iface = hdql_attr_def_collection_iface(ad);

    ASSERT_FALSE(iface->definitionData);
    ASSERT_TRUE(iface->new_iterator);
    ASSERT_TRUE(iface->yield);
    ASSERT_TRUE(iface->reset_iterator);
    ASSERT_TRUE(iface->destroy_iterator);
    ASSERT_TRUE(iface->compile_selection);
    ASSERT_TRUE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(_valueTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_Key * key = hdql_key_new(_context);
    hdql_key_set_datum(key, keyTypeCode, hdql_context_alloc(_context, keyIFace->size));
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_EQ(kCode, hdql_key_datum_get_type_code(key));
    }

    hdql_SelectionArgs_t selArgs = iface->compile_selection("2:4", iface->definitionData, _context);
    ASSERT_TRUE(selArgs);

    hdql_It_t it;
    for(int i = 0; i < 4; ++i) {
        struct hdql::test::RawData & root = rawDataInstance[i%2];
        if(0 == i) {
            it = iface->new_iterator( reinterpret_cast<hdql_Datum_t>( &root )
                              , iface->definitionData
                              , _context );
        }
        ASSERT_TRUE(it);
        size_t j = 2;
        hdql_Datum_t value = iface->reset_iterator( it
                , reinterpret_cast<hdql_Datum_t>( &root )
                , iface->definitionData
                , selArgs
                , (i >> 1) ? key : NULL
                , _context );
        ASSERT_TRUE(it);
        for(; j < 4; ++j) {
            EXPECT_EQ(*reinterpret_cast<std::remove_reference<decltype(*hdql::test::RawData::samples)>::type *>(value)
                    , root.samples[j] );
            if(i >> 1) {
                EXPECT_EQ(*reinterpret_cast<size_t*>(hdql_key_datum_get(key)), ((size_t) j));
            }
            value = iface->yield(it, iface->definitionData
                    , (i >> 1) ? key : NULL
                    , _context
                    );
        }
    }
    iface->destroy_iterator(it, iface->definitionData, _context);
    hdql_key_destroy(key, _context);

    iface->free_selection(iface->definitionData, selArgs, _context);
    hdql_compound_destroy(rawDataCompound, _context);
}  // }}} TEST_F(CppTemplatedInterfaces, AtomicArrayAttributeAccessWithSelection)


//
// Map of compound values attribute
TEST_F(CppTemplatedInterfaces, MapCompoundAttributeAccessNoSelection) {  // {{{
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", _context);
    compounds.emplace(typeid(hdql::test::Hit), hitCompound);
    // Add some attribute to hit compound to distinguish hits
    {  // Hit::energyDeposition
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::type_info(_valueTypes, compounds);
        struct hdql_ScalarAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0
                , NULL  // key type iface 
                , _context );
        hdql_compound_add_attr( hitCompound
                              , "energyDeposition"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", _context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace<&hdql::test::Track::hits>::type_info(_valueTypes, compounds);
        assert(typeInfo == hitCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Track::hits>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(_valueTypes, "uint32_t");  // TODO: DetID_t
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , _context );
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

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
    ASSERT_TRUE(iface->new_iterator);
    ASSERT_TRUE(iface->yield);
    ASSERT_TRUE(iface->reset_iterator);
    ASSERT_TRUE(iface->destroy_iterator);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(_valueTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_Key * key = hdql_key_new(_context);
    hdql_key_set_datum(key, keyTypeCode, hdql_context_alloc(_context, keyIFace->size));
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(_valueTypes, "unsigned int");
        ASSERT_EQ(kCode, hdql_key_datum_get_type_code(key));
    }

    std::set<hdql::test::DetID_t> hitOccurencies;
    hdql_It_t it = iface->new_iterator( reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, _context );
    for( hdql_Datum_t hit_ = iface->reset_iterator(it, reinterpret_cast<hdql_Datum_t>(&track)
                    , iface->definitionData, NULL, key, _context)
       ; hit_
       ; hit_ = iface->yield(it, iface->definitionData, key, _context) ) {
        // add key to counts
        hdql::test::DetID_t hitID = *reinterpret_cast<hdql::test::DetID_t*>(hdql_key_datum_get(key));
        if(hitID == 101 || hitID == 102) {
            EXPECT_EQ(reinterpret_cast<hdql::test::Hit*>(hit_), hit1.get());  // no implicit copies
        }
        auto ir = hitOccurencies.insert(hitID);
        EXPECT_TRUE(ir.second) << " repeated id=" << hitID;
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

    iface->destroy_iterator(it, iface->definitionData, _context);

    hdql_key_destroy(key, _context);

    hdql_compound_destroy(trackCompound, _context);
    hdql_compound_destroy(hitCompound, _context);
}  // }}} TEST_F(CppTemplatedInterfaces, MapCompoundAttributeAccessNoSelection)

TEST_F(CppTemplatedInterfaces, MapCompoundAttributeAccessWithSelection) {  // {{{
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * hitCompound = hdql_compound_new("Hit", _context);
    compounds.emplace(typeid(hdql::test::Hit), hitCompound);
    // Add some attribute to hit compound to distinguish hits
    {  // Hit::energyDeposition
        struct hdql_AtomicTypeFeatures typeInfo
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::type_info(_valueTypes, compounds);
        struct hdql_ScalarAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Hit::energyDeposition>::iface();
        struct hdql_AttrDef * ad = hdql_attr_def_create_atomic_scalar(
                  &typeInfo
                , &iface
                , 0x0
                , NULL  // key type iface 
                , _context );
        hdql_compound_add_attr( hitCompound
                              , "energyDeposition"
                              , ad);
    }

    struct hdql_Compound * trackCompound = hdql_compound_new("Track", _context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace< &hdql::test::Track::hits
                                  , hdql::test::SimpleRangeSelection  // < note sel type
                                  >::type_info(_valueTypes, compounds);
        assert(typeInfo == hitCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace< &hdql::test::Track::hits
                                  , hdql::test::SimpleRangeSelection  // < note sel type
                                  >::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(_valueTypes, "uint32_t");  // TODO: DetID_t
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface 
                , _context );
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

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
    ASSERT_TRUE(iface->new_iterator);
    ASSERT_TRUE(iface->yield);
    ASSERT_TRUE(iface->reset_iterator);
    ASSERT_TRUE(iface->destroy_iterator);
    ASSERT_TRUE(iface->compile_selection);
    ASSERT_TRUE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(_valueTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_Key * key = hdql_key_new(_context);
    hdql_key_set_datum(key, keyTypeCode, hdql_context_alloc(_context, keyIFace->size));
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(_valueTypes, "unsigned int");
        ASSERT_EQ(kCode, hdql_key_datum_get_type_code(key));
    }

    // create selection
    hdql_SelectionArgs_t selArgs = iface->compile_selection("102:250", iface->definitionData, _context);
    ASSERT_TRUE(selArgs);

    std::set<hdql::test::DetID_t> hitOccurencies;
    hdql_It_t it = iface->new_iterator( reinterpret_cast<hdql_Datum_t>(&track), iface->definitionData, _context );
    for( hdql_Datum_t hit_ = iface->reset_iterator(it, reinterpret_cast<hdql_Datum_t>(&track)
                , iface->definitionData, selArgs, key, _context)
       ; hit_
       ; hit_ = iface->yield(it, iface->definitionData, key, _context) ) {
        // try to dereference iterator (may fail)
        if(!hit_) break;  // if dereference failed, sequence is depleted
        // add key to counts
        hdql::test::DetID_t hitID = *reinterpret_cast<hdql::test::DetID_t*>(hdql_key_datum_get(key));
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

    iface->free_selection(iface->definitionData, selArgs, _context);

    iface->destroy_iterator(it, iface->definitionData, _context);

    hdql_key_destroy(key, _context);

    hdql_compound_destroy(trackCompound, _context);
    hdql_compound_destroy(hitCompound, _context);
}  // }}} TEST_F(CppTemplatedInterfaces, MapCompoundAttributeAccessWithSelection)

//
// Vector of compound values attribute
TEST_F(CppTemplatedInterfaces, VectorCompoundAttributeAccess) {  // {{{
    // Create compounds index helper
    hdql::helpers::Compounds compounds;
    // Create new compounds
    struct hdql_Compound * trackCompound = hdql_compound_new("Track", _context);
    compounds.emplace(typeid(hdql::test::Track), trackCompound);

    struct hdql_Compound * eventCompound = hdql_compound_new("Event", _context);
    compounds.emplace(typeid(hdql::test::Event), eventCompound);
    {  // Track::hits
        struct hdql_Compound * typeInfo
            = hdql::helpers::IFace<&hdql::test::Event::tracks>::type_info(_valueTypes, compounds);
        assert(typeInfo == trackCompound);
        struct hdql_CollectionAttrInterface iface
            = hdql::helpers::IFace<&hdql::test::Event::tracks>::iface();
        hdql_ValueTypeCode_t keyTypeCode
            = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_NE(0x0, keyTypeCode);
        struct hdql_AttrDef * ad = hdql_attr_def_create_compound_collection(
                  typeInfo
                , &iface
                , keyTypeCode
                , NULL  // key type iface
                , _context );
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

    ASSERT_FALSE(hdql_attr_def_is_static_const_value(ad));
    ASSERT_FALSE(hdql_attr_def_is_static_external_value(ad));

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
    ASSERT_TRUE(iface->new_iterator);
    ASSERT_TRUE(iface->yield);
    ASSERT_TRUE(iface->reset_iterator);
    ASSERT_TRUE(iface->destroy_iterator);
    EXPECT_FALSE(iface->compile_selection);
    EXPECT_FALSE(iface->free_selection);

    // Reserve key
    hdql_ValueTypeCode_t keyTypeCode = hdql_attr_def_get_key_type_code(ad);
    ASSERT_NE(0x0, keyTypeCode);
    const hdql_ValueInterface * keyIFace = hdql_types_get_type(_valueTypes, keyTypeCode);
    ASSERT_TRUE(keyIFace);
    ASSERT_GT(keyIFace->size, 0);

    hdql_Key * key = hdql_key_new(_context);
    hdql_key_set_datum(key, keyTypeCode, hdql_context_alloc(_context, keyIFace->size));
    {  // assure key is of size_t
        hdql_ValueTypeCode_t kCode = hdql_types_get_type_code(_valueTypes, "size_t");
        ASSERT_EQ(kCode, hdql_key_datum_get_type_code(key));
    }

    std::set<size_t> trackOccurences;
    hdql_It_t it = iface->new_iterator( reinterpret_cast<hdql_Datum_t>(&event), iface->definitionData, _context );
    for( hdql_Datum_t track_ = iface->reset_iterator(it, reinterpret_cast<hdql_Datum_t>(&event)
                , iface->definitionData, NULL, key, _context)
       ; track_
       ; track_ = iface->yield(it, iface->definitionData, key, _context) ) {
        if(!track_) break;  // if dereference failed, sequence is depleted
        // add key to counts
        size_t trackNo = *reinterpret_cast<size_t*>(hdql_key_datum_get(key));
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

    iface->destroy_iterator(it, iface->definitionData, _context);
    hdql_key_destroy(key, _context);

    hdql_compound_destroy(trackCompound, _context);
    hdql_compound_destroy(eventCompound, _context);
}  // }}} TEST_F(CppTemplatedInterfaces, VectorCompoundAttributeAccess)

}  // namespace ::hdql::test
}  // namespace test

