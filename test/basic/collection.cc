
// Test very basic query iteration of collection attribute definition.
// Attribute definitions used in these queries are created just in the code
// and not provided by any compound definitions.

#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include "basic-query.hh"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

namespace hdql {
namespace test {

namespace {
struct ItemWithCollectionAttr {
    int ints[5];
};
int defData2Check = 1337;
typedef int *It;

hdql_It_t
new_iterator( hdql_Datum_t newOwner
            , const hdql_Datum_t defData
            , hdql_Context_t context
            ) {
    if(!newOwner) throw std::runtime_error("no owner");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    It *it = new It;
    return reinterpret_cast<hdql_It_t>(it);
}

hdql_Datum_t
advance_and_get(hdql_It_t it_, const struct hdql_Datum *defData
        , struct hdql_Key *key, struct hdql_Context *context) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    It *it = reinterpret_cast<It*>(it_);
    if(**it != -1) {
        ++(*it);
        if(**it != -1) {
            return reinterpret_cast<hdql_Datum_t>(*it);
        } else
            return NULL;
    } else
        return NULL;
}

hdql_Datum_t 
reset_iterator( hdql_It_t it_
        , hdql_Datum_t owner
        , const struct hdql_Datum *defData
        , hdql_SelectionArgs_t
        , struct hdql_Key *key
        , hdql_Context_t context
        ) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    It *it = reinterpret_cast<It*>(it_);
    *it = reinterpret_cast<ItemWithCollectionAttr*>(owner)->ints;
    if(**it != -1)
        return reinterpret_cast<hdql_Datum_t>(*it);
    else
        return NULL;
}

void 
destroy_iterator( hdql_It_t it_
                , const struct hdql_Datum *defData
                , hdql_Context_t context
                ) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    It *it = reinterpret_cast<It*>(it_);

    delete it;
}

hdql_CollectionAttrInterface collectionAttrIFaceNoSelection {
    .definitionData = reinterpret_cast<hdql_Datum *>(&defData2Check),
    .new_iterator = new_iterator,
    .yield = advance_and_get,
    .reset_iterator = reset_iterator,
    .destroy_iterator = destroy_iterator,
    .compile_selection = NULL,
    .free_selection = NULL
};
}  // anon ns

// No selection collection tests 
//
class BasicQueryCollection : public BasicQuery {
public:
    void SetUp() override {
        BasicQuery::SetUp();
        // create simple attribute definition
        hdql_AtomicTypeFeatures typeInfo;
        typeInfo.arithTypeCode = 0x1;  // just to prevent assertions, must be unused
        typeInfo.isReadOnly = 0x1;
        _ad = hdql_attr_def_create_atomic_collection(&typeInfo
                , &collectionAttrIFaceNoSelection
                , 0x0
                , NULL
                , _context);
        ASSERT_TRUE(_ad);

        // create attribute definition
        // Normally ADs are part of groups (compounds), but compounds are not
        // needed for simple queries
        _q = hdql_query_create(_ad, NULL, _context);
        ASSERT_TRUE(_q);
    }
};

TEST_F(BasicQueryCollection, queryResolvesCollectionElements) {
    ItemWithCollectionAttr item = {.ints = {1, 2, -1}};

    hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
    ASSERT_TRUE(r) << "attribute reset() did not return a value";
    ASSERT_EQ(1, *reinterpret_cast<int*>(r)) << "value mismatch, 1st el";

    r = hdql_query_get(_q, NULL, _context);
    ASSERT_TRUE(r);
    ASSERT_EQ(2, *reinterpret_cast<int*>(r)) << "value mismatch, 2nd el";

    // check that subsequent attempts to advance the query results in NULL
    r = hdql_query_get(_q, NULL, _context);
    ASSERT_FALSE(r) << " extra value: " << *reinterpret_cast<int*>(r);
    ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
}

