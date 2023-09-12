#include "hdql/attr-def.h"
#include "hdql/query-key.h"

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <limits>
#include <cassert>

namespace hdql {
namespace test {

typedef unsigned int DetID_t;

struct SimpleSelect {
    ssize_t nMin, nMax;
};

static hdql_SelectionArgs_t
_compile_simple_selection( const char * expr_
        , const hdql_Datum_t definitionData
        , hdql_Context_t ctx
        ) {
    char errBuf[128];
    std::string expr(expr_);
    size_t n = expr.find('-');
    if(n == std::string::npos) {
        snprintf(errBuf, sizeof(errBuf), "Can't parse det.delstion expression \"%s\""
                  " -- no delimiter \"-\" found"
                , expr_ );
        return NULL;
    }
    SimpleSelect * ds = new SimpleSelect;
    std::string from = expr.`substr'(0, n)
              , to = expr.`substr'(n+1)
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
_free_simple_selection( const hdql_Datum_t definitionData
                      , hdql_SelectionArgs_t args
                      , hdql_Context_t ctx
                      ) {
    delete reinterpret_cast<SimpleSelect *>(args);
}

dnl Declare structures
include(`templates/push-defs-declare.m4')dnl
include(`templates/for-all-types.m4')dnl
include(`templates/pop-defs.m4')dnl
}  // namespace hdql
}  // namespace test

dnl Implement interfaces
include(`templates/push-defs-impl-iface.m4')
include(`templates/for-all-types.m4')
include(`templates/pop-defs.m4')

dnl Register types
int
fill_tables( const hdql_ValueTypes * valTypes
           , hdql_Compound * eventCompound
           , hdql_Compound * trackCompound
           , hdql_Compound * hitCompound
           , hdql_Context_t context
           ) {
    hdql_ValueTypeCode_t size_t_TypeCode = hdql_types_get_type_code(valTypes, "size_t");
include(`templates/push-defs-register-attr-def.m4')
include(`templates/for-all-types.m4')
include(`templates/pop-defs.m4')
}
