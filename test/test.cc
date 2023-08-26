#include "hdql/compound.h"
#include "hdql/query.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace test {

typedef unsigned int DetID_t;

struct Hit {
    float energyDeposition;
    float time;
    float x, y, z;
};

static void * _Hit_get_energyDeposition(void * hit_) {
    return &reinterpret_cast<Hit*>(hit_)->energyDeposition;
}

static void * _Hit_get_time(void * hit_) {
    return &reinterpret_cast<Hit*>(hit_)->time;
}

static void * _Hit_get_x(void * hit_) {
    return &reinterpret_cast<Hit*>(hit_)->x;
}

static void * _Hit_get_y(void * hit_) {
    return &reinterpret_cast<Hit*>(hit_)->y;
}

static void * _Hit_get_z(void * hit_) {
    return &reinterpret_cast<Hit*>(hit_)->z;
}

static void
fill_hit_table( na64dp_math_dsl_Table * hitTable) {
    hdql_compound_add_attr(
              hitTable
            , "energyDeposition"
            , 1
            , _Hit_get_energyDeposition
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar(
              hitTable
            , "time"
            , _Hit_get_time
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar(
              hitTable
            , "x"
            , _Hit_get_x
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar(
              hitTable
            , "y"
            , _Hit_get_y
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar(
              hitTable
            , "z"
            , _Hit_get_z
            , NULL
            , 0x1  // TODO
            );
}


struct Track {
    float chi2;
    int ndf;
    float pValue;
    std::unordered_map<DetID_t, std::shared_ptr<Hit>> hits;
};

static void * _Track_get_chi2(void * hit_) {
    return &reinterpret_cast<Track*>(hit_)->chi2;
}

static void * _Track_get_ndf(void * hit_) {
    return &reinterpret_cast<Track*>(hit_)->ndf;
}

static void * _Track_get_pValue(void * hit_) {
    return &reinterpret_cast<Track*>(hit_)->pValue;
}

static void
fill_track_table( na64dp_math_dsl_Table * trackTable
                , na64dp_math_dsl_Table * hitTable
                ) {
    na64dp_math_dsl_type_attr_add_scalar( trackTable
            , "chi2"
            , _Track_get_chi2
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar( trackTable
            , "ndf"
            , _Track_get_ndf
            , NULL
            , 0x2  // TODO
            );
    na64dp_math_dsl_type_attr_add_scalar( trackTable
            , "pValue"
            , _Track_get_pValue
            , NULL
            , 0x1  // TODO
            );
    na64dp_math_dsl_type_attr_add_collection( trackTable
            , "hits"
            , na64dp::math_dsl::IterableStateWrapper<&Track::hits>::_get_state
            , na64dp::math_dsl::IterableStateWrapper<&Track::hits>::_reset_state
            , na64dp::math_dsl::IterableStateWrapper<&Track::hits>::_free_state
            , hitTable
            , 0x0  // TODO
            );
}

struct Event {
    int eventID;
    std::unordered_map<DetID_t, std::shared_ptr<Hit>> hits;
    std::vector<std::shared_ptr<Track>> tracks;
};

static void * _Event_get_eventID(void * hit_) {
    return &reinterpret_cast<Track*>(hit_)->chi2;
}

static void
fill_event_table( na64dp_math_dsl_Table * eventTable
                , na64dp_math_dsl_Table * trackTable
                , na64dp_math_dsl_Table * hitTable
                ) {
    na64dp_math_dsl_type_attr_add_scalar( eventTable
            , "eventID"
            , _Event_get_eventID
            , NULL
            , 0x2  // TODO
            );
    na64dp_math_dsl_type_attr_add_collection( eventTable
            , "tracks"
            , na64dp::math_dsl::IterableStateWrapper<&Event::tracks>::_get_state
            , na64dp::math_dsl::IterableStateWrapper<&Event::tracks>::_reset_state
            , na64dp::math_dsl::IterableStateWrapper<&Event::tracks>::_free_state
            , hitTable
            , 0x0  // TODO
            );
}


//    na64dp_math_dsl_Table * hitTable    = na64dp_math_dsl_type_new()
//                        , * trackTable  = na64dp_math_dsl_type_new()
//                        , * eventTable  = na64dp_math_dsl_type_new()
//                        ;

}  // namespace test

