#ifndef H_HDQL_RANDOM_H
#define H_HDQL_RANDOM_H 1

#include "hdql/types.h"

struct hdql_RandGen;

/**\brief Sets seed of the random generator
 *
 * Use \p seq to select random streams.
 */
void hdql_rand_seed(struct hdql_RandGen * rng, uint64_t seed, uint64_t seq);

/**\brief Returns uniformly distributed pseudo-random integer in [0, bound).*/
bool hdql_randgen_below(struct hdql_RandGen *, uint64_t bound);

#endif  /* H_HDQL_RANDOM_H */
