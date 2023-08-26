#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <unordered_map>
#include <string>
#include <cassert>
#include <cstring>
#include <climits>

#include <iostream>  // XXX

namespace hdql {

extern "C" void
hdql_attribute_definition_init(struct hdql_AttributeDefinition * instance) {
    bzero(instance, sizeof(struct hdql_AttributeDefinition));
}

struct Compound {
    const std::string name;
    const hdql_Compound * parent;

    std::unordered_map<std::string, hdql_AttributeDefinition>
        attrsByName;
    std::unordered_map<hdql_AttrIdx_t, const hdql_AttributeDefinition *>
        attrsByIndex;

    Compound(const char * nm, const hdql_Compound * parent_)
            : name(nm)
            , parent(parent_)
            {}
};  // class Table

}  // namespace hdql

//
// Public C interface for Compound
struct hdql_Compound : public hdql::Compound {
    hdql_Compound(const char * name_, const hdql_Compound * parent_)
            : hdql::Compound(name_, parent_) {}
};

extern "C" hdql_Compound *
hdql_compound_new(const char * name) {
    return new hdql_Compound(name, NULL);
}

extern "C" hdql_Compound *
hdql_virtual_compound_new(const hdql_Compound * parent, struct hdql_Context * ctx) {
    assert(parent);
    assert(!parent->name.empty());
    return new hdql_Compound("", parent);  // TODO: ctx-based allocator
}

extern "C" void
hdql_virtual_compound_destroy(hdql_Compound * vCompound, struct hdql_Context * ctx) {
    for(auto & attrDef : vCompound->attrsByIndex ) {
        if(attrDef.second->isSubQuery) {
            if(!attrDef.second->typeInfo.subQuery) continue;
            hdql_query_destroy(attrDef.second->typeInfo.subQuery, ctx);
        }
    }
    delete vCompound;
}

extern "C" int
hdql_compound_is_virtual(const hdql_Compound * compound) {
    return NULL == compound->parent ? 0x0 : INT_MAX;
}

extern "C" const struct hdql_Compound *
hdql_virtual_compound_get_parent(const struct hdql_Compound * self) {
    return self->parent;
}

static const char gVirtualCompoundName[] = "(virtual)";

extern "C" const char *
hdql_compound_get_name(const struct hdql_Compound * c) {
    if(c->parent) return gVirtualCompoundName;
    return c->name.c_str();
}

#if 0
extern "C" int
hdql_virtual_compound_merge( struct hdql_Compound * a
                           , struct hdql_Compound * b
                           , struct hdql_Compound ** result
                           ) {
    /* consider case when both compounds are virtual */
    if((a->parent && b->parent)) {
        if(a->parent != b->parent) {
            *result = NULL;
            return -101;  /* can't merge two virtual compounds of different parents */
        }
        /* otherwise, just copy 2nd attributes into 1st and return it as a
         * result */
        for(const auto & p : b->attrsByName) {
            int rc = hdql_compound_add_attr( a, p.first.c_str()
                                           , a->attrsByName.size()
                                           , &(p.second) );
            if(rc < 0) return rc;
        }
        *result = a;
        return 0;
    }
    /* if none of the compounds is virtual, */
    if(a->parent) {
        if(a->parent != b)
            return -102;  /* 1st virtual compound has different base than 2nd owner */
        *result = a;
    }
    if(b->parent) {
        if(b->parent != a)
            return -102;  /* 2nd virtual compound has different base than 1st owner */
        *result = b;
    }
    if(NULL == *result) {
        *result = hdql_virtual_compound_new(const hdql_Compound *parent, struct hdql_Context *ctx);
    }
}
#endif

extern "C" int
hdql_compound_add_attr( hdql_Compound * instance
                      , const char * attrName
                      , hdql_AttrIdx_t attrIdx
                      , const struct hdql_AttributeDefinition * attrDef
                      ) {
    // basic integrity checks for the added item
    if( (!attrName) || '0' == *attrName ) return -11;  // empty name
    if( attrDef->isSubQuery ) {
        if(NULL == attrDef->typeInfo.subQuery)
            return -31;
        if(!attrDef->isCollection)
            return -32;  // sub-query not marked as "collection" -- questionable
    } else if( attrDef->isCollection ) {
        const hdql_CollectionAttrInterface & cIFace = attrDef->interface.collection;
        if( NULL == cIFace.create
         || NULL == cIFace.dereference
         || NULL == cIFace.advance
         || NULL == cIFace.reset
         || NULL == cIFace.destroy
         ) return -21;  // bad collection interface definition
        if( NULL != cIFace.get_key ) {
            if( 0x0 == cIFace.keyTypeCode ) {
                return -22;  // coll.attrs with key retrieval must provide valid key type
            }
        }
        if( (NULL == cIFace.compile_selection) != (NULL == cIFace.free_selection) ) {
            return -23;  // coll.attrs with(out) selectors must provide both ctr and dtr (or neither)
        }
    }
    auto ir1 = instance->attrsByName.emplace(attrName, *attrDef);
    if(!ir1.second) return -1;
    auto ir2 = instance->attrsByIndex.emplace(attrIdx, &ir1.first->second);
    if(!ir2.second) {
        instance->attrsByName.erase(ir1.first);
        return -2;
    }
    return 0;
}

