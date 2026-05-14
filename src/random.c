#include "hdql/random.h"
#include "hdql/context.h"
#include "hdql/types.h"
#include "hdql/util/pcg32.h"

#include <assert.h>

struct hdql_RandGen {
    struct hdql_RandGen * parent;

    /* TODO: subject of future changes */
    struct hdql_PCG32 * pcg32gen;
};

void
hdql_rand_seed(struct hdql_RandGen * rg, uint64_t seed, uint64_t seq) {
    if(rg->parent) return hdql_rand_seed(rg->parent, seed, seq);
    assert(rg->pcg32gen);
    hdql_rand_pcg32_seed(rg->pcg32gen, seed, seq);  /* <- TODO: subject of future change */
}

bool
hdql_randgen_below(struct hdql_RandGen *rg, uint64_t v) {
    if(rg->parent) return hdql_randgen_below(rg->parent, v);
    /* TODO: subject of future change */
    assert(rg->pcg32gen);
    return hdql_rand_pcg32_uint64_below(rg->pcg32gen, v);
}

/* Called by context constructor to instantiate the generator (should not be
 * exposed publicly) */
struct hdql_RandGen *
_hdql_randgen_create(struct hdql_RandGen *rgParent, struct hdql_Context *ctx) {
    struct hdql_RandGen * rg = (struct hdql_RandGen *)
                hdql_context_alloc(ctx, sizeof(struct hdql_RandGen));
    if(!rg) return NULL;
    if(!!(rg->parent = rgParent)) {
        rg->pcg32gen = NULL;  /* <- TODO: subject of future change */
        /* parent is specify; current shall just forward to parent */
        return rg;
    }
    /* no parent generator -- allocate new.
     * TODO: should be possible to specify other random generators and random
     *       engines in future. Desired API is not clear so far. */
    rg->pcg32gen = (struct hdql_PCG32 *) hdql_context_alloc(ctx, sizeof(struct hdql_PCG32));
    return rg;
}

/* Called by context destructor to destroy the generator (should not be
 * exposed publicly) */
void
_hdql_randgen_destroy(struct hdql_RandGen *rg, struct hdql_Context *ctx) {
    hdql_context_free(ctx, (hdql_Datum_t) rg->pcg32gen);  /* <- TODO: subject of future change */
    hdql_context_free(ctx, (hdql_Datum_t) rg);
}

