#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/util/ht.h"
#include "hdql/compound.h"

#include "hdql/internal-api.h"

#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#ifndef HDQL_COMPOUND_HT_NATTRS
#   define HDQL_COMPOUND_HT_NATTRS 64
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

struct hdql_Compound *
hdql_compound_new(const char * name, struct hdql_Context * context) {
    if(NULL == name || '\0' == *name)
        return NULL;  /* (ordinary) compound must have a name */
    struct hdql_Compound *cmpd = (struct hdql_Compound *) hdql_context_alloc(
            context, sizeof(struct hdql_Compound));
    const size_t nameLen = strlen(name) + 1;
    cmpd->name = (char *) hdql_context_alloc(context, nameLen);
    memcpy(cmpd->name, name, nameLen);
    cmpd->parent = NULL;
    cmpd->attrsByName = hdql_ht_create(hdql__context_get_allocator(context)
            , HDQL_COMPOUND_HT_NATTRS
            , HDQL_MURMUR3_32_DEFAULT_SEED
            );
    return cmpd;
}

struct hdql_Compound *
hdql_virtual_compound_new(const struct hdql_Compound *parent, struct hdql_Context * context) {
    /* NOTE: parentless virtual compound is permitted */
    struct hdql_Compound *cmpd = (struct hdql_Compound *) hdql_context_alloc(
            context, sizeof(struct hdql_Compound));
    cmpd->name = NULL;
    cmpd->parent = parent;
    cmpd->attrsByName = hdql_ht_create(hdql__context_get_allocator(context)
            , HDQL_COMPOUND_HT_NATTRS
            , HDQL_MURMUR3_32_DEFAULT_SEED
            );
    return cmpd;
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
    int rc = hdql_ht_iter( compound->attrsByName
            , hdql__destroy_attr_def
            , context
            );
    if(0 != rc) return HDQL_ERR_MEMORY;
    hdql_context_free(context, (hdql_Datum_t)(compound));
    /* ^^^ TODO: check mem failure */
    return HDQL_ERR_CODE_OK;
}

/*
 * Getters
 */

const struct hdql_Compound *
hdql_virtual_compound_get_parent(const struct hdql_Compound * self) {
    return self->parent;
}

bool
hdql_compound_is_virtual(const struct hdql_Compound * compound) {
    return NULL == compound->parent;
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
    return hdql_ht_s_get(instance->attrsByName, name);
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
    const size_t nameLen = strlen(attrName);
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
