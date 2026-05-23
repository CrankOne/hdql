#include "cross-type-operations.hh"
#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <gtest/gtest.h>
#include <cmath>

namespace hdql {
namespace test {

class TestSimpleNumericExpressions : public TestCrossTypeOperations {
protected:
    const struct hdql_ValueInterface * _rIFace;
    RootItem _rootItem;
public:
    void CompileQuery(const char * expr, bool enableKeys=false) override {
        TestCrossTypeOperations::CompileQuery(expr, enableKeys);
        const struct hdql_AttrDef * ad = hdql_query_top_attr(_query);
        ASSERT_TRUE(hdql_attr_def_is_atomic(ad));
        hdql_ValueTypeCode_t rTC = hdql_attr_def_get_atomic_value_type_code(ad);
        ASSERT_NE(rTC, 0x0);
        struct hdql_ValueTypes * types = hdql_context_get_types(_compounds.context_ptr());
        _rIFace = hdql_types_get_type(types, rTC);
        ASSERT_TRUE(rTC);
    }

    void UseSample1() {
        std::shared_ptr<Item> item0 = std::make_shared<Item>();
        item0->ff   = 0;
        item0->i32f = 0;
        _rootItem.a.push_back(item0);

        std::shared_ptr<Item> item1 = std::make_shared<Item>();
        item1->ff = 1.23;
        item1->i32f = 1;
        _rootItem.a.push_back(item1);

        std::shared_ptr<Item> item2 = std::make_shared<Item>();
        item2->ff = 3.21;
        item2->i32f = 2;
        _rootItem.a.push_back(item2);

        _rootItem.u16f = 42;
    }

