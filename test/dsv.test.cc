/* DSV (CSV) dump testing code
 *
 * Has crude CSV parser to interpret both the expected output and query
 * results.
 */

#include <cstdio>
#include <algorithm>
#include <charconv>
#include <variant>

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <sstream>

#include <gtest/gtest.h>

#include "events-struct.hh"
#include "hdql/query.h"
#include "samples.hh"

#include "hdql/attr-def.h"
#include "hdql/helpers/query-results-handler-csv.h"

//                                                                    ________
// _________________________________________________________________/ Fixture

using hdql::test::TestingEventStruct;

class DSVDumpTest : public TestingEventStruct {
protected:

    //
    // Value
    using Value = std::variant<std::monostate, int64_t, double, std::string>;
    // Parses value into a suitable type
    static Value parse_value(const std::string& s) {
        // Special case: non-available marker
        if (s == "N/A") return std::monostate{};
        // int
        int64_t i;
        auto [p1, ec1] = std::from_chars(s.data(), s.data() + s.size(), i);
        if( ec1 == std::errc() && p1 == s.data() + s.size() ) return i;
        // float
        double d;
        auto [p2, ec2] = std::from_chars(s.data(), s.data() + s.size(), d,
                                         std::chars_format::general);
        if( ec2 == std::errc() && p2 == s.data() + s.size() ) return d;
        // fallback to str
        return s;
    }
    // Type-casting comparison for values
    static bool values_match( const Value & a, const Value & b, double floatTolerance = 1e-6) {
        if( a.index() != b.index()) {
            // comparing int vs double, promote and compare numerically
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<double>(b))
                return std::abs(static_cast<double>(std::get<int64_t>(a)) - std::get<double>(b)) < floatTolerance;
            if (std::holds_alternative<double>(a) && std::holds_alternative<int64_t>(b))
                return std::abs(std::get<double>(a) - static_cast<double>(std::get<int64_t>(b))) < floatTolerance;
            return false;
        }
        // same type comparison
        return std::visit( [&](const auto& va) -> bool {
            using T = std::decay_t<decltype(va)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return true;
            else if constexpr (std::is_same_v<T, double>)
                return std::abs(va - std::get<double>(b)) < floatTolerance;
            else return va == std::get<T>(b);
        }, a );
    }

    // row and table representation
    using Row = std::map<std::string, Value>;
    using Table = std::vector<Row>;

    static bool row_less(const Row& a, const Row& b) {
        auto ita = a.begin();
        auto itb = b.begin();

        while (ita != a.end() && itb != b.end()) {
            if (ita->first != itb->first)
                return ita->first < itb->first;

            const Value& va = ita->second;
            const Value& vb = itb->second;

            // Compare by type index first to ensure consistent ordering
            if (va.index() != vb.index())
                return va.index() < vb.index();

            // Compare by actual value
            switch (va.index()) {
                case 0: break;  // monostate, equal
                case 1: {
                    int64_t ia = std::get<1>(va);
                    int64_t ib = std::get<1>(vb);
                    if (ia != ib) return ia < ib;
                    break;
                }
                case 2: {
                    double da = std::get<2>(va);
                    double db = std::get<2>(vb);
                    if (std::abs(da - db) > 1e-12) return da < db;
                    break;
                }
                case 3: {
                    const std::string& sa = std::get<3>(va);
                    const std::string& sb = std::get<3>(vb);
                    if (sa != sb) return sa < sb;
                    break;
                }
                default:
                    break;
            }

            ++ita;
            ++itb;
        }

        return ita != a.end(); // a has more fields than b => greater
    }

    static std::vector<std::string> tokenize_csv_line(const std::string& line) {
        std::vector<std::string> result;
        std::istringstream iss(line);
        std::string cell;
        while (std::getline(iss, cell, ',')) result.push_back(cell);
        return result;
    }

    static Table parse_csv_(const std::string& csv) {
        std::istringstream iss(csv);
        std::string line;

        // Header
        std::getline(iss, line);
        auto headers = tokenize_csv_line(line);

        Table table;
        while (std::getline(iss, line)) {
            auto values = tokenize_csv_line(line);
            if (values.size() != headers.size()) continue;  // malformed
            Row row;
            for (size_t i = 0; i < headers.size(); ++i) {
                row[headers[i]] = parse_value(values[i]);
            }
            table.push_back(std::move(row));
        }
        return table;
    }

    bool check_csv_match_unordered( const std::string& expectedCsv
                , const std::string& actualCsv) {
        Table expected = parse_csv_(expectedCsv);
        // ASSERT_NO_THROW(expected = parse_csv_(expectedCsv))
        //    << "while parsing expected data:\n" << expectedCsv;

        Table actual = parse_csv_(actualCsv);
        //ASSERT_NO_THROW(actual = parse_csv_(actualCsv))
        //    << "while parsing actual data:\n" << actualCsv;

        std::sort(expected.begin(), expected.end(), row_less);
        std::sort(actual.begin(), actual.end(), row_less);

        if( expected.size() != actual.size() ) return false;

        for( size_t i = 0; i < expected.size(); ++i ) {
            const auto& rowA = expected[i];
            const auto& rowB = actual[i];
            if( rowA.size() != rowB.size() ) return false;
            for( const auto& [key, valA] : rowA ) {
                auto it = rowB.find(key);
                if (it == rowB.end()) return false;
                if( !values_match(valA, it->second) ) return false;
            }
        }
        return true;
    }

    void check_request( const char * queryExpr
            , const char * expectedOutput
            , hdql::test::Event & ev
            ) {
        // Interpret query
        char errBuf[256] = "";
        int errDetails[5] = {0, -1, -1, -1, -1};  // error code, first line, first column, last line, last column
        hdql_Query * q = hdql_compile_query( queryExpr
                              , _eventCompound
                              , _ctx
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
        ASSERT_TRUE(q != NULL)
            << "failed to interpret query \""
            << queryExpr << "\", errCode=" << errDetails[0] << ": "
            << errBuf;

        // instantiate DSV workspace
        struct hdql_QueryResultsWorkspace * ws = hdql_query_results_init(
                  q
                , NULL  // const char ** attrs
                , &_iqr  // struct hdql_iQueryResultsHandler * iqr
                , _ctx  // struct hdql_Context * ctx
                );
        ASSERT_TRUE(ws != NULL)
            << "failed to initialize results handler workspace";

        // common: this is the loop -- when new root data instance comes from
        hdql_query_results_process_records_from((hdql_Datum_t) &ev, ws);

        // common: cleanup common iteration caches
        hdql_query_results_destroy(ws);
        fclose(_ss);
        _ss = NULL;

        // NOTE: query has to be destroyed AFTER the resuls handler since
        //       handler's composition depends on the query. (TODO: destroy in processing?)
        hdql_query_results_handler_csv_cleanup(&_iqr);
        hdql_query_destroy(q, _ctx);

        check_csv_match_unordered(expectedOutput, _buf);  // check results
    }

protected:
    char * _buf;
    FILE * _ss;
    struct hdql_iQueryResultsHandler _iqr;

    void SetUp() override {
        TestingEventStruct::SetUp();
        const size_t bufLen = 4*1024;
        _buf = (char *) malloc(bufLen);
        assert(_buf);
        _ss = fmemopen(_buf, bufLen, "w");

        int rc;
        const struct hdql_DSVFormatting fmt = {
              .valueDelimiter           = ","
            , .recordDelimiter          = "\n"
            , .attrDelimiter            = "."
            , .collectionLengthMarker   = "N"
            , .anonymousColumnName      = "value"
            , .nullToken                = "N/A"
            , .unlabeledKeyColumnFormat = "key%zu"
        };

        rc = hdql_query_results_handler_csv_init(&_iqr, _ss, &fmt, _ctx);
        ASSERT_EQ(0, rc);
    }

    

    void TearDown() override {
        TestingEventStruct::TearDown();
        if(_ss) fclose(_ss);
        free(_buf);
    }
};

