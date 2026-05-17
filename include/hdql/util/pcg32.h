#ifndef H_HDQL_UTILS_PCG64_H
#define H_HDQL_UTILS_PCG64_H 1

#include "hdql/types.h"

/* PCG64 implementation
 *
 * Used as default random generator.
 * */

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Generator state
 *
 * Exposed for external API to be able to save/recover the state.
 * */
struct hdql_PCG32 {
    uint32_t state, inc;
};

/**\brief Seeds the PCG32 random generator */
HDQL_API void hdql_rand_pcg32_seed(struct hdql_PCG32 * rng, uint64_t seed, uint64_t seq);

/**\brief Draws random 32-bit integer number */
HDQL_API uint32_t hdql_rand_pcg32_draw(struct hdql_PCG32 * rng);

/**\brief Draws 64-bit integer number as concatenation of 32-bit ones */
HDQL_API uint64_t hdql_rand_pcg32_draw_u64(struct hdql_PCG32 * rng);

/**\brief Returns uniformly distributed pseudo-random integer in [0, bound).
 *
 * Uses rejection sampling to avoid modulo bias when reducing 64-bit PRNG
 * output to a bounded range. Expected complexity is O(1) with negligible
 * rejection probability for typical bounds.
 *
 * \p rng    PRNG state.
 * \p bound  Exclusive upper bound. When zero, returns zero.
 *
 * \return Uniform random integer in range [0, bound).
 */
HDQL_API uint64_t hdql_rand_pcg32_uint64_below(struct hdql_PCG32 * rng, uint64_t bound);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_UTILS_PCG64_H */
