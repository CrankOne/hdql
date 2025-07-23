#include "hdql/attr-def-unwind.h"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/types.h"

#include <assert.h>
#include <stdlib.h>

static int
_attr_def_unwind( const hdql_AttrDefStack_t s
                , struct hdql_iAttrDefVisitor * v
                ) {
    const hdql_AttrDef_t topAD = hdql_attr_def_top_attr(s->ad);
    if(hdql_attr_def_is_atomic(topAD)) {
        if(v->atomic_ad)
            return v->atomic_ad(s, v->userdata);
        else
            return 0;
    } else {
        assert(hdql_attr_def_is_compound(topAD));
        if(!v->push_compound_ad) return 0;

        int rc = v->push_compound_ad(s, v->userdata);
        if( 1 == rc ) return 0;  /* skip expansion of compound */

        /* expand compound */
        const struct hdql_Compound * c = hdql_attr_def_compound_type_info(s->ad);
        assert(c);
        size_t na = hdql_compound_get_nattrs_recursive(c);
        const char ** aNames = (const char **) alloca(na*sizeof(char*));
        hdql_compound_get_attr_names_recursive(c, aNames);
        /* reentrant instance to avoid reallocs */
        struct hdql_AttrDefStack nextTier;
        nextTier.prev = s;
        for(size_t i = 0; i < na; ++i) {  /* iterate over attributes */
            nextTier.name = aNames[i];
            nextTier.ad = hdql_compound_get_attr(c, nextTier.name);
            rc = _attr_def_unwind(&nextTier, v);
            if(0 != rc) {
                if(v->pop_compound_ad)
                    v->pop_compound_ad(s, v->userdata);
                return rc;
            }
        }
        if(v->pop_compound_ad)
            v->pop_compound_ad(s, v->userdata);
    }
    return 0;
}

int
hdql_attr_def_unwind( const hdql_AttrDef_t ad
                    , struct hdql_iAttrDefVisitor * v
                    ) {
    struct hdql_AttrDefStack t;
    t.ad = ad;
    t.name = NULL;
    t.prev = NULL;
    return _attr_def_unwind(&t, v);
}

/*
 * -------------
 */

struct hdql_iDataVisitor {
    void * userdata;

    /** Should rely on stack's top to create userdata for `handle_atomic_data()`
     * and return 0 or ignore this attribute returning 1. Other return codes
     * considered as error */
    int (*reserve_atomic_data_handler)(const hdql_AttrDefStack_t cs, void ** handlerUD, void * commonUD);
    /** Handles atomic datum */
    int (*handle_atomic)(hdql_Datum_t datum, void * handlerUD, void * commonUD);

    /** Should rely on stack's top to create userdata for `handle_compound_data()`
     * and return 0 or ignore this attribute returning 1. Other return codes
     * considered as error */
    int (*reserve_compound_data_handler)(const hdql_AttrDefStack_t cs, void ** handlerUD, void * commonUD);
    /** Handles atomic datum */
    int (*handle_compound)(hdql_Datum_t datum, void * handlerUD, void * commonUD);
};

struct hdql_DatumHandler;  /* fwd */
struct hdql_CompoundDatumHandler {
    void * userdata;
    size_t n;
    struct hdql_DatumHandler ** handlers;
};

struct hdql_AtomicDatumHandler {
    void * userdata;
    // ...
};

struct hdql_DatumHandler {
    union {
        struct hdql_CompoundDatumHandler compound;
        struct hdql_AtomicDatumHandler atomic;
    } handler;
    union {
        hdql_Datum_t scalar;
        hdql_It_t collection;
    } dynData;
    // struct {} dynData
    static void (*handle)();  // dereference item and advance states
};

struct hdql_ADIterationState {
    struct hdql_iDataVisitor * v;
    // ...
};

static int
_cdh_atomic_ad(const hdql_AttrDefStack_t cs, void * state_) {
    struct hdql_ADIterationState * state = (struct hdql_ADIterationState *) state_;
    int rc;
    
    /* check if this atomic AD is the subject of iteration. Return if not. */
    void * ud = NULL;
    rc = state->v->reserve_atomic_data_handler(cs, &ud, state->v->userdata);
    if(rc == 1) return 0;  /* this attribute should be ignored -- that's it */
    if(rc != 0) return rc;  /* communicate error code */

    /* allocate new `hdql_DatumHandler`, set its handle to internal function
     * handling atomic attr, allocate its dynamic state, assign */
    struct hdql_DatumHandler * dh
        = (struct hdql_DatumHandler*) malloc(sizeof(struct hdql_DatumHandler));
    dh->handler.atomic.userdata = ud;

    if(hdql_attr_def_is_scalar(cs->ad)) {
        dh->handle = _cdh_handle_atomic_scalar_datum;
    } else {
        dh->handle = _cdh_handle_atomic_collection_datum;
    }

    /* `_cdh_init_dynamic_state() takes into account scalar/collection property
     * of AD, assigns dynamic state accordingly: */
    rc = _cdh_init_dynamic_state(cs->ad, dh);
    if(0 != rc) return rc;   /* communicate error */

    /* TODO: extend state with DH */
    return 0;
}

static int
_cdh_push_compound_ad(const hdql_AttrDefStack_t cs, void * state_) {
    struct hdql_ADIterationState * state = (struct hdql_ADIterationState *) state_;
    int rc;

    /* check if this atomic AD is the subject of iteration. Return if not. */
    void * ud = NULL;
    rc = state->v->reserve_compound_data_handler(cs, &ud, state->v->userdata);
    if(rc == 1) return 1;  /* tell the caller to ignore this AD */
    if(rc != 0) return rc;  /* communicate error code */

    /* allocate new `hdql_DatumHandler`, set its handle to internal function
     * handling compound attr, allocate its dynamic state, assign */
    struct hdql_DatumHandler * dh
        = (struct hdql_DatumHandler*) malloc(sizeof(struct hdql_DatumHandler));
    dh->handler.compound.userdata = ud;
    
    if(hdql_attr_def_is_scalar(cs->ad)) {
        dh->handle = _cdh_handle_compound_scalar_datum;
    } else {
        dh->handle = _cdh_handle_compound_collection_datum;
    }

    /* `_cdh_init_dynamic_state() takes into account scalar/collection property
     * of AD, assigns dynamic state accordingly: */
    rc = _cdh_init_dynamic_state(cs->ad, dh);
    if(0 != rc) return rc;  /* communicate error */

    /* TODO: extend state with DH, push stack */
    return 0;
}

static void
_cdh_pop_compound_ad(const hdql_AttrDefStack_t cs, void * state_) {
    struct hdql_ADIterationState * state = (struct hdql_ADIterationState *) state_;
    /* TODO: finalize extending state with DH, pop stack */
}