//                                                                      _____
// ___________________________________________________________________/ Tests

TEST_F(DSVDumpTest, handlesAtomicResult ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits.time", R"~(key0,value
301,7.80e+00
202,6.70e+00
103,5.60e+00
102,4.50e+00
101,3.40e+00
)~", ev);
}

TEST_F(DSVDumpTest, handlesAtomicCollection ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits.rawData.samples", R"~(key0,key1,value
301,0,41
301,1,42
301,2,43
301,3,44
103,0,21
103,1,22
103,2,23
103,3,24
102,0,11
102,1,12
102,2,13
102,3,14
101,0,1
101,1,2
101,2,3
101,3,4
)~", ev);
}

TEST_F(DSVDumpTest, iteratesCollectionWithSelector) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".tracks.hits[50:103].rawData.time"
            , R"~(key0,key1,value
0,102,2.000000e-02
0,101,1.000000e-02
)~", ev);
}

TEST_F(DSVDumpTest, handlesAtomicCollectionWithLabeledColumnsAndSelection ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits[0:200->hitID].rawData.samples[0:1->sampleID]", R"~(hitID,sampleID,value
103,0,21
103,1,22
102,0,11
102,1,12
101,0,1
101,1,2
)~", ev);
}

TEST_F(DSVDumpTest, handlesCompoundResult ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits",
R"~(key0,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
301,4,5.00e-02,1.20e+00,9.00e+00,7.80e+00,6.00e+00,5.00e+00
202,4,N/A,5.00e-01,8.90e+00,6.70e+00,5.00e+00,4.00e+00
103,4,3.00e-02,9.00e+00,7.80e+00,5.60e+00,4.00e+00,3.00e+00
102,4,2.00e-02,8.90e+00,6.70e+00,4.50e+00,3.00e+00,2.00e+00
101,4,1.00e-02,7.80e+00,5.60e+00,3.40e+00,2.00e+00,1.00e+00
)~", ev);
}

