#ifndef H_HDQL_FUNCTION_H
#define H_HDQL_FUNCTION_H

#include "hdql/compound.h"
#include "hdql/operations.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Common struct for function-like objects
 * */
struct hdql_Func;

/**\brief Dictionary of functions */
struct hdql_Functions;

#if 1
struct hdql_FuncDef {
    bool returnsCollection;
    bool returnsCompoundType;
    bool stateful;

    /**\brief Supplementary function definition data*/
    hdql_Datum_t definitionData;

    /**\brief Initializes userdata for function using given argument queries*/
    hdql_Datum_t (*instantiate)( struct hdql_Query **
                               , struct hdql_AttrDef *
                               , hdql_Datum_t definitionData
                               , hdql_Context_t
                               );
    /**\brief Invokes a function, sets the result */
    int (*call)(hdql_Datum_t root, hdql_Datum_t result, hdql_Datum_t userdata, hdql_Context_t context);
    /**\brief Deletes userdata */
    void (*destroy)(hdql_Datum_t, hdql_Context_t);

    int (*reserve_keys)();
    int (*copy_keys)();
    int (*call_on_keys)();
    int (*destroy_keys)();
};
#endif

#if 1
/**\brief Reserves keys sub-list
 *
 * Given pointer will be set to an array of keys containing sub-arrays.
 * Terminated with an item of type code `HDQL_VALUE_TYPE_CODE_MAX`. */
int hdql_func_reserve_keys( struct hdql_Func *
                          , struct hdql_CollectionKey ** dest
                          , hdql_Context_t );

/**\brief Copies argument keys */
int hdql_func_copy_keys( struct hdql_Func * fDef
                       , struct hdql_CollectionKey * dest
                       , const struct hdql_CollectionKey * src
                       , hdql_Context_t ctx
                       );
/**\brief Invoces callback on every key respecting query topology */
int hdql_func_call_on_keys( struct hdql_Func * fDef
                          , struct hdql_CollectionKey * keys
                          , hdql_KeyIterationCallback_t callback, void * userdata
                          , size_t cLevel, size_t nKeyAtLevel
                          , hdql_Context_t context
                          );
/**\brief Destroys keys respecting query topology */
int hdql_func_destroy_keys( struct hdql_Func * fDef
                          , struct hdql_CollectionKey * keys
                          , hdql_Context_t
                          );
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_FUNCTION_H */
