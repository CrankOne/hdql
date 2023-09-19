#include "hdql/function.h"
#include "hdql/context.h"
#include "hdql/types.h"

#include <cassert>
#include <unordered_map>
#include <string>

#if 0
//
// Function interface

struct hdql_Func {
    union {
        hdql_Datum_t arithOp;
    } pl;
};

//
// C API for functions

extern "C" int
hdql_func_reserve_keys( struct hdql_Func * fDef
                      , hdql_CollectionKey ** keys
                      , hdql_Context_t ctx
                      ) {
    assert(0);
    //assert(fDef);
    //assert(keys);
    //assert(ctx);
    //*keys = fDef->reserve_keys(ctx);
    //return 0;
}

extern "C" int
hdql_func_call_on_keys( struct hdql_Func * fDef
                      , struct hdql_CollectionKey * keys
                      , hdql_KeyIterationCallback_t callback, void * userdata
                      , size_t cLevel, size_t nKeyAtLevel
                      , hdql_Context_t ctx
                      ) {
    assert(0);
    //return fDef->call_for_each_key(keys, callback, userdata, cLevel, nKeyAtLevel, ctx);
}

extern "C" int
hdql_func_destroy_keys( struct hdql_Func * fDef
                      , struct hdql_CollectionKey * keys
                      , hdql_Context_t ctx
                      ) {
    assert(0);
    //return fDef->destroy_keys(keys, ctx);
}

extern "C" int
hdql_func_copy_keys( struct hdql_Func * fDef
                   , hdql_CollectionKey * dest
                   , const hdql_CollectionKey * src
                   , hdql_Context_t ctx
                   ) {
    assert(0);
    //return fDef->copy_keys(dest, src, ctx);
}
#endif

//                                       ______________________________________
// ____________________________________/ Context-private functions table mgmnt

struct hdql_Functions : public std::unordered_multimap<std::string, void*> {
};

// NOT exposed to public header
extern "C" struct hdql_Functions *
_hdql_functions_create(struct hdql_Context * ctx) {
    //return new hdql_Functions;
}

// NOT exposed to public header
extern "C" void
_hdql_functions_destroy(struct hdql_Context * ctx, struct hdql_Functions * funcDict) {
    //delete funcDict;
}

