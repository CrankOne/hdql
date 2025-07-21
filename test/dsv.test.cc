/* DSV (CSV) dump testing code
 *
 * Has crude CSV parser to interpret both the expected output and query
 * results.
 */

#include <cstdio>
#include <exception>
#include <gtest/gtest.h>

#include <charconv>

#include "events-struct.hh"
#include "samples.hh"

#include "hdql/attr-def.h"
#include "hdql/helpers/query-results-handler-csv.h"

//                                                                    ________
// _________________________________________________________________/ Fixture

using hdql::test::TestingEventStruct;

class DSVDumpTest : public TestingEventStruct {
protected:
    // type alias for tokenized CSV row for comparison
    using RowMap = std::unordered_map<std::string, std::string>;
    using Value = std::variant<std::monostate, int64_t, double, std::string>;

    // crude CSV parser
    static std::vector<RowMap> parse_csv(const std::string& csv) {
        std::vector<RowMap> result;
        std::istringstream iss(csv);
        std::string line;
        std::vector<std::string> columns;

        auto split = [](const std::string& s) -> std::vector<std::string> {
                    std::vector<std::string> tokens;
                    std::string token;
                    std::istringstream ss(s);
                    while (std::getline(ss, token, ',')) {
                        tokens.push_back(token);
                    }
                    return tokens;
                };
        // tokenize header
        if( !std::getline(iss, line) ) return result;
        columns = split(line);
        // tokenize rows
        while( std::getline(iss, line) ) {
            std::vector<std::string> cells = split(line);
            if( cells.size() != columns.size() ) {
                throw std::runtime_error("CSV row/column size mismatch");
            }

            RowMap row;
            for (size_t i = 0; i < columns.size(); ++i) {
                row[columns[i]] = cells[i];
            }
            result.push_back(std::move(row));
        }

        return result;
    }

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

    bool values_match( const Value & a, const Value & b, double floatTolerance = 1e-6) {
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

    // compare expected vs actual (unordered columns)
    void check_csv_match( const std::vector<RowMap> & expected
                        , const std::vector<RowMap> & generated
                        ) {
        ASSERT_EQ(expected.size(), generated.size())
            << "Row count mismatch: expected " << expected.size()
            << ", got " << generated.size();

        for( size_t i = 0; i < expected.size(); ++i ) {
            for( const auto& [key, expectedValue] : expected[i] ) {
                auto it = generated[i].find(key);
                ASSERT_NE(it, generated[i].end())
                        << "Missing column: \"" << key << "\"";
                Value expv   = parse_value(expectedValue)
                    , actual = parse_value(it->second)
                    ;
                EXPECT_TRUE( values_match(expv, actual) )
                        << "Mismatch at row " << i << ", column: " << key
                        << "; expected: " << expectedValue
                        << ", got: " << it->second;
            }
        }
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

        std::vector<RowMap> expectedTable;
        ASSERT_NO_THROW(expectedTable = parse_csv(expectedOutput))
            << "while parsing expected data for request \""
            << queryExpr << "\": " << "\n" << expectedOutput;
        

        std::vector<RowMap> actualTable;
        ASSERT_NO_THROW(actualTable = parse_csv(_buf))
            << "while parsing actual data for request \""
            << queryExpr << "\": " << "\n" << _buf;

        check_csv_match(expectedTable, actualTable);  // check results
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
        hdql_query_results_handler_csv_cleanup(&_iqr);
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

TEST_F(DSVDumpTest, handlesCompoundResult ) {
    hdql::test::Event ev;
    hdql::test::fill_data_sample_1(ev);
    check_request(".hits", R"~(key0,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
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
    check_request(".hits{a:=.x, b:=.y, c:=.z}", R"~(key0,c,b,a,NrawData.samples,rawData.time,z,y,x,time,energyDeposition
301,1.2,9.00e+00,7.80e+00,4,5.00e-02,1.20e+00,9.0e+00,7.80e+00,6.00e+00,5.00e+00
202,5.0e-01,8.90e+00,6.70e+00,4,N/A,5.00e-01,8.90e+00,6.70e+00,5.00e+00,4.00e+00
103,9.0e+00,7.80e+00,5.60e+00,4,3.00e-02,9.00e+00,7.80e+00,5.60e+00,4.00e+00,3.00e+00
102,8.9e+00,6.70e+00,4.50e+00,4,2.00e-02,8.90e+00,6.70e+00,4.50e+00,3.00e+00,2.00e+00
101,7.8e+00,5.60e+00,3.40e+00,4,1.00e-02,7.80e+00,5.60e+00,3.40e+00,2.00e+00,1.00e+00
)~", ev);
}

// .tracks.hits.rawData.samples         iterates atomic collection
// .tracks.hits[50:103].rawData.time    iterates collection with selector
// .hits{a:=.x, b:=.y, c:=.z:.b > 7}    resolves compound attributes
//
// Really complex one:
//
//  .tracks[->trackID]{
//        c2n := .chi2/.ndf
//      , h:=.hits[->hitID]{:.z < 6} 
//      : .c2n > 3
//      }.h{ xx:=.x
//         , yy:=.y
//         : sqrt(.xx + .yy) < 7
//         }
// Note, that moving [->hitID] from .hits to .h causes assertion failure;
// unclear, whether this is permitted state. Same problematic case is:
//  .tracks[->trackID]{c2n := .chi2/.ndf, h:=.hits{:.z < 6} : .c2n > 0}

