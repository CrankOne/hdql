#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/util/allocator.h"
#include "hdql/util/ht.h"
#include "hdql/compound.h"

#include "hdql/internal-api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Init HT size for the attributes of the compound type */
#ifndef HDQL_HT_COMPOUND_NATTRS
#   define HDQL_HT_COMPOUND_NATTRS 64
#endif

/* Init HT size of the compounds table */
#ifndef HDQL_HT_COMPOUNDS_INDEX_NITEMS
#   define HDQL_HT_COMPOUNDS_INDEX_NITEMS 128
#endif

struct hdql_Compound {
    /* name of the compound (type name) */
    char * name;
    /* pointer to parent compound (for virtual compounds) */
    const struct hdql_Compound *parent;
    /* attributes (hash table) */
    struct hdql_ht *attrsByName;
};

/*
 * Creation / deletion
 */

/* internal API (does not auto-add compound to context's compounds table) */
struct hdql_Compound *
hdql__compound_new(const char * name, struct hdql_Context * context) {
    if(NULL == name || '\0' == *name)
        return NULL;  /* (ordinary) compound must have a name */
    struct hdql_Compound *cmpd = (struct hdql_Compound *) hdql_context_alloc(
            context, sizeof(struct hdql_Compound));
    const size_t nameLen = strlen(name) + 1;
    cmpd->name = (char *) hdql_context_alloc(context, nameLen);
    memcpy(cmpd->name, name, nameLen);
    cmpd->parent = NULL;
    cmpd->attrsByName = hdql_ht_create(hdql__context_get_allocator(context)
            , HDQL_HT_COMPOUND_NATTRS
            , HDQL_MURMUR3_32_DEFAULT_SEED
            );
    return cmpd;
}

struct hdql_Compound *
hdql_compound_create(const char * name, struct hdql_Context * context
        , int *rc
        , void *typeID, size_t typeIDSize
        ) {
    struct hdql_Compound *c = hdql__compound_new(name, context);
    if(!c) {
        *rc = HDQL_ERR_GENERIC;
        return NULL;
    }
    /* register compound */
    *rc = hdql_compounds_add(hdql_context_get_compounds(context)
            , name, c, typeID, typeIDSize);
    switch(*rc) {
        case HDQL_ERR_CODE_OVERRIDDEN:
        case HDQL_ERR_CODE_OK:
            return c;
        default:
            return NULL;
    }
}

/* internal API (does not auto-add v-compound to context's compounds table) */
struct hdql_Compound *
hdql__virtual_compound_new(const struct hdql_Compound *parent, struct hdql_Context * context) {
    /* NOTE: parentless virtual compound is permitted */
    struct hdql_Compound *cmpd = (struct hdql_Compound *) hdql_context_alloc(
            context, sizeof(struct hdql_Compound));
    cmpd->name = NULL;
    cmpd->parent = parent;
    cmpd->attrsByName = hdql_ht_create(hdql__context_get_allocator(context)
            , HDQL_HT_COMPOUND_NATTRS
            , HDQL_MURMUR3_32_DEFAULT_SEED
            );
    return cmpd;
}

struct hdql_Compound *
hdql_virtual_compound_create(const struct hdql_Compound * parent, struct hdql_Context * context) {
    struct hdql_Compound *c = hdql__virtual_compound_new(parent, context);
    if(!c) return NULL;
    int rc = hdql__context_add_virtual_compound(context, c);
    assert(HDQL_ERR_CODE_OK == rc);  /* todo: handle/forward RC on opt build*/
    return c;
}


static int
hdql__destroy_transient_attr_def(const unsigned char *name
        , size_t keyLen
        , void ** value
        , void * context_) {
    struct hdql_AttrDef *ad = (struct hdql_AttrDef *) *value;
    struct hdql_Context *context = (struct hdql_Context *) context_;

    if(hdql_attr_def_is_transient(ad))
        hdql_attr_def_destroy(ad, context);
    /* ^^^ TODO: destroy may fail and we should forward return code upwards */
    *value = NULL;
    return 0;
}

