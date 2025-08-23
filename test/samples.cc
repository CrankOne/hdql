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
    ev.hits.clear();
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

    ev.tracks.clear();
    ev.tracks.push_back(t1);
    ev.tracks.push_back(t2);
    ev.tracks.push_back(t3);

    ev.eventID = 104501;
}

void
fill_data_sample_2( Event & ev ) {
    std::shared_ptr<RawData> rawData1 = std::make_shared<RawData>(RawData{0.10, {  5,  6,  7,  8}})
                           , rawData2 = std::make_shared<RawData>(RawData{0.11, { 15, 16, 17, 18}})
                           , rawData3 = std::make_shared<RawData>(RawData{0.12, { 25, 26, 27, 28}})
                           /* no raw data 4 */
                           , rawData5 = std::make_shared<RawData>(RawData{0.15, { 45, 46, 47, 48}})
                           ;

    std::shared_ptr<Hit> h10 = std::make_shared<Hit>(Hit{10, 20, 1.1, 2.2, 3.3, rawData1})
                       , h11 = std::make_shared<Hit>(Hit{11, 21, 4.4, 5.5, 6.6, rawData2})
                       , h12 = std::make_shared<Hit>(Hit{12, 22, 7.7, 8.8, 9.9, /* no raw data */})
                       , h13 = std::make_shared<Hit>(Hit{13, 23, 0.1, 0.2, 0.3, rawData3})
                       , h14 = std::make_shared<Hit>(Hit{14, 24, 3.1, 4.1, 5.9, rawData5})
                       ;

    ev.hits.emplace(401, h10);
    ev.hits.emplace(402, h11);
    ev.hits.emplace(450, h12);
    ev.hits.emplace(501, h13);
    ev.hits.emplace(502, h14);

    std::shared_ptr<Track> t1 = std::make_shared<Track>(Track{ 2.5, 4, 0.02 })
                         , t2 = std::make_shared<Track>(Track{ 0.2, 2, 0.50 })
                         , t3 = std::make_shared<Track>(Track{12.0, 1, 0.001 })
                         ;

    ev.hits.clear();
    // t1 uses two early hits + one later hit
    t1->hits.emplace(401, h10);
    t1->hits.emplace(402, h11);
    t1->hits.emplace(502, h14);

    // t2: single hit with no raw data attached
    t2->hits.emplace(450, h12);

    // t3: two hits from the "500s" block
    t3->hits.emplace(501, h13);
    // note: t3 intentionally does not include h14 to keep sets disjoint

    ev.tracks.clear();
    ev.tracks.push_back(t1);
    ev.tracks.push_back(t2);
    ev.tracks.push_back(t3);

    ev.eventID = 104502;
}

void
fill_data_sample_3( Event & ev ) {
    std::shared_ptr<RawData> rawData1 = std::make_shared<RawData>(RawData{0.21, {  2,  4,  6,  8}})
                           /* no raw data 2 */
                           , rawData3 = std::make_shared<RawData>(RawData{0.23, {  4,  8, 12, 16}})
                           , rawData4 = std::make_shared<RawData>(RawData{0.24, {  6, 12, 18, 24}})
                           , rawData5 = std::make_shared<RawData>(RawData{0.25, { 10, 20, 30, 40}})
                           ;

    std::shared_ptr<Hit> h20 = std::make_shared<Hit>(Hit{ 6,  7, -1.0,  0.0,  1.0, rawData1})
                       , h21 = std::make_shared<Hit>(Hit{ 7,  8, -2.0, -1.0,  0.5, rawData3})
                       , h22 = std::make_shared<Hit>(Hit{ 8,  9,  0.3,  0.4,  0.5, rawData4})
                       , h23 = std::make_shared<Hit>(Hit{ 9, 10,  9.9,  8.8,  7.7, /* no raw data */})
                       , h24 = std::make_shared<Hit>(Hit{10, 11,  5.5,  6.6,  7.7, rawData5})
                       ;

    ev.hits.emplace( 10, h20);
    ev.hits.emplace( 11, h21);
    ev.hits.emplace( 12, h22);
    ev.hits.emplace( 20, h23);
    ev.hits.emplace( 99, h24);

    std::shared_ptr<Track> tA = std::make_shared<Track>(Track{ 5.5, 2, 0.10 })
                         , tB = std::make_shared<Track>(Track{ 0.05, 3, 0.90 })
                         , tC = std::make_shared<Track>(Track{100.0, 5, 0.0001 })
                         , tD = std::make_shared<Track>(Track{ 1.0, 1, 0.20 })
                         ;

    ev.hits.clear();
    // tA: three consecutive hits
    tA->hits.emplace(10, h20);
    tA->hits.emplace(11, h21);
    tA->hits.emplace(12, h22);

    // tB: only the "no raw data" hit
    tB->hits.emplace(20, h23);

    // tC: intentionally no hits

    // tD: the last hit only
    tD->hits.emplace(99, h24);

    ev.tracks.clear();
    ev.tracks.push_back(tA);
    ev.tracks.push_back(tB);
    ev.tracks.push_back(tC);
    ev.tracks.push_back(tD);

    ev.eventID = 104503;
}


}  // namespace ::hdql::test
}  // namespace hdql

