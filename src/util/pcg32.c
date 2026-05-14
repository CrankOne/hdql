#include "hdql/util/pcg32.h"

void
hdql_rand_pcg32_seed(struct hdql_PCG32 * rng, uint64_t seed, uint64_t seq) {
    rng->state = 0;
    rng->inc = (seq << 1u) | 1u;

    (void) hdql_rand_pcg32_draw(rng);
    rng->state += seed;
    (void) hdql_rand_pcg32_draw(rng);
}

uint32_t
hdql_rand_pcg32_draw(struct hdql_PCG32 * rng) {
    const uint64_t S = rng->state;
    rng->state = S * 6364136223846793005ULL  /*< a special multiplier, recommended for LCG */
               + rng->inc;
    uint32_t x = (uint32_t)(((S >> 18u) ^ S) >> 27u);
    uint32_t r = S >> 59u;
    return (x >> r) | (x << ((-r) & 31));
}

uint64_t
hdql_rand_pcg32_draw_u64(struct hdql_PCG32 * rng) {
    const uint64_t hi = hdql_rand_pcg32_draw(rng);
    const uint64_t lo = hdql_rand_pcg32_draw(rng);
    return (hi << 32) | lo;
}

uint64_t
hdql_rand_pcg32_uint64_below(struct hdql_PCG32 * rng, uint64_t bound) {
    if(bound == 0) return 0;
    /* avoid modulo bias (overhead should be small) */
    uint64_t threshold = -bound % bound;
    for(;;) {
        uint64_t r = hdql_rand_pcg32_draw_u64(rng);
        if(r >= threshold)
            return r % bound;
    }
}