int
hdql_virtual_compound_destroy(struct hdql_Compound *vCompound, struct hdql_Context * context) {
    assert(NULL == vCompound->name);  /* virtual compound must not have a name */
    int rc = hdql_ht_iter( vCompound->attrsByName
            , hdql__destroy_transient_attr_def
            , context
            );
    if(0 != rc) return HDQL_ERR_MEMORY;  /* TODO: elaborate error handling */ 
    if(vCompound->attrsByName)
        hdql_ht_destroy(vCompound->attrsByName);
    hdql_context_free(context, (hdql_Datum_t) vCompound);
    /* ^^^ TODO: check mem failure */
    return HDQL_ERR_CODE_OK;
}


static int
hdql__destroy_attr_def(const unsigned char *name
        , size_t keyLen
        , void ** value
        , void * context_) {
    struct hdql_AttrDef *ad = (struct hdql_AttrDef *) *value;
    struct hdql_Context *context = (struct hdql_Context *) context_;

    hdql_attr_def_destroy(ad, context);
    /* ^^^ TODO: destroy may fail and we should forward return code upwards */
    *value = NULL;
    return 0;
}

int
hdql_compound_destroy(struct hdql_Compound *compound, hdql_Context_t context) {
    assert(NULL != compound->name);  /* (ordinary) compound must have a name */
    int rc = 0;
    if(compound->attrsByName) {
        rc = hdql_ht_iter( compound->attrsByName
            , hdql__destroy_attr_def
            , context
            );
        if(0 != rc) return HDQL_ERR_MEMORY;
        hdql_ht_destroy(compound->attrsByName);
    }
    if(compound->name)
        hdql_context_free(context, (hdql_Datum_t) compound->name);
    hdql_context_free(context, (hdql_Datum_t)(compound));
    return HDQL_ERR_CODE_OK;
}

/*
 * Getters
 */

static int
hdql__print_attr_name(const unsigned char *name
        , size_t keyLen
        , void **value
        , void *outf) {
    (void) keyLen;
    (void) value;
    fputs((const char *) name, (FILE*) outf);
    fputc('\n', (FILE*) outf);
    return 0;
}

void
hdql_compound_print_attr_names(const struct hdql_Compound * c, FILE *dest) {
    hdql_ht_iter(c->attrsByName
            , hdql__print_attr_name
            , dest);
}

const struct hdql_Compound *
hdql_virtual_compound_get_parent(const struct hdql_Compound * self) {
    return self->parent;
}

bool
hdql_compound_is_virtual(const struct hdql_Compound * compound) {
    return NULL != compound->parent;
}


static int
hdql__query_attr_def_is_bound(const unsigned char *name
        , size_t keyLen
        , void ** value
        , void * context_) {
    (void) keyLen;
    (void) name;
    (void) context_;
    struct hdql_AttrDef *ad = (struct hdql_AttrDef *) *value;
    if(hdql_attr_def_is_bound(ad)) return INT_MAX;
    return 0;
}

bool
hdql_virtual_compound_is_bound(const struct hdql_Compound * compound) {
    int rc = hdql_ht_iter(compound->attrsByName
            , hdql__query_attr_def_is_bound
            , NULL);
    return (bool) rc;
}

bool
hdql_compound_is_same(const struct hdql_Compound * compoundA, const struct hdql_Compound * compoundB) {
    /* todo: reserved for further (possible) elaboration */
    return compoundA == compoundB;
}

const struct hdql_AttrDef *
hdql_compound_get_attr(const struct hdql_Compound *instance, const char *name) {
    const struct hdql_AttrDef * r;
    for( r = hdql_ht_s_get(instance->attrsByName, name)
       ; NULL == r
       ; r = hdql_ht_s_get( instance->attrsByName, name)
       ) {
        assert(NULL == r);
        instance = instance->parent;
        if(!instance) return NULL;
    }
    return r;
}

const char *
hdql_compound_get_name(const struct hdql_Compound *instance) {
    return instance->name;
}

size_t
hdql_compound_get_nattrs(const struct hdql_Compound * c) {
    return hdql_ht_size(c->attrsByName);
}


static int
hdql__set_attr_name_ptr(const unsigned char *name
        , size_t keyLen
        , void **value
        , void *dest_) {
    (void) keyLen;
    (void) value;
    const char ***dest = (const char ***) dest_;
    **dest = (const char*) name;
    ++(*dest);
    return 0;
}

void
hdql_compound_get_attr_names(const struct hdql_Compound * c, const char ** dest) {
    hdql_ht_iter(c->attrsByName
            , hdql__set_attr_name_ptr
            , &dest);
}

