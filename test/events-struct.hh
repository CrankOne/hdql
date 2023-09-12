#pragma once

#include "hdql/context.h"
#include "hdql/operations.h"
#include "hdql/types.h"
#include "hdql/value.h"
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
    float time;
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

//
// Register types
int
fill_tables( hdql_Compound * eventsCompound
           , hdql_Context_t
           );

//
// G-Test testing fixture
class TestingEventStruct : public ::testing::Test {
protected:
    hdql_ValueTypes * _valueTypes;
    hdql_Operations * _operations;

    hdql_Compound * _eventCompound
                , * _trackCompound
                , * _hitCompound
                ;

    hdql_Context_t _context;
protected:
    void SetUp() override;
    void TearDown() override;
};  // class TestingEventStruct

//TEST_F(HDQLTestingEvent, )

}  // namespace ::hdql::test
}  // namespace hdql

