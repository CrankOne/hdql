#include <gtest/gtest.h>
#include <stdexcept>

#include "events-struct.hh"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "samples.hh"

#include "hdql/helpers/query.hh"

namespace hdql {
namespace test {

class TestCppHelpers : public TestingEventStruct {
protected:
    hdql_Context_t _thisContext;
    const hdql_Compound * _eventCompoundPtr;
    hdql::test::Event _ev;
public:
    void SetUp() override {
        TestingEventStruct::SetUp();
        // create child context
        _thisContext
            = hdql_context_create_descendant(_compounds.context_ptr(), HDQL_CTX_PRINT_PUSH_ERROR);

        // create and fill event to operate
        fill_data_sample_1(_ev);

        // get event's compound definition
        {
            auto it = _compounds.find(typeid(hdql::test::Event));
            ASSERT_FALSE(_compounds.end() == it); //?
            _eventCompound = it->second;
        }
    }
    void TearDown() override {
        TestingEventStruct::TearDown();
        // destroy own (child) context
        hdql_context_destroy(_thisContext);
    }
};

// Tests functions of polymorphic query C++ wrapper
// - creates children context
// - creates and fills "event" sample (compound) instance
// - uses C++ query wrapper to build ("compile") a data query expression using
//   compound definition, and context
// - checks C++ query helper properly forwards the properties of deduced items
// - uses polymorphic ("generic") query cursor to iterate query results, making
//   sure everything was visited
// - destroys children context
TEST_F(TestCppHelpers, UseGenericQueryResultOnAtomicScalarCppWrappers) {
    // instantiate C++ query helper
    // -> query all set of the hits within tracks instances associated with an event
    helpers::Query q(".tracks.hits.x", _eventCompound, _thisContext, _compounds, true);
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    // - is a direct query (not a forwarding query generator)
    ASSERT_TRUE(q.is_direct_query());
    ASSERT_FALSE(q.is_fwd_query());
    // - is of scalar type (does not produce a collection that needs to be
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_value());
    // - is not transient (syntetic) attribute created within a context
    ASSERT_FALSE(q.is_transient());

    // iterate over results using polymorphic cursor
    // NOTE: same as in TestingEventStruct.straightDataIterationWorksOnSample1
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101, -1}, 3.4},
        {{0, 102, -1}, 4.5},
        {{2, 103, -1}, 5.6},
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
    };
    for(helpers::GenericQueryCursor qc = q.generic_cursor_on(_ev); qc; ++qc) {
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
            if(0x0 == q.keys()[lvl].code) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, q.keys()[lvl].code);
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(q.keys()[lvl].pl.datum);
        }

        bool found = false;
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kk[0] != expectedQueryResults[i].keys[0]
             || kk[1] != expectedQueryResults[i].keys[1]
             || kk[2] != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);
            expectedQueryResults[i].visited = true;

            EXPECT_NEAR(qc.get<float>(), expectedQueryResults[i].result, 1e-6);

            break;
        }
        EXPECT_TRUE(found);
    }
    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
}