void
hdql_compound_get_attr_names_recursive(const struct hdql_Compound *c, const char ** dest) {
    assert(c);
    assert(dest);
    do {
        hdql_ht_iter(c->attrsByName
            , hdql__set_attr_name_ptr
            , &dest);
    } while(!!(c = c->parent));
}

char *
hdql_compound_get_full_type_str(
          const struct hdql_Compound * c
        , char * buf, size_t bufSize
        ) {
    assert(c);
    assert(buf);
    assert(bufSize > 0);
    *buf = '\0';
    char * bufEnd = buf + bufSize, * cb;

    #define _M_apps(t) \
    cb = strncat(buf, t, bufSize); \
    if(buf == bufEnd) {return buf;} \
    assert(buf < bufEnd); \
    bufSize -= cb - buf; \
    buf = cb;
    do {
        if(hdql_compound_is_virtual(c)) {
            /* append string with {attrs, list}-> */
            _M_apps("{");
            const size_t nAttrs = hdql_compound_get_nattrs(c);
            const char ** attrNames = (const char **) alloca(sizeof(char*)*nAttrs);
            hdql_compound_get_attr_names(c, attrNames);
            for(size_t i = 0; i < nAttrs; ++i) {
                if(i) {
                    _M_apps(", ")
                }
                _M_apps(attrNames[i]);
            }
            _M_apps("}->");
        } else {
            buf = strncat(buf, c->name, bufSize);
        }
    } while(NULL != (c = c->parent));
    #undef _M_apps

    return buf;
}

size_t
hdql_compound_get_nattrs_recursive(const struct hdql_Compound * c) {
    assert(c);
    size_t nAttrs = 0;
    do {
        nAttrs += hdql_compound_get_nattrs(c);
    } while(!!(c = c->parent));
    return nAttrs;
}

size_t
hdql_compound_for_each_own_attribute(const struct hdql_Compound * C
        , int (*cllb)(const char *, size_t, const struct hdql_AttrDef *, void *)
        , void * userdata) {
    const size_t nAttrsOverall = hdql_compound_get_nattrs(C);
    const char ** names = (const char **) alloca(sizeof(char*)*nAttrsOverall);
    hdql_compound_get_attr_names(C, names);
    size_t nBindingQueries = 0;
    for(size_t nAttr = 0; nAttr < nAttrsOverall; ++nAttr) {
        const struct hdql_AttrDef * attrAD = hdql_compound_get_attr(C, names[nAttr]);
        if(!hdql_attr_def_is_bound(attrAD)) continue;
        ++nBindingQueries;
        if(cllb ) {
            int rc = cllb(names[nAttr], nBindingQueries, attrAD, userdata);
            if(0 != rc) return nBindingQueries;
        }
    }
    return nBindingQueries;
}

/*
 * Modifiers
 */

int
hdql_compound_add_attr( struct hdql_Compound *instance
                      , const char * attrName
                      , struct hdql_AttrDef * attrDef
                      ) {
    /* basic integrity checks for the added item */
    if( (!attrName) || '0' == *attrName ) return HDQL_ERR_BAD_ARGUMENT;  /* ampty name */

    /* probe attribute name first */
    const size_t nameLen = strlen(attrName) + 1;
    size_t nb;
    struct hdql_htEntry *entry = hdql_ht_lookup(instance->attrsByName
            , (const unsigned char *) attrName, nameLen
            , &nb);
    if(entry != NULL) return HDQL_ERR_NAME_COLLISION;  /* name's not uniq */
    /* ok, insert */
    int rc = hdql_ht_ins_cached(instance->attrsByName, (unsigned char *) attrName
            , nameLen, attrDef, entry, nb );
    if(HDQL_HT_RC_INSERTED == rc) return HDQL_ERR_CODE_OK;  /* ok */
    if(HDQL_HT_ERR_MEM == rc) return HDQL_ERR_MEMORY;  /* ht mem failure */
    return HDQL_ERR_GENERIC;  /* ht failed by unknown reason */
}

/*
 * Compound types table
 */

struct hdql_Compounds {
    struct hdql_Compounds *parent;
    struct hdql_ht *byName;
    struct hdql_ht *byTypeID;
};

