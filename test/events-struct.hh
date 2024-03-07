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

//TEST_F(HDQLTestingEvent, )

}  // namespace ::hdql::test
}  // namespace hdql