TEST_F(BasicQueryCollection, queryResolvesSingleCollectionElement) {
    ItemWithCollectionAttr item = {.ints = {144, -1}};

    hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
    ASSERT_TRUE(r) << "attribute reset() did not return a value";
    ASSERT_EQ(144, *reinterpret_cast<int*>(r)) << "value mismatch, 1st el";

    // check that subsequent attempts to advance the query results in NULL
    r = hdql_query_get(_q, NULL, _context);
    ASSERT_FALSE(r) << " extra value: " << *reinterpret_cast<int*>(r);
    ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
}

TEST_F(BasicQueryCollection, queryResolvesNoDataInEmptyCollection) {
    ItemWithCollectionAttr item = {.ints = {-1}};

    hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
    ASSERT_FALSE(r) << "attribute reset() did not return a value";

    // check that subsequent attempts to advance the query results in NULL
    ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
    ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
}

// Selected collection tests
//

namespace {

struct Selection {int a, b;};

struct SelIt {
    ItemWithCollectionAttr * item;
    int nEl;
    Selection * sel;
};

hdql_SelectionArgs_t
compile_selection( const char * expr
                 , const hdql_Datum_t definitionData
                 , hdql_Context_t context ) {
    if(!expr) throw std::runtime_error("no sel expr");
    if(!definitionData) throw std::runtime_error("no def. data");
    if(!context) throw std::runtime_error("no context");

    Selection *sel = new Selection;
    sel->a = expr[0] - '0';
    sel->b = expr[2] - '0';
    return reinterpret_cast<hdql_SelectionArgs_t>(sel);
}

void 
free_selection( const hdql_Datum_t definitionData
              , hdql_SelectionArgs_t sel
              , hdql_Context_t context
              ) {
    if(!sel) throw std::runtime_error("no selection");
    if(!definitionData) throw std::runtime_error("no def. data");
    if(!context) throw std::runtime_error("no context");

    delete reinterpret_cast<Selection*>(sel);
}

hdql_It_t
new_iterator_s( hdql_Datum_t newOwner
            , const hdql_Datum_t defData
            , hdql_Context_t context
            ) {
    if(!newOwner) throw std::runtime_error("no owner");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    SelIt *it = new SelIt;
    return reinterpret_cast<hdql_It_t>(it);
}

hdql_Datum_t 
reset_iterator_s( hdql_It_t it_
        , hdql_Datum_t owner
        , const struct hdql_Datum *defData
        , hdql_SelectionArgs_t sel_
        , struct hdql_Key *key
        , hdql_Context_t context
        ) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");
    if(key) {
        if(!hdql_key_is_datum(key)) throw std::runtime_error("key isn't a datum");
        if(123 != hdql_key_datum_get_type_code(key)) throw std::runtime_error("bad key type code");
    }

    SelIt *it = reinterpret_cast<SelIt*>(it_);
    it->sel = reinterpret_cast<Selection*>(sel_);
    it->item = reinterpret_cast<ItemWithCollectionAttr*>(owner);
    it->nEl = 0;
    while(it->item->ints[it->nEl] != -1) {
        if(!it->sel) {
            if(key) *reinterpret_cast<uint16_t*>(hdql_key_datum_get(key)) = it->nEl;
            return reinterpret_cast<hdql_Datum_t>(it->item->ints + it->nEl);
        } else {
            if(it->nEl < it->sel->a || it->nEl >= it->sel->b) {
                ++(it->nEl);
                continue;
            }
            if(key) *reinterpret_cast<uint16_t*>(hdql_key_datum_get(key)) = it->nEl;
            return reinterpret_cast<hdql_Datum_t>(it->item->ints + it->nEl);
        }
    }
    return NULL;
}

hdql_Datum_t
advance_and_get_s( hdql_It_t it_
        , const struct hdql_Datum *defData
        , struct hdql_Key *key
        , struct hdql_Context *context
        ) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");
    if(key) {
        if(!hdql_key_is_datum(key)) throw std::runtime_error("key isn't a datum");
        if(123 != hdql_key_datum_get_type_code(key)) throw std::runtime_error("bad key type code");
    }

    SelIt *it = reinterpret_cast<SelIt*>(it_);
    while(it->item->ints[it->nEl] != -1) {
        ++(it->nEl);
        if(it->item->ints[it->nEl] == -1) break;
        if(!it->sel) {
            if(key) *reinterpret_cast<uint16_t*>(hdql_key_datum_get(key)) = it->nEl;
            return reinterpret_cast<hdql_Datum_t>(it->item->ints + it->nEl);
        } else {
            if(it->nEl < it->sel->a || it->nEl >= it->sel->b) {
                continue;
            }
            if(key) *reinterpret_cast<uint16_t*>(hdql_key_datum_get(key)) = it->nEl;
            return reinterpret_cast<hdql_Datum_t>(it->item->ints + it->nEl);
        }
    }
    return NULL;
}

