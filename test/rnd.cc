#include <gtest/gtest.h>

#include "hdql/util/pcg32.h"

TEST(PCG32, sameSeedSameSequence) {
    hdql_PCG32 a, b;
    hdql_rand_pcg32_seed(&a, 0xdeadbeef, 0);
    hdql_rand_pcg32_seed(&b, 0xdeadbeef, 0);

    for(size_t i = 0; i < 100; ++i)
        EXPECT_EQ(hdql_rand_pcg32_draw(&a), hdql_rand_pcg32_draw(&b));
}

TEST(PCG32, differentSeedsDiffer) {
    hdql_PCG32 a, b;
    hdql_rand_pcg32_seed(&a, 0xdeadbeef, 0);
    hdql_rand_pcg32_seed(&b, 0xfacade,   0);

    bool anyDifferent = false;
    for(size_t i = 0; i < 10; ++i)
        anyDifferent |= hdql_rand_pcg32_draw(&a) != hdql_rand_pcg32_draw(&b);

    EXPECT_TRUE(anyDifferent);
}

TEST(PCG32, boundedResultIsBelowBound) {
    hdql_PCG32 rng;
    hdql_rand_pcg32_seed(&rng, 0xdeadbeef, 0);

    for(uint64_t bound : {1UL, 2UL, 3UL, 10UL, 1000UL, UINT64_C(1) << 63}) {
        for(size_t i = 0; i < 100; ++i)
            EXPECT_LT(hdql_rand_pcg32_uint64_below(&rng, bound), bound);
    }
}

TEST(PCG32, boundOneAlwaysReturnsZero) {
    hdql_PCG32 rng;
    hdql_rand_pcg32_seed(&rng, 0xdeadbeef, 0);

    for(size_t i = 0; i < 10; ++i)
        EXPECT_EQ(0u, hdql_rand_pcg32_uint64_below(&rng, 1));
}

