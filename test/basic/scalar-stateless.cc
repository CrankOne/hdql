
// Test a very basic query iteration of stateless scalar attribute definition.
// Attribute definitions used in these queries are created just in the code
// and not provided by any compound definitions.

#include "hdql/attr-def.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include "basic-query.hh"

#include <gtest/gtest.h>
#include <stdexcept>

namespace hdql {
namespace test {

namespace {

struct ItemWithScalarAttr {
    int a;
};

// Stateless interface
//

int toCheckStateless = 0x707;

hdql_Datum_t 
reset_a_stateless( hdql_Datum_t owner
       , hdql_Datum_t dynData
       , const struct hdql_Datum *defData
       , struct hdql_Key * key
       , hdql_Context_t context
       ) {
    if(!owner) throw std::runtime_error("NULL owner");
    if(dynData) throw std::runtime_error("dyn. data set for stateless scalar");
    if(!defData) throw std::runtime_error("def. data not set for stateless scalar");
    if(defData != reinterpret_cast<hdql_Datum_t>(&toCheckStateless))
        throw std::runtime_error("addr differs for def.data, stateless case");
    if(*reinterpret_cast<const int*>(defData) != toCheckStateless)
        throw std::runtime_error("def. data differs for def.data, stateless case");
    if(!context) throw std::runtime_error("no context");

    return reinterpret_cast<hdql_Datum_t>(
            &reinterpret_cast<ItemWithScalarAttr*>(owner)->a);
}

hdql_ScalarAttrInterface statelessScalarAttrIFace {
    .definitionData = reinterpret_cast<hdql_Datum *>(&toCheckStateless),
    .new_dyn_data = nullptr,
    .reset = reset_a_stateless,
    .destroy_dyn_data = nullptr
};

}  // anon ns

// Simple stateless scalar attribute
//
TEST_F(BasicQuery, queryResolvesStatelessScalarOnce) {
    // create simple attribute definition
    hdql_AtomicTypeFeatures typeInfo;
    typeInfo.arithTypeCode = 0x1;  // just to prevent assertions, must be unused
    typeInfo.isReadOnly = 0x1;
    _ad = hdql_attr_def_create_atomic_scalar(&typeInfo
            , &statelessScalarAttrIFace
            , 0x0
            , NULL
            , _context);
    ASSERT_TRUE(_ad);

    // create attribute definition
    // Normally ADs are part of groups (compounds), but compounds are not
    // needed for simple queries
    _q = hdql_query_create(_ad, NULL, _context);
    ASSERT_TRUE(_q);

    {
        ItemWithScalarAttr item;
        item.a = 0xFEED;

        hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
        ASSERT_TRUE(r) << "attribute reset() did not return a value";

        ASSERT_EQ(0xFEED, *reinterpret_cast<int*>(r)) << "value mismatch";

        // check that subsequent attempts to advance the query results in NULL
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
    }

    {
        ItemWithScalarAttr item;
        item.a = 0xBEEF;

        hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
        ASSERT_TRUE(r) << "attribute reset() did not return a value";

        ASSERT_EQ(0xBEEF, *reinterpret_cast<int*>(r)) << "value mismatch";

        // check that subsequent attempts to advance the query results in NULL
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
    }
}

}  // namespace ::hdql::test
}  // namespace hdql

