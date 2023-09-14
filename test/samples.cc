#include "samples.hh"
#include "events-struct.hh"
#include <memory>
#include <stdexcept>

namespace hdql {
namespace test {

void
fill_data_sample_1( Event & ev ) {
    std::shared_ptr<RawData> rawData1 = std::make_shared<RawData>(RawData{0.01, { 1,  2,  3,  4}})
                           , rawData2 = std::make_shared<RawData>(RawData{0.02, {11, 12, 13, 14}})
                           , rawData3 = std::make_shared<RawData>(RawData{0.03, {21, 22, 23, 24}})
                           /* no raw data 5 */
                           , rawData5 = std::make_shared<RawData>(RawData{0.05, {41, 42, 43, 44}})
                           ;

    std::shared_ptr<Hit> h1 = std::make_shared<Hit>(Hit{ 1, 2, 3.4, 5.6, 7.8, rawData1})
                       , h2 = std::make_shared<Hit>(Hit{ 2, 3, 4.5, 6.7, 8.9, rawData2})
                       , h3 = std::make_shared<Hit>(Hit{ 3, 4, 5.6, 7.8, 9.0, rawData3})
                       , h4 = std::make_shared<Hit>(Hit{ 4, 5, 6.7, 8.9, 0.5, /*no raw data*/})
                       , h5 = std::make_shared<Hit>(Hit{ 5, 6, 7.8, 9.0, 1.2, rawData5})
                       ;
    ev.hits.emplace(101, h1);
    ev.hits.emplace(102, h2);
    ev.hits.emplace(103, h3);
    ev.hits.emplace(202, h4);
    ev.hits.emplace(301, h5);

    std::shared_ptr<Track> t1 = std::make_shared<Track>(Track{ 0.1, 2, 0.25 })
                         , t2 = std::make_shared<Track>(Track{  10, 3, 0.01 })
                         , t3 = std::make_shared<Track>(Track{  7,  2, 0.04 })
                         //, t4 = std::make_shared<Track>(Track{ 1.2, 3, 0.18 })
                         ;

    t1->hits.emplace(101, h1);
    t1->hits.emplace(102, h2);

    // t2 has no hits

    t3->hits.emplace(103, h3);
    t3->hits.emplace(202, h4);
    t3->hits.emplace(301, h5);

    ev.tracks.push_back(t1);
    ev.tracks.push_back(t2);
    ev.tracks.push_back(t3);

    ev.eventID = 104501;
}

}
}

