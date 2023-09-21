#include "hdql/function.h"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/types.h"
#include "hdql/helpers/functions.hh"

#include <cassert>
#include <unordered_map>
#include <string>
#include <cstring>

#include <iostream>  // XXX

#include <cmath>

struct FuncDef {
    void * userdata;
    hdql_FunctionConstructor_t try_instantiate;
};

struct hdql_Functions : public std::unordered_multimap<std::string, FuncDef> {
    // ...
};

int
hdql_functions_define( struct hdql_Functions * functions
                     , const char * name
                     , hdql_FunctionConstructor_t fdef 
                     , void * userdata
                     ) {
    if(!name) return -1;
    if(*name == '\0') return -1;
    // TODO: other checks for function name validity
    if(!fdef) return -2;
    struct hdql_FuncDef * fDef = (struct hdql_FuncDef *) malloc(sizeof(struct FuncDef));
    bzero(fDef, sizeof(struct FuncDef));
    functions->emplace(name, FuncDef{userdata, fdef});
    return 0;
}

int
hdql_functions_resolve( struct hdql_Functions * funcDict
                      , const char * name
                      , struct hdql_Query ** argsQueries
                      , hdql_AttrDef ** r
                      , hdql_Context_t context
                      ) {
    assert(r);
    auto eqRange = funcDict->equal_range(name);
    if(eqRange.first == eqRange.second) {
        return HDQL_ERR_FUNC_UNKNOWN;
    }
    *r = NULL;
    for(auto it = eqRange.first; it != eqRange.second; ++it) {
        char errBf[128] = "";
        *r = it->second.try_instantiate(argsQueries, it->second.userdata
                , errBf, sizeof(errBf), context);
        if(NULL != *r) return HDQL_ERR_CODE_OK;
        std::cerr << "  " << it->second.try_instantiate
                  << ": " << errBf
                  << std::endl;
    }
    return HDQL_ERR_FUNC_CANT_INSTANTIATE;
}

/*
 * Standard functions library
 */

int
hdql_functions_add_standard_math(struct hdql_Functions * functions) {
    using namespace hdql;
    #define _M_ADD_STD_MATH_FUNC(fname) \
        hdql_functions_define(functions, # fname, hdql::helpers::math_f_construct(:: fname), reinterpret_cast<void*>(&:: fname));
    _M_ADD_STD_MATH_FUNC(sin);
    _M_ADD_STD_MATH_FUNC(cos);
    _M_ADD_STD_MATH_FUNC(sqrt);
    _M_ADD_STD_MATH_FUNC(atan2);
    // ...
    #undef _M_ADD_STD_MATH_FUNC
    return 0;
}


//                                       ______________________________________
// ____________________________________/ Context-private functions table mgmnt

// NOT exposed to public header
extern "C" struct hdql_Functions *
_hdql_functions_create(struct hdql_Context * ctx) {
    return new hdql_Functions;
}

// NOT exposed to public header
extern "C" void
_hdql_functions_destroy(struct hdql_Functions * funcDict, struct hdql_Context * ctx) {
    delete funcDict;
}