int
hdql_compounds_add(struct hdql_Compounds *compounds
        , const char *name, struct hdql_Compound *c
        , void *typeID, size_t typeIDSize
        ) {
    assert(compounds);
    /* probe compound by name */
    const size_t nameLen = strlen(name) + 1;
    size_t nb;
    struct hdql_htEntry *entry = hdql_ht_lookup(compounds->byName
            , (const unsigned char *) name, nameLen, &nb);
    if(entry) return HDQL_ERR_NAME_COLLISION;
    /* ok, insert */
    int rc = hdql_ht_ins_cached(compounds->byName, (unsigned char *) name
            , nameLen, c, entry, nb );
    if(HDQL_HT_RC_INSERTED != rc) {
        if(HDQL_HT_ERR_MEM == rc) return HDQL_ERR_MEMORY;  /* ht mem failure */
        return HDQL_ERR_GENERIC;  /* ht failed by unknown reason */
    }
    if(!typeID) return HDQL_ERR_CODE_OK; /* skip type ID insertion */
    /* probe by type ID, possibly overwrite */
    rc = hdql_ht_ins(compounds->byTypeID, typeID, typeIDSize, c);
    switch(rc) {
        case HDQL_HT_RC_INSERTED:
            return HDQL_ERR_CODE_OK;
        case HDQL_HT_ERR_MEM:
            return HDQL_ERR_MEMORY;
        case HDQL_HT_RC_UPDATED:
            return HDQL_ERR_CODE_OVERRIDDEN;
        default:
            return HDQL_ERR_GENERIC;
    };
}

const struct hdql_Compound *
hdql_compounds_get_by_name(const struct hdql_Compounds *compounds
        , const char *name ) {
    const struct hdql_Compound *r;
    for( r = hdql_ht_s_get(compounds->byName, name)
       ; NULL == r && compounds->parent
       ; r = hdql_ht_s_get(compounds->byName, name)) {
        compounds = compounds->parent;
    }
    return r;
}

const struct hdql_Compound *
hdql_compounds_get_by_type_id(const struct hdql_Compounds *compounds
        , const void *typeID, size_t typeIDLen ) {
    const struct hdql_Compound *r;
    for( r = hdql_ht_get(compounds->byTypeID, typeID, typeIDLen)
       ; NULL == r && compounds->parent
       ; r = hdql_ht_get(compounds->byTypeID, typeID, typeIDLen)) {
        compounds = compounds->parent;
    }
    return r;
}

/*
 * Internal API
 */

struct hdql_Compounds *
hdql__compounds_create(struct hdql_Compounds *parent, struct hdql_Context *context) {
    const struct hdql_Allocator *alloc = hdql__context_get_allocator(context);
    struct hdql_Compounds *compounds = alloc->alloc(sizeof(struct hdql_Compounds), alloc->userdata);
    if(!compounds) return NULL;

    compounds->parent = parent;

    compounds->byName = hdql_ht_create(alloc, HDQL_HT_COMPOUNDS_INDEX_NITEMS, HDQL_MURMUR3_32_DEFAULT_SEED);
    if(!compounds->byName) {
        alloc->free(compounds, alloc->userdata);
        return NULL;
    }

    compounds->byTypeID = hdql_ht_create(alloc, HDQL_HT_COMPOUNDS_INDEX_NITEMS, HDQL_MURMUR3_32_DEFAULT_SEED);
    if(!compounds->byTypeID) {
        hdql_ht_destroy(compounds->byName);
        alloc->free(compounds, alloc->userdata);
        return NULL;
    }

    return compounds;
}

static int
hdql__compound_entry_destroy(
        const unsigned char *key, size_t keyLen, void **c_, void *context_) {
    hdql_Context_t context = (hdql_Context_t) context_;
    struct hdql_Compound *c = (struct hdql_Compound *) *c_;
    hdql_compound_destroy(c, context);
    return 0;
}

void
hdql__compounds_destroy(struct hdql_Compounds *c, struct hdql_Context *context) {
    hdql_ht_iter(c->byName, hdql__compound_entry_destroy, context);
    hdql_ht_destroy(c->byTypeID);
    hdql_ht_destroy(c->byName);

    const struct hdql_Allocator *alloc = hdql__context_get_allocator(context);
    alloc->free(c, alloc->userdata);
}

