#include <gtest/gtest.h>
#include <stdexcept>
#include <typeindex>

#include "events-struct.hh"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/types.h"
#include "hdql/value.h"
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
            = hdql_context_create_descendant(_compoundsContext, HDQL_CTX_PRINT_PUSH_ERROR);

        // create and fill event to operate
        fill_data_sample_1(_ev);

        // get event's compound definition
        {
            std::type_index ti = typeid(hdql::test::Event);
            _rootCompound = hdql_compounds_get_by_type_id(hdql_context_get_compounds(_thisContext), &ti, sizeof(std::type_index));
            ASSERT_TRUE(_rootCompound);
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
    Query q(".tracks.hits.x", _rootCompound, _thisContext, true);
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_const_value());
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
    for(GenericQueryCursor qc = q.generic_cursor_on(_ev); qc; ++qc) {
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
            hdql_Key * curKey = hdql_key_get_list_item(const_cast<hdql_Key*>(q.keys()), lvl);
            if(hdql_key_is_empty(curKey)) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            ASSERT_TRUE(hdql_key_is_datum(curKey));
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes
                    , hdql_key_datum_get_type_code(curKey));
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(hdql_key_datum_get(curKey));
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
    // add standard type conversions
    hdql_converters_add_std(hdql_context_get_conversions(_thisContext)
            , hdql_context_get_types(_thisContext), _thisContext);

    // instantiate C++ query helper
    // -> query all set of the hits within tracks instances associated with an event
    Query q(".tracks.hits.x", _rootCompound, _thisContext, true);
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_const_value());
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
        for(QueryCursor<double> qc = q.cursor_on<double>(_ev); qc; ++qc) {
            int kk[3];  // key to control
            for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
                hdql_Key * curKey = hdql_key_get_list_item(const_cast<hdql_Key*>(q.keys()), lvl);
                if(hdql_key_is_empty(curKey)) {
                    kk[lvl] = -1;
                    //printf( " --- " );  // xxx
                    continue;
                }
                ASSERT_TRUE(hdql_key_is_datum(curKey));
                const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes
                        , hdql_key_datum_get_type_code(curKey) );
                assert(vi->get_as_string);
                //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
                //printf(" %s ", bf);
                kk[lvl] = vi->get_as_int(hdql_key_datum_get(curKey));
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
    //Query q = _compounds.query<hdql::test::Event>(".tracks.hits.x");  // TODO: restore this API
    Query q(".tracks.hits.x", _rootCompound, _thisContext, true);
    
    // make sure the query has been result deduced properly
    // - is of atomic type
    ASSERT_TRUE(q.is_atomic());
    ASSERT_FALSE(q.is_compound());
    //   iterades on its own)
    ASSERT_TRUE(q.is_scalar());
    ASSERT_FALSE(q.is_collection());
    // - is not a static value
    ASSERT_FALSE(q.is_static_const_value());
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
    QueryCursor<float> qc = q.cursor<float>();

    EXPECT_THROW(++qc, hdql::errors::NoQueryTarget);

    qc.reset(_ev);
    for(; qc; ++qc) {
        int kk[3];  // key to control
        for(size_t lvl = 0; lvl < q.keys_depth(); ++lvl) {
            hdql_Key * curKey = hdql_key_get_list_item(const_cast<hdql_Key*>(q.keys()), lvl);
            if(hdql_key_is_empty(curKey)) {
                kk[lvl] = -1;
                //printf( " --- " );  // xxx
                continue;
            }
            ASSERT_TRUE(hdql_key_is_datum(curKey));
            const hdql_ValueInterface * vi = hdql_types_get_type(_valueTypes
                        , hdql_key_datum_get_type_code(curKey) );
            assert(vi->get_as_string);
            //vi->get_as_string(keys[lvl].datum, bf, sizeof(bf));
            //printf(" %s ", bf);
            kk[lvl] = vi->get_as_int(hdql_key_datum_get(curKey));
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