    void ExpectFloatResults(const double * values, size_t n) {
        size_t nEl = 0;
        hdql_Datum_t r;
        ResetQuery(reinterpret_cast<hdql_Datum_t>(&_rootItem), r);
        while(r) {
            EXPECT_NEAR( values[nEl]
                       , _rIFace->get_as_float(r)
                       , 1e-6  // this crude is in use since .ff is float (not double)
                       );
            AdvanceQuery(r);
            ++nEl;
        }
        EXPECT_EQ(nEl, n);
    }
};

// Check sum left/right symmetry in common cases:
//  - of 2, 3 constants
//  - of scalar with static const
//  - TODO: of scalar with dynamic const
//  - of collection elements with static const
//  - TODO: of collection element with dynamic value
//  - of collection element with scalar
// Assuming all binary operations are quite similar in handling,
// this would be enough to test this wide class).

TEST_F(TestSimpleNumericExpressions, sumOfTwoStaticConstants) {
    CompileQuery("3 + 5", false);
    UseSample1();
    double expectedResults[] = { 3 + 5 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, sumOfThreeStaticConstants) {
    CompileQuery("3 + 4 + 5", false);
    UseSample1();
    double expectedResults[] = { 3 + 4 + 5 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, sumOfScalarWithStaticConst) {
    CompileQuery(".u16f + 11.35", false);
    UseSample1();
    double expectedResults[] = { ((unsigned short) 42) + ((double) 11.35) };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, sumWorksAgainstLHSCollection_vs_staticConst) {
    CompileQuery(".a.ff + 5", false);
    UseSample1();
    double expectedResults[] = { 0 + 5, 1.23 + 5, 3.21 + 5 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksAgainstLHSCollection_vs_scalar) {
    CompileQuery(".a.ff + .u16f", false);
    UseSample1();
    double expectedResults[] = { 0 + 42, 1.23 + 42, 3.21 + 42 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksWithinLHSCollection_vs_staticConst) {
    CompileQuery(".a{r:=.ff + 5}.r", false);
    UseSample1();
    double expectedResults[] = { 0 + 5, 1.23 + 5, 3.21 + 5 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksWithinBoundLHSCollectionAttribute_vs_staticConst) {
    CompileQuery("{r:=*.a.ff + 5}.r", false);
    UseSample1();
    double expectedResults[] = { 0 + 5, 1.23 + 5, 3.21 + 5 };
    ExpectFloatResults(expectedResults, 3);
}


TEST_F(TestSimpleNumericExpressions, sumWorksAgainstRHSCollection_vs_staticConst) {
    CompileQuery("5 + .a.ff", false);
    UseSample1();
    double expectedResults[] = { 5 + 0, 5 + 1.23, 5 + 3.21 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksAgainstRHSCollection_vs_scalar) {
    CompileQuery(".u16f + .a.ff", false);
    UseSample1();
    double expectedResults[] = { 0 + 42, 1.23 + 42, 3.21 + 42 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksWithinRHSCollection_vs_staticConst) {
    CompileQuery(".a{r:=5 + .ff}.r", false);
    UseSample1();
    double expectedResults[] = { 5 + 0, 5 + 1.23, 5 + 3.21 };
    ExpectFloatResults(expectedResults, 3);
}

TEST_F(TestSimpleNumericExpressions, sumWorksWithinBoundRHSCollectionAttribute_vs_staticConst) {
    CompileQuery("{r:=*5 + .a.ff}.r", false);
    UseSample1();
    double expectedResults[] = { 0 + 5, 1.23 + 5, 3.21 + 5 };
    ExpectFloatResults(expectedResults, 3);
}

// Operator priorities
//

TEST_F(TestSimpleNumericExpressions, priority_prodOverSum) {
    CompileQuery("2 + 3*4", false);
    UseSample1();
    double expectedResults[] = { 2 + 3*4 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_prodOverSub) {
    CompileQuery("20 - 3*4", false);
    UseSample1();
    double expectedResults[] = { 20 - 3*4 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_divOverSum) {
    CompileQuery("2 + 12/3", false);
    UseSample1();
    double expectedResults[] = { 2 + 12./3 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_modOverSum) {
    CompileQuery("2 + 13%5", false);
    UseSample1();
    double expectedResults[] = { 2 + 13%5 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_sumOverShift) {
    CompileQuery("1 << 2 + 1", false);
    UseSample1();
    double expectedResults[] = { 1 << (2 + 1) };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_shiftOverRelational) {
    CompileQuery("1 << 3 > 4", false);
    UseSample1();
    double expectedResults[] = { (1 << 3) > 4 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_relationalOverEquality) {
    CompileQuery("1 < 2 == true", false);
    UseSample1();
    double expectedResults[] = { (1 < 2) == 1 };
    ExpectFloatResults(expectedResults, 1);
}

// This won't work since we do not implicitly cast bool to int.
//TEST_F(TestSimpleNumericExpressions, priority_equalityOverBitwiseAnd) {
//    CompileQuery("6 & 3 == 2", false);
//    UseSample1();
//    double expectedResults[] = { 6 & (3 == 2) };
//    ExpectFloatResults(expectedResults, 1);
//}

TEST_F(TestSimpleNumericExpressions, priority_bitwiseAndOverBitwiseXor) {
    CompileQuery("6 ^ 3 & 2", false);
    UseSample1();
    double expectedResults[] = { 6 ^ (3 & 2) };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_bitwiseXorOverBitwiseOr) {
    CompileQuery("4 | 6 ^ 3", false);
    UseSample1();
    double expectedResults[] = { 4 | (6 ^ 3) };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_bitwiseOrOverLogicalAnd) {
    CompileQuery("0 && 4 | 1", false);
    UseSample1();
    double expectedResults[] = { 0 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_logicalAndOverLogicalOr) {
    CompileQuery("1 || 0 && 0", false);
    UseSample1();
    double expectedResults[] = { 1 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_unaryMinusOverProduct) {
    CompileQuery("-2*3", false);
    UseSample1();
    double expectedResults[] = { (-2)*3 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_logicalNotOverEquality) {
    CompileQuery("!0 == 1", false);
    UseSample1();
    double expectedResults[] = { (!0) == 1 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_bitwiseNotOverBitwiseAnd) {
    CompileQuery("~0 & 7", false);
    UseSample1();
    double expectedResults[] = { (~0) & 7 };
    ExpectFloatResults(expectedResults, 1);
}

TEST_F(TestSimpleNumericExpressions, priority_parenthesesOverride) {
    CompileQuery("(2 + 3)*4", false);
    UseSample1();
    double expectedResults[] = { (2 + 3)*4 };
    ExpectFloatResults(expectedResults, 1);
}

//TEST_F(TestSimpleNumericExpressions, arithmeticPriority) {
//    CompileQuery("-3*4 + 5%3 << 2 & 7 == 4 || !0 && 6/3 + 1 > 2", false);
//    UseSample1();
//    double expectedResults[] = { -3*4 + 5%3 << 2 & 7 == 4 || !0 && 6/3 + 1 > 2 };
//    ExpectFloatResults(expectedResults, 1);
//}

}  // namespace ::hdql::test
}  // namespace hdql


