#include "iteration-results.hh"
#include "hdql/query-key.h"
#include <gtest/gtest.h>
#include <sstream>

namespace hdql {
namespace test {

void
QueryIterationTest::SetExpectations(const ExpectedEntry * entries_) {
    // allocated list of expected results, but with "is visited" marker
    int keyLen = 0;

    size_t nEntries = 0;
    for(auto src = entries_; src->keys[0] != -1; ++src, ++nEntries) {}
    assert(nEntries > 0);
    _entries.resize(nEntries);
    auto it = _entries.begin();
    for(auto src = entries_; src->keys[0] != -1; ++src, ++nEntries, ++it) {
        // copy keys
        int i;
        for(i = 0; ; ++i) {
            it->keys[i] = src->keys[i];
            if(it->keys[i] == -1) break;
        }
        if(0 == keyLen) {
            keyLen = i;
        }
        #ifndef NDEBUG
        else {
            // this is mistake in entries data, not a subject of UT
            assert(keyLen == i);
        }
        #endif
        // copy value
        it->result = src->result;
        // unset "is visited"
        it->isVisited = false;
    }

    // check the query results in a kyy of expected length (flat form)
    ASSERT_EQ(_flatKeyViewLen, keyLen);

    // check that the result is of type that can be casted to float
    const hdql_AttrDef * ad = hdql_query_top_attr(_query);
    ASSERT_TRUE(ad);
    ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
    _resultVI = hdql_types_get_type(_valueTypes, hdql_attr_def_get_atomic_value_type_code(ad));
    ASSERT_TRUE(_resultVI);
}

void
QueryIterationTest::IterateResultsOn(const Event &ev) {
    // iterate over query results, use simple scan to locate visited entry
    hdql_Datum_t r;
    ResetQuery(reinterpret_cast<hdql_Datum_t>(const_cast<Event *>(&ev)), r);
    while(r) {
        int kk[3];  // key to control
        kk[2] = -1;
        for(size_t lvl = 0; lvl < _flatKeyViewLen; ++lvl) {
            kk[lvl] = _flatKeyIfaces[lvl]->get_as_int(hdql_key_datum_get(_flatKeyView[lvl]));
        }
        // locate and mark as visited, assuring it was not visited before
        bool found = false;
        for(size_t i = 0; i < _entries.size(); ++i) {
            bool thisOne = true;
            for(size_t j = 0; j < _flatKeyViewLen; ++j) {
                if( kk[j] == _entries[i].keys[j] ) continue;
                thisOne = false;
                break;
            }
            if(!thisOne) continue;
            found = true;
            EXPECT_FALSE(_entries[i].isVisited);
            EXPECT_NEAR(_resultVI->get_as_float(r), _entries[i].result, 1e-6);
            _entries[i].isVisited = true;
            break;
        }
        std::string keyStr;
        if(!found) {
            std::ostringstream oss;
            oss << "(";
            for(size_t lvl = 0; lvl < _flatKeyViewLen; ++lvl) {
                oss << (lvl ? ", " : "") << kk[lvl];
            }
            oss << ")";
            keyStr = oss.str();
        }
        EXPECT_TRUE(found) << " unexpected item " << keyStr << ", value is "
            << _resultVI->get_as_float(r);
        AdvanceQuery(r);
    }
}

void
QueryIterationTest::CheckAllResolved() {
    // assure all have been visited
    for(auto entry: _entries) {
        std::string keyStr;
        if(!entry.isVisited) {
            std::ostringstream oss;
            oss << "(";
            for(size_t lvl = 0; lvl < _flatKeyViewLen; ++lvl) {
                oss << (lvl ? ", " : "") << entry.keys[lvl];
            }
            oss << ")";
            keyStr = oss.str();
        }
        EXPECT_TRUE(entry.isVisited) << "missing item " << keyStr
            << ", value " << entry.result;
    }
}

}  // namespace ::hdql::test
}  // namespace hdql