void 
destroy_iterator_s( hdql_It_t it_
                , const struct hdql_Datum *defData
                , hdql_Context_t context
                ) {
    if(!it_) throw std::runtime_error("NULL iterator");
    if(!defData) throw std::runtime_error("empty def data");
    if(defData != reinterpret_cast<hdql_Datum_t>(&defData2Check))
        throw std::runtime_error("defData mismatch");
    if(!context) throw std::runtime_error("no context");

    SelIt *it = reinterpret_cast<SelIt*>(it_);

    delete it;
}

hdql_CollectionAttrInterface collectionAttrIFaceSelection {
    .definitionData = reinterpret_cast<hdql_Datum *>(&defData2Check),
    .new_iterator = new_iterator_s,
    .yield = advance_and_get_s,
    .reset_iterator = reset_iterator_s,
    .destroy_iterator = destroy_iterator_s,
    .compile_selection = compile_selection,
    .free_selection = free_selection
};

int
reserve_keys( struct hdql_Key * key
            , const hdql_Datum_t defData
            , hdql_Context_t context) {
    if(!hdql_key_is_empty(key)) throw std::runtime_error("non-empty key for reserve");
    hdql_Datum_t keyDatum = hdql_context_alloc(context, sizeof(uint16_t));
    hdql_key_set_datum(key, 123, keyDatum);
    return HDQL_ERR_CODE_OK;
}

}  // anon ns


class BasicQuerySelectedCollection : public BasicQuery {
public:
    void SetUp() override {
        BasicQuery::SetUp();
        // create simple attribute definition
        hdql_AtomicTypeFeatures typeInfo;
        typeInfo.arithTypeCode = 0x1;  // just to prevent assertions, must be unused
        typeInfo.isReadOnly = 0x1;
        _ad = hdql_attr_def_create_atomic_collection(&typeInfo
                , &collectionAttrIFaceSelection
                , 0x0
                , reserve_keys
                , _context);
        ASSERT_TRUE(_ad);
    }

    void TestSelection( const int *arr
                      , const char *selexpr
                      , const int *expected
                      , const uint16_t *expectedKeys
                      ) {
        ItemWithCollectionAttr item;
        for(const int *v = arr; ; ++v) {
            item.ints[v - arr] = *v;
            if(*v == -1) break;
        }
        hdql_SelectionArgs_t sel = compile_selection(selexpr
                , reinterpret_cast<hdql_Datum_t>(&defData2Check), _context);
        _q = hdql_query_create(_ad, sel, _context);
        ASSERT_TRUE(_q);

        hdql_Key * key = NULL, *keyEl = NULL;
        if(expectedKeys) {
            key = hdql_key_new(_context);
            hdql_key_reserve_for_query(_q, key, _context);
            ASSERT_TRUE(hdql_key_is_list(key));
            keyEl = hdql_key_get_list_item(key, 0);
            ASSERT_EQ(123, hdql_key_datum_get_type_code(keyEl));
        }

        hdql_Datum_t r;
        const int *expVal = expected;
        for( r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), key, _context)
           ; -1 != *expVal
           ; ++expVal, r = hdql_query_get(_q, key, _context)) {
            size_t nVal = expected - expVal;
            ASSERT_TRUE(r) << "attribute reset() did not return a value (#" << nVal << ")";
            ASSERT_EQ(*expVal, *reinterpret_cast<int*>(r)) << "value mismatch at #" << nVal;
            if(expectedKeys) {
                EXPECT_EQ(*expectedKeys, *reinterpret_cast<uint16_t*>(hdql_key_datum_get(keyEl)));
            }
            if(expectedKeys) ++expectedKeys;
        }
        EXPECT_EQ(*expVal, -1) << "filtered sequence incomplete";
        ASSERT_FALSE(r) << "query returns values the end";

        // check that subsequent attempts to advance the query results in NULL
        r = hdql_query_get(_q, NULL, _context);
        ASSERT_FALSE(r) << " extra value: " << *reinterpret_cast<int*>(r);
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));

        // for safety, try to advance once again -- after collection depleted,
        // these calls should become idempotent
        r = hdql_query_get(_q, NULL, _context);
        ASSERT_FALSE(r) << " extra value (2): " << *reinterpret_cast<int*>(r);
        ASSERT_FALSE(hdql_query_get(_q, NULL, _context));

        if(expectedKeys)
            hdql_key_destroy(key, _context);
    }
};