TEST_F(TestCppHelpers, UseStaticQueryResultOnAtomicScalarInlineCppWrappers) {
    
    //_thisContext;
    // add standard type conversions
    hdql_converters_add_std(hdql_context_get_conversions(_thisContext), hdql_context_get_types(_thisContext));

    // instantiate C++ query helper
    // -> query all set of the hits within tracks instances associated with an event
    helpers::Query q(".tracks.hits.x", _eventCompound, _thisContext, _compounds, true);
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    // - is a direct query (not a forwarding query generator)
    ASSERT_TRUE(q.is_direct_query());
    ASSERT_FALSE(q.is_fwd_query());
    // - is of scalar type (does not produce a collection that needs to be
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_value());
    // - is not transient (syntetic) attribute created within a context
    ASSERT_FALSE(q.is_transient());

    // iterate over results using polymorphic cursor
    // NOTE: same as in TestingEventStruct.straightDataIterationWorksOnSample1
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101, -1}, 3.4},
        {{0, 102, -1}, 4.5},
        {{2, 103, -1}, 5.6},
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
    };
    for(int i = 0; i < 2; ++i) {
        // inline test case
        for(helpers::QueryCursor<double> qc = q.cursor_on<double>(_ev); qc; ++qc) {
            int kk[3];  // key to control
            for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
                if(0x0 == q.keys()[lvl].code) {
                    kk[lvl] = -1;
                    //printf( " --- " );  // xxx
                    continue;
                }
                const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, q.keys()[lvl].code);
                assert(vi->get_as_string);
                //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
                //printf(" %s ", bf);
                kk[lvl] = vi->get_as_int(q.keys()[lvl].pl.datum);
            }

            bool found = false;
            for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
                if( kk[0] != expectedQueryResults[i].keys[0]
                 || kk[1] != expectedQueryResults[i].keys[1]
                 || kk[2] != expectedQueryResults[i].keys[2]
                 ) continue;
                found = true;
                EXPECT_FALSE(expectedQueryResults[i].visited);

                EXPECT_NEAR(qc.get(), expectedQueryResults[i].result, 1e-6);

                expectedQueryResults[i].visited = true;
                break;
            }
            EXPECT_TRUE(found);
        }
        // assure all have been visited
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            EXPECT_TRUE(expectedQueryResults[i].visited);
        }
        // reset for "next event"
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            expectedQueryResults[i].visited = false;
        }
    }
}

TEST_F(TestCppHelpers, UseStaticQueryResultOnAtomicScalarCppWrappers) {
    
    // instantiate C++ query helper
    // -> query all set of the hits within tracks instances associated with an event
    //   (this time using thin template method to instantiate the query)
    helpers::Query q = _compounds.query<hdql::test::Event>(".tracks.hits.x");
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    // - is a direct query (not a forwarding query generator)
    ASSERT_TRUE(q.is_direct_query());
    ASSERT_FALSE(q.is_fwd_query());
    // - is of scalar type (does not produce a collection that needs to be
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_value());
    // - is not a transient (syntetic) attribute created within a context
    ASSERT_FALSE(q.is_transient());

    // iterate over results using polymorphic cursor
    // NOTE: same as in TestingEventStruct.straightDataIterationWorksOnSample1
    static struct {
        int keys[3];
        float result;
        bool visited;
    } expectedQueryResults[] = {
        {{0, 101, -1}, 3.4},
        {{0, 102, -1}, 4.5},
        {{2, 103, -1}, 5.6},
        {{2, 202, -1}, 6.7},
        {{2, 301, -1}, 7.8},
    };
    // out of test case, reentrant
    helpers::QueryCursor<float> qc = q.cursor<float>();

    EXPECT_THROW(++qc, hdql::errors::NoQueryTarget);

    qc.reset(_ev);
    for(; qc; ++qc) {
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
            if(0x0 == q.keys()[lvl].code) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes, q.keys()[lvl].code);
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(q.keys()[lvl].pl.datum);
        }

        bool found = false;
        for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
            if( kk[0] != expectedQueryResults[i].keys[0]
             || kk[1] != expectedQueryResults[i].keys[1]
             || kk[2] != expectedQueryResults[i].keys[2]
             ) continue;
            found = true;
            EXPECT_FALSE(expectedQueryResults[i].visited);

            EXPECT_NEAR(qc.get(), expectedQueryResults[i].result, 1e-6);

            expectedQueryResults[i].visited = true;
            break;
        }
        EXPECT_TRUE(found);
    }
    // assure all have been visited
    for(size_t i = 0; i < sizeof(expectedQueryResults)/sizeof(*expectedQueryResults); ++i) {
        EXPECT_TRUE(expectedQueryResults[i].visited);
    }
}

}  // namespace ::hdql::test
}  // namespace hdql