extern "C" const hdql_AttributeDefinition *
hdql_compound_get_attr( const hdql_Compound * instance, const char * name ) {
    assert(instance);
    assert(name);
    auto it = instance->attrsByName.find(name);
    if(instance->attrsByName.end() == it) {
        if(instance->parent) {  // current compound is virtual, look in parent
            return hdql_compound_get_attr(instance->parent, name);
        }
        return NULL;
    }
    return &it->second;
}

extern "C" void
hdql_compound_destroy(hdql_Compound * dest) {
    delete dest;
}

#if 1
/*
 * Sub-query as-collection interface
 */

struct SubQueryIterator {
    hdql_Query * subQuery;
    hdql_Datum_t result;
    hdql_CollectionKey * keys;
};

static hdql_It_t
_subquery_collection_interface_create(
          hdql_Datum_t ownerDatum
        , hdql_SelectionArgs_t selection
        , hdql_Context_t ctx
        ) {
    //SubQueryIterator * it = new SubQueryIterator;  // TODO: ctx-based allocator
    SubQueryIterator * it = hdql_alloc(ctx, SubQueryIterator);  // TODO: ctx-based allocator
    it->subQuery = reinterpret_cast<hdql_Query *>(selection);
    assert(it->subQuery);
    it->result = NULL;
    it->keys = NULL;
    int rc = hdql_query_reserve_keys_for(it->subQuery, &(it->keys), ctx);
    assert(rc == 0);
    assert(it->keys);
    return reinterpret_cast<hdql_It_t>(it);
}

static hdql_Datum_t
_subquery_collection_interface_dereference(
          hdql_Datum_t unused
        , hdql_It_t it_
        , hdql_Context_t ctx
        ) {
    #if 1
    //SubQueryIterator * it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    const struct hdql_AttributeDefinition * topAttrDef
        = hdql_query_top_attr(it->subQuery);
    hdql_Datum_t d = it->result;
    if(!topAttrDef->isSubQuery)
        return d;
    assert(0);
    #else
    return reinterpret_cast<SubQueryIterator *>(it_)->result;
    #endif
}

static hdql_It_t
_subquery_collection_interface_advance(
          hdql_Datum_t owner
        , hdql_SelectionArgs_t selection
        , hdql_It_t it_
        , hdql_Context_t ctx
        ) {
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    it->result = hdql_query_get(it->subQuery, owner, it->keys, ctx);
    return it_;
}

static void
_subquery_collection_interface_reset( 
          hdql_Datum_t owner
        , hdql_SelectionArgs_t
        , hdql_It_t it_
        , hdql_Context_t ctx
        ) {
    assert(it_);
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    hdql_query_reset(it->subQuery, owner, ctx);
    it->result = hdql_query_get(it->subQuery, owner, it->keys, ctx);
}

static void
_subquery_collection_interface_destroy(
          hdql_It_t it_
        , hdql_Context_t ctx
        ) {
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    if(it->keys) {
        hdql_query_destroy_keys_for(it->subQuery, it->keys, ctx);
    }
    // NOTE: sub-queries get destroyed within virtual compound dtrs
    // no need to `if(it->subQuery) hdql_query_destroy(it->subQuery, ctx);`
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(it_));
}

static void
_subquery_collection_interface_get_key(
          hdql_Datum_t unused
        , hdql_It_t it_
        , struct hdql_CollectionKey * keyPtr
        , hdql_Context_t ctx
        ) {
    assert(it_);
    assert(keyPtr);
    //auto it = reinterpret_cast<SubQueryIterator *>(it_);
    SubQueryIterator * it = hdql_cast(ctx, SubQueryIterator, it_);
    assert(it->keys);
    assert(keyPtr->code == 0x0);  // key type code for subquery
    assert(keyPtr->datum);
    int rc = hdql_query_copy_keys( it->subQuery
                , reinterpret_cast<hdql_CollectionKey *>(keyPtr->datum), it->keys, ctx);
    assert(0 == rc);
}

const struct hdql_CollectionAttrInterface gSubQueryInterface = {
    .keyTypeCode = 0x0,
    .create = _subquery_collection_interface_create,
    .dereference = _subquery_collection_interface_dereference,
    .advance = _subquery_collection_interface_advance,
    .reset = _subquery_collection_interface_reset,
    .destroy = _subquery_collection_interface_destroy,
    .get_key = _subquery_collection_interface_get_key,
    .compile_selection = NULL,
    .free_selection = NULL,  // TODO: free subquery here?
};
#endif

