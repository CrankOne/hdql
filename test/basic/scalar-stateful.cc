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

// Stateful interface
//

int toCheckStateful = 1337;

// Statefull interface
//

hdql_Datum_t
instantiate_a_stateful( hdql_Datum_t newOwner
                      , const hdql_Datum_t defData
                      , hdql_Context_t context
                      ) {
    if(!newOwner) throw std::runtime_error("no owner");
    if(!defData) throw std::runtime_error("empty def data");
    if(!context) throw std::runtime_error("no context");
    int *dynData = new int;
    *dynData = *reinterpret_cast<int*>(defData);
    return reinterpret_cast<hdql_Datum *>(dynData);
}

hdql_Datum_t 
reset_a_stateful( hdql_Datum_t owner
       , hdql_Datum_t dynData
       , const hdql_Datum_t defData
       , struct hdql_Key * key
       , hdql_Context_t context
       ) {
    if(!owner) throw std::runtime_error("NULL owner");
    if(!defData) throw std::runtime_error("def. data not set for stateful scalar");
    if(defData != reinterpret_cast<hdql_Datum_t>(&toCheckStateful))
        throw std::runtime_error("addr differs for def.data, stateful case");
    if(*reinterpret_cast<int*>(defData) != toCheckStateful)
        throw std::runtime_error("def. data differs for def.data, stateful case");
    if(!context) throw std::runtime_error("no context");
    if(!dynData) throw std::runtime_error("no dyn. data");
    if(*reinterpret_cast<int*>(dynData) != toCheckStateful)
        throw std::runtime_error("dyn. data differs");
    *reinterpret_cast<int*>(dynData) <<= 2;

    return reinterpret_cast<hdql_Datum_t>(
            &reinterpret_cast<ItemWithScalarAttr*>(owner)->a);
}

void 
destroy_a_stateful( hdql_Datum_t dynData
                  , const hdql_Datum_t defData
                  , hdql_Context_t context
                  ) {
    if(!defData) throw std::runtime_error("def. data not set for stateful scalar");
    if(defData != reinterpret_cast<hdql_Datum_t>(&toCheckStateful))
        throw std::runtime_error("addr differs for def.data, stateful case");
    if(*reinterpret_cast<int*>(defData) != toCheckStateful)
        throw std::runtime_error("def. data differs for def.data, stateful case");
    if(!context) throw std::runtime_error("no context");
    if(!dynData) throw std::runtime_error("no dyn. data");
    if(*reinterpret_cast<int*>(dynData) != (toCheckStateful << 2))
        throw std::runtime_error("dyn. data differs");

    delete reinterpret_cast<int*>(dynData);
}

hdql_ScalarAttrInterface statefulScalarAttrIFace {
    .definitionData = reinterpret_cast<hdql_Datum *>(&toCheckStateful),
    .instantiate = instantiate_a_stateful,
    .reset = reset_a_stateful,
    .destroy = destroy_a_stateful
};
}  // anon ns

// Simple stateless scalar attribute
//
TEST_F(BasicAttrDefQuery, queryResolvesStatefulScalarOnce) {
    // create simple attribute definition
    hdql_AtomicTypeFeatures typeInfo;
    typeInfo.arithTypeCode = 0x1;  // just to prevent assertions, must be unused
    typeInfo.isReadOnly = 0x1;
    _ad = hdql_attr_def_create_atomic_scalar(&typeInfo
            , &statefulScalarAttrIFace
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
    toCheckStateful <<= 2;
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


