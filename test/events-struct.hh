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
helpers::Compounds define_compound_types(hdql_Context_t);

//
// G-Test testing fixture
class TestingEventStruct : public ::testing::Test {
protected:
    hdql_ValueTypes * _valueTypes;
    hdql_Operations * _operations;
    hdql::helpers::Compounds _compounds;
    hdql_Context_t _context;
    hdql_Compound * _eventCompound;
protected:
    void SetUp() override;
    void TearDown() override;
};  // class TestingEventStruct

//TEST_F(HDQLTestingEvent, )

}  // namespace ::hdql::test
}  // namespace hdql

