#include "events-struct.hh"

#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/operations.h"
#include "hdql/types.h"
#include "hdql/value.h"
#include "hdql/query.h"

#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

namespace hdql {
namespace test {

struct SimpleSelect {
    ssize_t nMin, nMax;
};

static hdql_SelectionArgs_t
_compile_simple_selection( const char * expr_
        , const hdql_AttributeDefinition * targetAttrDef
        , hdql_Context_t ctx
        , char * errBuf, size_t errBufLen
        ) {
    std::string expr(expr_);
    size_t n = expr.find('-');
    if(n == std::string::npos) {
        snprintf(errBuf, errBufLen, "Can't parse det.delstion expression \"%s\""
                  " -- no delimiter `-' found"
                , expr_ );
        return NULL;
    }
    SimpleSelect * ds = new SimpleSelect;
    std::string from = expr.substr(0, n)
              , to = expr.substr(n+1)
              ;
    if(from == "...") {
        ds->nMin = std::numeric_limits<decltype(SimpleSelect::nMin)>::min();
    } else {
        ds->nMin = std::stoi(from);
    }
    if(to == "...") {
        ds->nMax = std::numeric_limits<decltype(SimpleSelect::nMax)>::max();
    } else {
        ds->nMax = std::stoi(to);
    }
    return reinterpret_cast<hdql_SelectionArgs_t>(ds);
}

static void
_free_simple_selection( hdql_Context_t ctx
                          , hdql_SelectionArgs_t args
                          ) {
    delete reinterpret_cast<SimpleSelect *>(args);
}


static hdql_Datum_t _hit_get_edep( hdql_Datum_t hit_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Hit*>(hit_)->energyDeposition);
}

static hdql_Datum_t _hit_get_time( hdql_Datum_t hit_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Hit*>(hit_)->time);
}

static hdql_Datum_t _hit_get_x( hdql_Datum_t hit_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Hit*>(hit_)->x);
}

static hdql_Datum_t _hit_get_y( hdql_Datum_t hit_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Hit*>(hit_)->y);
}

static hdql_Datum_t _hit_get_z( hdql_Datum_t hit_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Hit*>(hit_)->z);
}

// track

static hdql_Datum_t _track_get_chi2( hdql_Datum_t track_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Track*>(track_)->chi2);
}

static hdql_Datum_t _track_get_ndf( hdql_Datum_t track_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Track*>(track_)->ndf);
}

static hdql_Datum_t _track_get_pValue( hdql_Datum_t track_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Track*>(track_)->pValue);
}

// track::hits collection

static hdql_It_t
_track_hits_it_create( hdql_Datum_t track_
                     , hdql_SelectionArgs_t selArgs
                     , hdql_Context_t ctx_
                     ) {
    auto itPtr = new decltype(Track::hits)::iterator;
    return reinterpret_cast<hdql_It_t>(itPtr);
}

static hdql_Datum_t
_track_hits_it_dereference( hdql_Datum_t track_
                          , hdql_It_t it_
                          , hdql_Context_t ctx_
                          ) {
    auto & it    = *reinterpret_cast<decltype(Track::hits)::iterator*>(it_);
    auto & track = *reinterpret_cast<Track*>(track_);
    if(track.hits.end() == it) return NULL;
    return reinterpret_cast<hdql_Datum_t>(it->second.get());
}

