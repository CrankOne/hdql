#include "hdql/compound.h"
#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/types.h"

#include <unordered_map>
#include <string>
#include <cassert>
#include <cstring>
#include <climits>

// Compound instances are not used during query evaluation, so no need to
// put them into context-based allocator
//#define HDQL_CONTEXT_BASED_COMPOUNDS_CREATION

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
    #ifdef HDQL_CONTEXT_BASED_COMPOUNDS_CREATION
    char * bf = reinterpret_cast<char *>(hdql_alloc(ctx, struct hdql_Compound));
    return new (bf) hdql_Compound(name, NULL);
    #else
    return new hdql_Compound(name, NULL);
    #endif
}

extern "C" hdql_Compound *
hdql_virtual_compound_new(const hdql_Compound * parent, struct hdql_Context * ctx) {
    assert(parent);
    //assert(!parent->name.empty());
    #ifdef  HDQL_CONTEXT_BASED_COMPOUNDS_CREATION
    char * bf = reinterpret_cast<char *>(hdql_alloc(ctx, struct hdql_Compound));
    return new (bf) hdql_Compound("", parent);
    #else
    return new hdql_Compound("", parent);
    #endif
}

extern "C" void
hdql_virtual_compound_destroy(hdql_Compound * vCompound, struct hdql_Context * ctx) {
    for(auto & attrDef : vCompound->attrsByName ) {
        if(hdql_attr_def_is_fwd_query(attrDef.second))
            hdql_attr_def_destroy(attrDef.second, ctx);
    }
    #ifdef HDQL_CONTEXT_BASED_COMPOUNDS_CREATION
    vCompound->~hdql_Compound();
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(vCompound));
    #else
    delete vCompound;
    #endif
}

extern "C" int
hdql_compound_is_virtual(const hdql_Compound * compound) {
    return NULL == compound->parent ? 0x0 : INT_MAX;
}

extern "C" bool
hdql_compound_is_same(const struct hdql_Compound * compoundA
        , const struct hdql_Compound * compoundB) {
    /* todo: reserved for further (possible) elaboration */
    return compoundA == compoundB;
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

extern "C" char *
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
            buf = strncat(buf, c->name.c_str(), bufSize);
        }
    } while(NULL != (c = c->parent));
    #undef _M_apps

    return buf;
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

extern "C" size_t
hdql_compound_get_nattrs(const struct hdql_Compound * c) {
    assert(c);
    return c->attrsByName.size();
}

extern "C" size_t
hdql_compound_get_nattrs_recursive(const struct hdql_Compound * c) {
    assert(c);
    size_t nAttrs = 0;
    do {
        nAttrs += c->attrsByName.size();
    } while(!!(c = c->parent));
    return nAttrs;
}

extern "C" void
hdql_compound_get_attr_names(const struct hdql_Compound * c, const char ** dest) {
    assert(c);
    assert(dest);
    size_t n = 0;
    for(auto & p : c->attrsByName) {
        dest[n++] = p.first.c_str();
    }
}

extern "C" void
hdql_compound_get_attr_names_recursive(const struct hdql_Compound * c, const char ** dest) {
    assert(c);
    assert(dest);
    size_t n = 0;
    do {
        for(auto & p : c->attrsByName) {
            dest[n++] = p.first.c_str();
        }
    } while(!!(c = c->parent));
}

extern "C" void
hdql_compound_destroy(hdql_Compound * compound, hdql_Context_t context) {
    for(auto & attrDef : compound->attrsByName ) {
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(attrDef.second));
    }
    #ifdef HDQL_CONTEXT_BASED_COMPOUNDS_CREATION
    compound->~hdql_Compound();
    hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(compound));
    #else
    delete compound;
    #endif
}

