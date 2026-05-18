#include "compiled-query.hh"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/helpers/fancy-print-err.h"
#include "hdql/query-key.h"
#include "hdql/value.h"

namespace hdql {
namespace test {

TestCompiledQuery::TestCompiledQuery()
            : _compounds(nullptr)
            , _rootCompound(nullptr)
            , _query(nullptr)
            , _queryKey(nullptr)
            , _flatKeyViewLen(0)
            , _flatKeyView(nullptr)
            , _flatKeyIfaces(nullptr)
            {}

void
TestCompiledQuery::SetUp() {
    TestingContext::SetUp();
    // this is the compound types definitions
    _compounds = _define_compounds(_ctx, _rootCompound);
}

void
TestCompiledQuery::CompileQuery(const char * expression, bool enableKeys) {
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query( expression
                              , _rootCompound
                              , _compounds.context_ptr()
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
    int rc = errDetails[0];
    if(rc) {
        char fancyprintbuf[1024];
        rc = hdql_fancy_error_print(fancyprintbuf, sizeof(fancyprintbuf)
                , expression, errDetails, errBuf, 0x1);
        fputs(fancyprintbuf, stderr);
        if(rc) {
            fputs("(truncated)\n", stderr);
        }
    }
    ASSERT_EQ(errDetails[0], 0);

    if(enableKeys) {
        _queryKey = hdql_key_new(_compounds.context_ptr());
        rc = hdql_key_reserve_for_query(_query, _queryKey, _compounds.context_ptr());
        ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
        _flatKeyViewLen = hdql_key_flat_view_size(_queryKey, _compounds.context_ptr());
        if(_flatKeyViewLen) {
            _flatKeyView = new hdql_Key_t [_flatKeyViewLen];
            rc = hdql_key_flat_view_populate(_queryKey, _flatKeyView);
            ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
            _flatKeyIfaces = new const hdql_ValueInterface *[_flatKeyViewLen];
            hdql_ValueTypes *types = hdql_context_get_types(_compounds.context_ptr());
            for(size_t i = 0; i < _flatKeyViewLen; ++i) {
                ASSERT_TRUE(hdql_key_is_datum(_flatKeyView[i]));
                _flatKeyIfaces[i] = hdql_types_get_type(types, hdql_key_datum_get_type_code(_flatKeyView[i]));
                ASSERT_TRUE(_flatKeyIfaces[i]);
            }
        }
    }
}

void
TestCompiledQuery::TearDown() {
    if(_flatKeyView)
        delete [] _flatKeyView;
    if(_queryKey)
        hdql_key_destroy(_queryKey, _compounds.context_ptr());
    if(_query)
        hdql_query_destroy(_query, _compounds.context_ptr());
    // sic! in this order: non-virtual compounds get destroyed AFTER context as
    // they are used to resolve attribute definitions in queries while cleaning
    // up queries
    TestingContext::TearDown();
    for(auto & ce : _compounds) {
        hdql_compound_destroy(ce.second, _compounds.context_ptr());
    }
}

}  // namespace ::hdql::test
}  // namespace hdql