TEST_F(DSVDumpTest, handlesVirtualCompoundResult ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits{a:=.x, b:=.y, c:=.z}",
R"~(key0,c,b,a,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
301,1.2,9.00e+00,7.80e+00,4,5.00e-02,1.20e+00,9.0e+00,7.80e+00,6.00e+00,5.00e+00
202,5.0e-01,8.90e+00,6.70e+00,4,N/A,5.00e-01,8.90e+00,6.70e+00,5.00e+00,4.00e+00
103,9.0e+00,7.80e+00,5.60e+00,4,3.00e-02,9.00e+00,7.80e+00,5.60e+00,4.00e+00,3.00e+00
102,8.9e+00,6.70e+00,4.50e+00,4,2.00e-02,8.90e+00,6.70e+00,4.50e+00,3.00e+00,2.00e+00
101,7.8e+00,5.60e+00,3.40e+00,4,1.00e-02,7.80e+00,5.60e+00,3.40e+00,2.00e+00,1.00e+00
)~", ev);
}

TEST_F(DSVDumpTest, handlesDoubleVirtualCompound ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits{a:=.x, b:=.y, c:=.z:.b > 8}{d:=.a+.b}",
R"~(key0,d,c,b,a,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
301,1.68e+01,1.20e+00,9.00e+00,7.80e+00,4,5.00e-02,1.20e+00,9.00e+00,7.80e+00,6.00e+00,5.00e+00
202,1.56e+01,5.00e-01,8.90e+00,6.70e+00,4,N/A,5.00e-01,8.90e+00,6.70e+00,5.00e+00,4.00e+00
)~", ev);
}

// TODO: enable std math to make it work
#if 0
TEST_F(DSVDumpTest, handlesComplexQuery ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(R"~(.tracks[->trackID]{
      c2n := .chi2/.ndf
    , h:=.hits[->hitID]{:.z < 6}
    : .c2n > 3
    }.h{ xx:=.x
       , yy:=.y
       : sqrt(.xx + .yy) < 7
       })~",
R"~(trackID,hitID,yy,xx,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
2,301,9.00e+00,7.80e+00,4,5.00e-02,1.20e+00,9.00e+00,7.80e+00,6.00e+00,5.00e+00
2,202,8.90e+00,6.70e+00,4,N/A,5.00e-01,8.90e+00,6.70e+00,5.00e+00,4.00e+00
)~", ev);
    // TODO
    // Note, that moving [->hitID] from .hits to .h causes interesting errors,
    // worth to investigate or improve the parser. It also breaks the fancy
    // print function.
}
#endif

// Interesting problematic case:
//  .tracks[1:2]{:.chi2/.ndf > 6}               =>infinite loop

