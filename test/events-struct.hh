#pragma once

#include "hdql/compound.h"
#include "hdql/helpers/compounds.hh"

#include <type_traits>
#include <unordered_map>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace hdql {
namespace test {

//
// Testing fixture defining "testing" events structure

typedef unsigned int DetID_t;

struct RawData {
    double time;
    short samples[4];
};

struct Hit {
    float energyDeposition;
    float time;
    float x, y, z;
    std::shared_ptr<RawData> rawData;
};

struct Track {
    float chi2;
    int ndf;
    float pValue;
    std::unordered_map<DetID_t, std::shared_ptr<Hit>> hits;
};

struct Event {
    int eventID;
    std::unordered_map<DetID_t, std::shared_ptr<Hit>> hits;
    std::vector<std::shared_ptr<Track>> tracks;
};

// Register testing types
helpers::CompoundTypes define_compound_types(hdql_Context_t);

//
// G-Test testing fixture
class TestingEventStruct : public ::testing::Test {
protected:
    hdql_Context_t _ctx;
    hdql_ValueTypes * _valueTypes;
    hdql_Operations * _operations;
    hdql::helpers::CompoundTypes _compounds;
    hdql_Compound * _eventCompound;
protected:
    TestingEventStruct()
            : _valueTypes(nullptr)
            , _operations(nullptr)
            , _compounds(nullptr)
            , _eventCompound(nullptr)
            {}
    TestingEventStruct(const TestingEventStruct & orig)
            : _valueTypes(orig._valueTypes)
            , _operations(orig._operations)
            , _compounds(orig._compounds)
            , _eventCompound(orig._eventCompound)
            {}
    void SetUp() override;
    void TearDown() override;
};  // class TestingEventStruct

}  // namespace ::hdql::test
}  // namespace hdql

namespace hdql {
namespace test {
/// Some primitive selection type for tests
typedef std::pair<size_t, size_t> SimpleRangeSelection;

SimpleRangeSelection compile_simple_selection(const char * expression_);

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
        return new (buf) test::SimpleRangeSelection(test::compile_simple_selection(strexpr));
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
        return new (buf) test::SimpleRangeSelection(test::compile_simple_selection(strexpr));
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
        return new (buf) test::SimpleRangeSelection(test::compile_simple_selection(strexpr));
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
