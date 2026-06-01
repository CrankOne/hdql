#include "compiled-query.hh"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/helpers/fancy-print-err.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/value.h"

namespace hdql {
namespace test {

TestCompiledQuery::TestCompiledQuery()
            : _compoundsContext(nullptr)
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
    _compoundsContext = hdql_context_create_descendant(_context, HDQL_CTX_PRINT_PUSH_ERROR);
    // this is the compound types definitions
    _define_compounds(_compoundsContext, _rootCompound);
}

void
TestCompiledQuery::CompileQuery(const char * expression, bool enableKeys) {
    char errBuf[128]; int errDetails[5];
    _query = hdql_compile_query( expression
                              , _rootCompound
                              , _compoundsContext
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
        _queryKey = hdql_key_new(_compoundsContext);
        rc = hdql_key_reserve_for_query(_query, _queryKey, _compoundsContext);
        ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
        _flatKeyViewLen = hdql_key_flat_view_size(_queryKey, _compoundsContext);
        if(_flatKeyViewLen) {
            _flatKeyView = new hdql_Key_t [_flatKeyViewLen];
            rc = hdql_key_flat_view_populate(_queryKey, _flatKeyView);
            ASSERT_EQ(rc, HDQL_ERR_CODE_OK);
            _flatKeyIfaces = new const hdql_ValueInterface *[_flatKeyViewLen];
            hdql_ValueTypes *types = hdql_context_get_types(_compoundsContext);
            for(size_t i = 0; i < _flatKeyViewLen; ++i) {
                ASSERT_TRUE(hdql_key_is_datum(_flatKeyView[i]));
                _flatKeyIfaces[i] = hdql_types_get_type(types, hdql_key_datum_get_type_code(_flatKeyView[i]));
                ASSERT_TRUE(_flatKeyIfaces[i]);
            }
        }
    }
}

void
TestCompiledQuery::ResetQuery(hdql_Datum *datum, hdql_Datum *&result) {
    ASSERT_TRUE(_query);
    result = hdql_query_reset(_query, reinterpret_cast<hdql_Datum_t>(datum)
                    , _queryKey
                    , _compoundsContext);
    ASSERT_FALSE(hdql_context_has_errors(_compoundsContext));
    // ^^^ TODO: details on error
}

void
TestCompiledQuery::AdvanceQuery(hdql_Datum *&result) {
    result = hdql_query_get(_query, _queryKey, _compoundsContext);
    ASSERT_FALSE(hdql_context_has_errors(_compoundsContext));
    // ^^^ TODO: details on error
}

void
TestCompiledQuery::TearDown() {
    if(_flatKeyIfaces)
        delete [] _flatKeyIfaces;
    if(_flatKeyView)
        delete [] _flatKeyView;
    if(_queryKey)
        hdql_key_destroy(_queryKey, _compoundsContext);
    if(_query)
        hdql_query_destroy(_query, _compoundsContext);
    if(_compoundsContext)
        hdql_context_destroy(_compoundsContext);
    TestingContext::TearDown();
}

}  // namespace ::hdql::test
}  // namespace hdql