TEST_F(BasicQuerySelectedCollection, queryResolvesCollectionElementsNoSelection) {
    ItemWithCollectionAttr item = {.ints = {123, 345, -1}};

    // create attribute definition, no selection, actually (but iface
    // supports)
    _q = hdql_query_create(_ad, NULL, _context);
    ASSERT_TRUE(_q);

    hdql_Datum_t r = hdql_query_reset(_q, reinterpret_cast<hdql_Datum_t>(&item), NULL, _context);
    ASSERT_TRUE(r) << "attribute reset() did not return a value";
    ASSERT_EQ(123, *reinterpret_cast<int*>(r)) << "value mismatch, 1st el";

    r = hdql_query_get(_q, NULL, _context);
    ASSERT_TRUE(r);
    ASSERT_EQ(345, *reinterpret_cast<int*>(r)) << "value mismatch, 2nd el";

    // check that subsequent attempts to advance the query results in NULL
    r = hdql_query_get(_q, NULL, _context);
    ASSERT_FALSE(r) << " extra value: " << *reinterpret_cast<int*>(r);
    ASSERT_FALSE(hdql_query_get(_q, NULL, _context));
}

TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsMiddle) {
    int values[] = {123, 235, 345, 456, -1};
    int expected[] = {235, 345, -1};
    TestSelection(values, "1-3", expected, NULL);
}

TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsMiddleWithKey) {
    int values[] = {123, 235, 345, 456, -1};
    int expected[] = {235, 345, -1};
    uint16_t keys[] = {1, 2};
    TestSelection(values, "1-3", expected, keys);
}


TEST_F(BasicQuerySelectedCollection, queryResolvesNoSelectedCollectionElementsNoMatch) {
    int values[] = {123, 235, 345, 456, -1};
    int expected[] = {-1};
    TestSelection(values, "4-9", expected, NULL);
}


TEST_F(BasicQuerySelectedCollection, queryResolvesNoSelectedCollectionElementsEmptyCollection) {
    int values[] = {-1};
    int expected[] = {-1};
    TestSelection(values, "0-9", expected, NULL);
}


TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsLeftIntersection) {
    int values[] = {12, 14, 26, 40, -1};
    int expected[] = {12, 14, -1};
    TestSelection(values, "0-2", expected, NULL);
}

TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsLeftIntersectionWithKey) {
    int values[] = {12, 14, 26, 40, -1};
    int expected[] = {12, 14, -1};
    uint16_t keys[] = {0, 1};
    TestSelection(values, "0-2", expected, keys);
}


TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsRightIntersection) {
    int values[] = {12, 14, 26, 40, -1};
    int expected[] = {26, 40, -1};
    TestSelection(values, "2-9", expected, NULL);
}

TEST_F(BasicQuerySelectedCollection, queryResolvesSelectedCollectionElementsRightIntersectionWithKey) {
    int values[] = {12, 14, 26, 40, -1};
    int expected[] = {26, 40, -1};
    uint16_t keys[] = {2, 3};
    TestSelection(values, "2-9", expected, keys);
}

}  // namespace ::hdql::test
}  // namespace hdql

