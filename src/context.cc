#include "hdql/context.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/function.h"
#include "hdql/operations.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <list>
#include <string>
#include <cstdarg>
#include <unordered_map>

#include <iostream>  // XXX

// These functions are not defined in public headers, but still implemented

extern "C" struct hdql_ValueTypes * _hdql_value_types_table_create(hdql_Context_t);
extern "C" void _hdql_value_types_table_destroy(struct hdql_ValueTypes *);

extern "C" struct hdql_Operations * _hdql_operations_create(struct hdql_Context *);
extern "C" void _hdql_operations_destroy(struct hdql_Operations *, struct hdql_Context *);

extern "C" struct hdql_Functions * _hdql_functions_create(struct hdql_Context *);
extern "C" void _hdql_functions_destroy(struct hdql_Functions *, struct hdql_Context *);

extern "C" struct hdql_Converters * _hdql_converters_create(struct hdql_Context *);
extern "C" void _hdql_converters_destroy(struct hdql_Converters *, struct hdql_Context *);

//extern "C" struct hdql_Functions * _hdql_functions_create(struct hdql_Context *);
//extern "C" void _hdql_functions_destroy(struct hdql_Context *, struct hdql_Functions *);

struct VariadicDatumInfo {
    uint32_t nUsedBytes, nAllocatedBytes;
};

struct hdql_Context {
    #ifdef HDQL_TYPES_DEBUG
    std::unordered_map<hdql_Datum_t, std::string> typesByPtr;
    #endif

    struct hdql_ValueTypes * valueTypes;
    struct hdql_Operations * operations;
    struct hdql_Functions  * functions;
    struct hdql_Converters * converters;
    // temporary (virtual, usually sub-queries) attribute definitions that
    // meant to compose "virtual compound" types. Pointers should remain valid
    // during whole context lifecycle
    // TODO: 
    //std::list<hdql_AttrDef> virtualCompoundAttributes;

    std::list<hdql_Compound *> virtualCompounds;

    std::list<std::pair<hdql_Err_t, std::string>> errors;

    std::unordered_map<hdql_Datum_t, VariadicDatumInfo> variadicDataSizes;
};

extern "C" hdql_Context_t
hdql_context_create() {
    // ...
    auto ctx = new hdql_Context;
    ctx->valueTypes = _hdql_value_types_table_create(ctx);
    ctx->converters = _hdql_converters_create(ctx);
    ctx->operations = _hdql_operations_create(ctx);
    ctx->functions  = _hdql_functions_create(ctx);
    // ...
    return ctx;
}

extern "C" struct hdql_ValueTypes *
hdql_context_get_types( hdql_Context_t ctx ) {
    return ctx->valueTypes;
}

extern "C" struct hdql_Operations *
hdql_context_get_operations( hdql_Context_t ctx ) {
    return ctx->operations;
}

struct hdql_Functions *
hdql_context_get_functions( hdql_Context_t ctx ) {
    return ctx->functions;
}

extern "C" struct hdql_Converters *
hdql_context_get_conversions(hdql_Context_t ctx) {
    return ctx->converters;
}

extern "C" hdql_Datum_t
hdql_context_alloc( hdql_Context_t ctx
                  , size_t len
                  ) {
    // TODO: page-aligned/pool allocation
    return reinterpret_cast<hdql_Datum_t>(malloc(len));
}


extern "C" hdql_Datum_t
hdql_context_variadic_datum_alloc( hdql_Context_t context
                                 , uint32_t usedSize
                                 , uint32_t preallocSize
                                 ) {
    assert(context);
    if(0 == preallocSize) {
        hdql_context_err_push(context, HDQL_ERR_MEMORY
                    , "Bad size for new variadic data block: %ub"
                    , preallocSize );
        return NULL;
    }
    hdql_Datum_t newBlock = reinterpret_cast<hdql_Datum_t>(malloc(preallocSize));
    if(NULL == newBlock) {
        hdql_context_err_push(context, HDQL_ERR_MEMORY
                    , "Failed to allocate new variadic data block of size %ub (to use %ub)"
                    , preallocSize
                    , usedSize
                    );
        return NULL;
    }
    auto ir = context->variadicDataSizes.emplace(newBlock
            , VariadicDatumInfo{.nUsedBytes=usedSize, .nAllocatedBytes=preallocSize});
    if(!ir.second) {
        hdql_context_err_push(context, HDQL_ERR_MEMORY
                    , "Failed to emplace new allocated variadic data"
                      " block %p of size %ub (to use %ub)"
                    , newBlock
                    , preallocSize
                    , usedSize
                    );
        free(newBlock);
        return NULL;
    }
    return ir.first->first;
}