hdql_It_t
_track_hits_it_advance( hdql_Datum_t track_
                      , hdql_SelectionArgs_t selection_
                      , hdql_It_t it_
                      , hdql_Context_t ctx_
                      ) {
    assert(it_);
    assert(track_);
    auto & track = *reinterpret_cast<Track*>(track_);
    auto & it =    *reinterpret_cast<decltype(Track::hits)::iterator*>(it_);
    SimpleSelect * selection = selection_
                 ? reinterpret_cast<SimpleSelect*>(selection_)
                 : NULL
                 ;
    assert(track.hits.end() != it);
    //if(selection && track.hits.end() != it)
    //    std::cout << " xxx " << selection->nMin << " ? "
    //        << it->first << " ? " << selection->nMax
    //        << std::endl;
    do {
        ++it;
    } while( selection
         && ( it != track.hits.end()
          && (it->first < selection->nMin || it->first > selection->nMax)));
    //if(selection && track.hits.end() != it)
    //    std::cout << " xxx " << selection->nMin << " <= "
    //        << it->first << " <= " << selection->nMax
    //        << std::endl;
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_track_hits_it_reset( hdql_Datum_t track_
                    , hdql_SelectionArgs_t selection_
                    , hdql_It_t it_
                    , hdql_Context_t ctx_
                    ) {
    assert(track_);
    assert(it_);
    auto & it = *reinterpret_cast<decltype(Track::hits)::iterator*>(it_);
    auto & track = *reinterpret_cast<Track*>(track_);
    SimpleSelect * selection = selection_
                 ? reinterpret_cast<SimpleSelect*>(selection_)
                 : NULL
                 ;
    it = track.hits.begin();
    if(selection) {
        while( it != track.hits.end()
            && (selection->nMin > it->first || selection->nMax < it->first)) {
            ++it;
        }
    }
}

static void
_track_hits_it_destroy( hdql_It_t it_, hdql_Context_t ctx ) {
    delete reinterpret_cast<decltype(Track::hits)::iterator*>(it_);
}

static void
_track_hits_get_key( hdql_Datum_t track_
                   , hdql_It_t it_
                   , struct hdql_CollectionKey * dest
                   , hdql_Context_t ctx
                   ) {
    auto & it = *reinterpret_cast<decltype(Track::hits)::iterator*>(it_);
    assert(dest->datum);
    *reinterpret_cast<unsigned int*>(dest->datum) = static_cast<unsigned int>(it->first);
}

// event

static hdql_Datum_t _event_get_eventID( hdql_Datum_t track_, hdql_Context_t ctx_, hdql_Datum_t unused_) {
    return reinterpret_cast<hdql_Datum_t>(&reinterpret_cast<Event*>(track_)->eventID);
}

// event::hits collection

static hdql_It_t
_event_hits_it_create( hdql_Datum_t event_
                     , hdql_SelectionArgs_t selArgs
                     , hdql_Context_t ctx_
                     ) {
    auto itPtr = new decltype(Event::hits)::iterator;
    return reinterpret_cast<hdql_It_t>(itPtr);
}

static hdql_Datum_t
_event_hits_it_dereference( hdql_Datum_t event_
                          , hdql_It_t it_
                          , hdql_Context_t ctx_
                          ) {
    auto & it    = *reinterpret_cast<decltype(Event::hits)::iterator*>(it_);
    auto & event = *reinterpret_cast<Event*>(event_);
    if(event.hits.end() == it) return NULL;
    return reinterpret_cast<hdql_Datum_t>(it->second.get());
}

hdql_It_t
_event_hits_it_advance( hdql_Datum_t event_
                      , hdql_SelectionArgs_t selection_
                      , hdql_It_t it_
                      , hdql_Context_t ctx_
                      ) {
    assert(it_);
    auto & event = *reinterpret_cast<Event*>(event_);
    auto & it =    *reinterpret_cast<decltype(Event::hits)::iterator*>(it_);
    SimpleSelect * selection = selection_
                 ? reinterpret_cast<SimpleSelect*>(selection_)
                 : NULL
                 ;
    assert(event.hits.end() != it);
    do {
        ++it;
    } while( selection
         && (it != event.hits.end()
             && (it->first < selection->nMin || it->first > selection->nMax) ) );
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_event_hits_it_reset( hdql_Datum_t event_
                    , hdql_SelectionArgs_t selection_
                    , hdql_It_t it_
                    , hdql_Context_t ctx_
                    ) {
    assert(event_);
    assert(it_);
    auto & hits = reinterpret_cast<Event*>(event_)->hits;
    auto & it = *reinterpret_cast<decltype(Event::hits)::iterator*>(it_);
    it = hits.begin();;

    SimpleSelect * selection = selection_
                 ? reinterpret_cast<SimpleSelect*>(selection_)
                 : NULL
                 ;
    it = hits.begin();
    if(selection) {
        while( it != hits.end()
            && (selection->nMin > it->first || selection->nMax < it->first)) {
            ++it;
        }
    }
}

static void
_event_hits_it_destroy( hdql_It_t it_, hdql_Context_t ctx ) {
    delete reinterpret_cast<decltype(Event::hits)::iterator*>(it_);
}

static void
_event_hits_get_key( hdql_Datum_t event_
                   , hdql_It_t it_
                   , struct hdql_CollectionKey * dest
                   , hdql_Context_t ctx
                   ) {
    auto & it = *reinterpret_cast<decltype(Event::hits)::iterator*>(it_);
    assert(dest->datum);
    *reinterpret_cast<unsigned int*>(dest->datum) = static_cast<unsigned int>(it->first);
}


// event::tracks collection

static hdql_It_t
_event_tracks_it_create( hdql_Datum_t event_
                       , hdql_SelectionArgs_t selArgs
                       , hdql_Context_t ctx_
                       ) {
    auto itPtr = new decltype(Event::tracks)::iterator;
    *itPtr = reinterpret_cast<Event*>(event_)->tracks.begin();
    return reinterpret_cast<hdql_It_t>(itPtr);
}

static hdql_Datum_t
_event_tracks_it_dereference( hdql_Datum_t event_
                            , hdql_It_t it_
                            , hdql_Context_t ctx_
                            ) {
    auto & it    = *reinterpret_cast<decltype(Event::tracks)::iterator*>(it_);
    auto & event = *reinterpret_cast<Event*>(event_);
    if(event.tracks.end() == it) return NULL;
    assert(it->get());
    return reinterpret_cast<hdql_Datum_t>(it->get());
}

hdql_It_t
_event_tracks_it_advance( hdql_Datum_t event_
                        , hdql_SelectionArgs_t selection_
                        , hdql_It_t it_
                        , hdql_Context_t ctx_
                        ) {
    assert(it_);
    auto & event = *reinterpret_cast<Event*>(event_);
    auto & it =    *reinterpret_cast<decltype(Event::tracks)::iterator*>(it_);
    assert(event.tracks.end() != it);
    if(!selection_) {
        ++it;
    } else {
        SimpleSelect & selection = *reinterpret_cast<SimpleSelect*>(selection_);
        do {
            ++it;
        } while( it != event.tracks.end()
             && std::distance(event.tracks.begin(), it) <  selection.nMin
             && std::distance(event.tracks.begin(), it) <= selection.nMax );
        if(std::distance(event.tracks.begin(), it) > selection.nMax)
            it = event.tracks.end();
    }
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_event_tracks_it_reset( hdql_Datum_t event_
                      , hdql_SelectionArgs_t selection_
                      , hdql_It_t it_
                      , hdql_Context_t ctx_
                      ) {
    assert(event_);
    assert(it_);
    auto & it = *reinterpret_cast<decltype(Event::tracks)::iterator*>(it_);
    auto & tracks = reinterpret_cast<Event*>(event_)->tracks;
    it = tracks.begin();
    if(!selection_) return;
    auto & selection = *reinterpret_cast<SimpleSelect*>(selection_);
    while(it != tracks.end()
            && std::distance(tracks.begin(), it) <  selection.nMin
            && std::distance(tracks.begin(), it) <= selection.nMax
          ) {
        ++it;
    }
    if(std::distance(tracks.begin(), it) > selection.nMax)
        it = tracks.end();
}

static void
_event_tracks_it_destroy( hdql_It_t it_
                        , hdql_Context_t ctx_
                        ) {
    delete reinterpret_cast<decltype(Event::tracks)::iterator*>(it_);
}

static void
_event_tracks_get_key( hdql_Datum_t event_
                     , hdql_It_t it_
                     , struct hdql_CollectionKey * dest
                     , hdql_Context_t ctx
                     ) {
    auto & event = *reinterpret_cast<Event*>(event_);
    auto & it = *reinterpret_cast<decltype(Event::tracks)::iterator*>(it_);
    ssize_t nHit = std::distance(event.tracks.begin(), it);
    assert(nHit >= 0);
    assert(static_cast<size_t>(nHit) < event.tracks.size());
    assert(dest->datum);
    *reinterpret_cast<unsigned int*>(dest->datum) = static_cast<unsigned int>(nHit);
}

void
fill_data_sample_1( Event & ev ) {
    std::shared_ptr<Hit> h1 = std::make_shared<Hit>(Hit{ 1, 2, 3.4, 5.6, 7.8})
                       , h2 = std::make_shared<Hit>(Hit{ 2, 3, 4.5, 6.7, 8.9})
                       , h3 = std::make_shared<Hit>(Hit{ 3, 4, 5.6, 7.8, 9.0})
                       , h4 = std::make_shared<Hit>(Hit{ 4, 5, 6.7, 8.9, 0.1})
                       , h5 = std::make_shared<Hit>(Hit{ 5, 6, 7.8, 9.0, 1.2})
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

//                      * * *   * * *   * * *

int
fill_tables( const hdql_ValueTypes * valTypes
           , hdql_Compound * eventCompound
           , hdql_Compound * trackCompound
           , hdql_Compound * hitCompound
           ) {
    // hit compound
    int idx = 0;
    int rc;
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _hit_get_edep;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( hitCompound, "energyDeposition", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Hit::energyDeposition" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _hit_get_time;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( hitCompound, "time", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Hit::time" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _hit_get_x;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( hitCompound, "x", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Hit::x" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _hit_get_y;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( hitCompound, "y", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Hit::y" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _hit_get_z;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( hitCompound, "z", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Hit::z" << std::endl;
            return -1;
        }
    }

    // track compound
    idx = 0;
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _track_get_chi2;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( trackCompound, "chi2", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Track::chi2" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _track_get_ndf;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "int");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( trackCompound, "ndf", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Track::ndf" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _track_get_pValue;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "float");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( trackCompound, "pValue", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Track::pValue" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x0;
        ad.isCollection = 0x1;
        ad.interface.collection.keyTypeCode = hdql_types_get_type_code(valTypes, "unsigned int");
        ad.interface.collection.create      = _track_hits_it_create;
        ad.interface.collection.dereference = _track_hits_it_dereference;
        ad.interface.collection.advance     = _track_hits_it_advance;
        ad.interface.collection.reset       = _track_hits_it_reset;
        ad.interface.collection.destroy     = _track_hits_it_destroy;
        ad.interface.collection.get_key     = _track_hits_get_key;
        ad.interface.collection.compile_selection = _compile_simple_selection;
        ad.interface.collection.free_selection = _free_simple_selection;
        ad.typeInfo.compound = hitCompound;
        rc = hdql_compound_add_attr( trackCompound, "hits", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr track::hits" << std::endl;
            return -1;
        }
    }

    // event compound
    idx = 0;
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x1;
        ad.isCollection = 0x0;
        ad.interface.scalar.dereference = _event_get_eventID;
        ad.typeInfo.atomic.arithTypeCode = hdql_types_get_type_code(valTypes, "int");
        assert(0 != ad.typeInfo.atomic.arithTypeCode);
        ad.typeInfo.atomic.isReadOnly = 0x0;
        rc = hdql_compound_add_attr( eventCompound, "eventID", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Event::eventID" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x0;
        ad.isCollection = 0x1;
        ad.interface.collection.keyTypeCode = hdql_types_get_type_code(valTypes, "unsigned int");
        ad.interface.collection.create      = _event_hits_it_create;
        ad.interface.collection.dereference = _event_hits_it_dereference;
        ad.interface.collection.advance     = _event_hits_it_advance;
        ad.interface.collection.reset       = _event_hits_it_reset;
        ad.interface.collection.destroy     = _event_hits_it_destroy;
        ad.interface.collection.get_key     = _event_hits_get_key;
        ad.interface.collection.compile_selection = _compile_simple_selection;
        ad.interface.collection.free_selection = _free_simple_selection;
        ad.typeInfo.compound = hitCompound;
        rc = hdql_compound_add_attr( eventCompound, "hits", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Event::hits" << std::endl;
            return -1;
        }
    }
    {
        hdql_AttributeDefinition ad;
        hdql_attribute_definition_init(&ad);
        ad.isAtomic = 0x0;
        ad.isCollection = 0x1;
        ad.interface.collection.keyTypeCode = hdql_types_get_type_code(valTypes, "unsigned int");
        ad.interface.collection.create      = _event_tracks_it_create;
        ad.interface.collection.dereference = _event_tracks_it_dereference;
        ad.interface.collection.advance     = _event_tracks_it_advance;
        ad.interface.collection.reset       = _event_tracks_it_reset;
        ad.interface.collection.destroy     = _event_tracks_it_destroy;
        ad.interface.collection.get_key     = _event_tracks_get_key;
        ad.interface.collection.compile_selection = _compile_simple_selection;
        ad.interface.collection.free_selection = _free_simple_selection;
        ad.typeInfo.compound = trackCompound;
        rc = hdql_compound_add_attr( eventCompound, "tracks", ++idx
                              , &ad
                              );
        if(rc) {
            std::cerr << "error adding attr Event::tracks" << std::endl;
            return -1;
        }
    }
    return 0;
}

void
TestingEventStruct::SetUp() {
    _context = hdql_context_create();

    // reentrant table with type interfaces
    _valueTypes = hdql_context_get_types(_context);
    // add standard (int, float, etc) types
    hdql_value_types_table_add_std_types(_valueTypes);
    // reentrant table with operations
    _operations = hdql_context_get_operations(_context);
    hdql_op_define_std_arith(_operations, _valueTypes);
    // this is the compound types definitions (TODO: should be wrapped in C++
    // template-based helpers)
    _eventCompound = hdql_compound_new("Event");
    _trackCompound = hdql_compound_new("Track");
    _hitCompound   = hdql_compound_new("Hit");
    int rc = fill_tables( _valueTypes
           , _eventCompound
           , _trackCompound
           , _hitCompound
           );
    if(rc) throw std::runtime_error("failed to initialize type tables");
}

void
TestingEventStruct::TearDown() {
    hdql_context_destroy(_context);
    hdql_compound_destroy(_eventCompound);
    hdql_compound_destroy(_hitCompound);
    hdql_compound_destroy(_trackCompound);
}

}  // namespace ::hdql::test
}  // namespace hdql


