#pragma once

#include "events-struct.hh"

namespace hdql {
namespace test {

// Checks that compiled query returns a sequence of results with certain keys.
// Does not check the order of queries. Relies on the testing "event" compounds
// defined via C++ helper struct. All the key types defined in these compounds
// can be casted to integer.
class QueryIterationTest : public TestingEventStruct {
protected:
    struct EntryWithVisitedMarker {
        int keys[6];
        double result;
        bool isVisited;
    };
    std::vector<EntryWithVisitedMarker> _entries;
    const hdql_ValueInterface * _resultVI;
public:
    struct ExpectedEntry {
        // list of integer keys, terminated by -1
        int keys[6];
        // expected result value
        double result;
    };

    void SetExpectations(const ExpectedEntry *);

    void IterateResultsOn(const Event &ev);

    void CheckAllResolved();
};

}  // namespace ::hdql::test
}  // namespace hdql