extern "C" uint32_t
hdql_context_variadic_datum_size(hdql_Context_t context, hdql_Datum_t datumPtr) {
    auto it = context->variadicDataSizes.find(datumPtr);
    if(context->variadicDataSizes.end() == it) return UINT32_MAX;
    return it->second.nUsedBytes;
}

hdql_Datum_t
hdql_context_variadic_datum_realloc(hdql_Context_t context, hdql_Datum_t datum, uint32_t size) {
    assert(context);
    assert(datum);
    assert(size);
    auto it = context->variadicDataSizes.find(datum);
    assert(context->variadicDataSizes.end() != it);
    if(it->second.nAllocatedBytes >= size) {
        it->second.nUsedBytes = size;
        return it->first;
    } else {
        void * newData = realloc(datum, size);
        if(NULL == newData) {
            hdql_context_err_push(context, HDQL_ERR_MEMORY
                        , "Failed to reallocate variadic data block of size %ub"
                        , size );
            return NULL;
        }
        if(it->first == newData) {
            it->second.nAllocatedBytes = size;
            it->second.nUsedBytes = size;
            return it->first;
        } else {
            context->variadicDataSizes.erase(it);
            auto ir = context->variadicDataSizes.emplace(
                      reinterpret_cast<hdql_Datum_t>(newData)
                    , VariadicDatumInfo{.nUsedBytes=size, .nAllocatedBytes=size});
            if(!ir.second) {
                hdql_context_err_push(context, HDQL_ERR_MEMORY
                        , "Failed to emplace new variadic data ptr %p", newData);
                free(newData);
                return NULL;
            }
            return ir.first->first;
        }
    }
}

extern "C" void
hdql_context_variadic_datum_free(hdql_Context_t context, hdql_Datum_t datumPtr) {
    auto it = context->variadicDataSizes.find(datumPtr);
    if(context->variadicDataSizes.end() == it) return;
    free(datumPtr);
    context->variadicDataSizes.erase(it);
}


#ifdef HDQL_TYPES_DEBUG
hdql_Datum_t
hdql_context_alloc_typed( hdql_Context_t ctx
                        , size_t size
                        , const char * typeName
                        ) {
    // TODO: strip off `const' qualifier
    assert(size > 0);
    hdql_Datum_t ptr = hdql_context_alloc(ctx, size);
    ctx->typesByPtr.emplace(ptr, typeName);
    return ptr;
}

hdql_Datum_t
hdql_context_check_type( hdql_Context_t ctx
                       , hdql_Datum_t ptr
                       , const char * typeName
                       ) {
    char errbf[128];
    auto it = ctx->typesByPtr.find(ptr);
    if(ctx->typesByPtr.end() == it) {
        snprintf(errbf, sizeof(errbf), "Pointer %p is not allocated by HDQL"
                " context %p", ptr, ctx );
        throw std::runtime_error(errbf);
    }
    // TODO: strip off `const' qualifier
    if(it->second != typeName) {
        snprintf(errbf, sizeof(errbf), "Pointer %p is of C-type `%s' while"
                " cast to `%s' requested", ptr, it->second.c_str(), typeName );
        throw std::runtime_error(errbf);
    }
    return ptr;
}
#endif

extern "C" int
hdql_context_free(hdql_Context_t ctx, hdql_Datum_t ptr) {
    // TODO: page-aligned/pool allocation
    free(ptr);
    return 0;
}

extern "C" void
hdql_context_add_virtual_compound(hdql_Context_t ctx, struct hdql_Compound * compoundPtr) {
    ctx->virtualCompounds.push_back(compoundPtr);
}

extern "C" void
hdql_context_err_push( hdql_Context_t context
                     , hdql_Err_t code
                     , const char * format, ...) {
    char errBuf[1024];  // TODO: macro parameter?
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(errBuf, sizeof(errBuf), format, argptr);
    va_end(argptr);
    context->errors.push_back(std::pair<hdql_Err_t, std::string>(code, errBuf));
}

extern "C" void
hdql_context_destroy(hdql_Context_t ctx) {
    if(ctx->functions)
        _hdql_functions_destroy(ctx->functions, ctx);
    if(ctx->operations)
        _hdql_operations_destroy(ctx->operations, ctx);
    for(auto vCompoundPtr : ctx->virtualCompounds) {
        hdql_virtual_compound_destroy(vCompoundPtr, ctx);
    }
    if(ctx->converters)
        _hdql_converters_destroy(ctx->converters, ctx);
    if(ctx->valueTypes)
        _hdql_value_types_table_destroy(ctx->valueTypes);
    delete ctx;
}

