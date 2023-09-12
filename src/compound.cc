#include "hdql/compound.h"
#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/query.h"
#include "hdql/query-key.h"
#include "hdql/types.h"

#include <unordered_map>
#include <string>
#include <cassert>
#include <cstring>
#include <climits>

namespace hdql {

struct Compound {
    const std::string name;
    const hdql_Compound * parent;

    std::unordered_map<std::string, hdql_AttrDef *> attrsByName;

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
hdql_compound_new(const char * name, struct hdql_Context * ctx) {
    char * bf = reinterpret_cast<char *>(hdql_alloc(ctx, struct hdql_Compound));
    return new (bf) hdql_Compound(name, NULL);
}

extern "C" hdql_Compound *
hdql_virtual_compound_new(const hdql_Compound * parent, struct hdql_Context * ctx) {
    assert(parent);
    assert(!parent->name.empty());
    return new hdql_Compound("", parent);  // TODO: ctx-based allocator
}

extern "C" void
hdql_virtual_compound_destroy(hdql_Compound * vCompound, struct hdql_Context * ctx) {
    for(auto & attrDef : vCompound->attrsByName ) {
        if(hdql_attr_def_is_fwd_query(attrDef.second)) {
            hdql_query_destroy(hdql_attr_def_fwd_query(attrDef.second), ctx);
        }
        hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(attrDef.second));
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

extern "C" int
hdql_compound_add_attr( hdql_Compound * instance
                      , const char * attrName
                      , struct hdql_AttrDef * attrDef
                      ) {
    // basic integrity checks for the added item
    if( (!attrName) || '0' == *attrName ) return -11;  // empty name
    auto ir1 = instance->attrsByName.emplace(attrName, attrDef);
    if(!ir1.second) return -1;
    return 0;
}

extern "C" const hdql_AttrDef *
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
    return it->second;
}

extern "C" void
hdql_compound_destroy(hdql_Compound * compound, hdql_Context_t context) {
    for(auto & attrDef : compound->attrsByName ) {
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(attrDef.second));
    }
    compound->~hdql_Compound();
    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(compound));
}

