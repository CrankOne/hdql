#ifndef H_HDQL_QUERY_KEY_H
#define H_HDQL_QUERY_KEY_H

#include "hdql/query.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_CollectionKey {
    /**\brief Collection key type
     *
     * HDQL manages key instance lifecycle, so key must be one of the
     * registered types.
     *
     * Zero code is reserved for empty key placeholders and key lists. */
    hdql_ValueTypeCode_t code:HDQL_VALUE_TYPEDEF_CODE_BITSIZE;
    hdql_ValueTypeCode_t isList:1;
    hdql_ValueTypeCode_t isTerminal:1;
    /**\brief Ptr to actual key value */
    union {
        hdql_Datum_t datum;
        struct hdql_CollectionKey * keysList;
    } pl;
};


/**\brief Reserves keys for the query */
int
hdql_query_keys_reserve( struct hdql_Query * query
                       , struct hdql_CollectionKey ** keys
                       , hdql_Context_t ctx );

int
hdql_query_keys_copy( struct hdql_CollectionKey * dest
                    , const struct hdql_CollectionKey * src
                    , hdql_Context_t ctx
                    );

typedef int (*hdql_KeyIterationCallback_t)(
          const struct hdql_CollectionKey *
        , hdql_Context_t
        , void *
        , size_t queryLevel
        , size_t queryNoInLevel
        );

int
hdql_query_keys_for_each( const struct hdql_CollectionKey *
                        , hdql_Context_t ctx
                        , hdql_KeyIterationCallback_t callback
                        , void * userdata
                        );

int
hdql_query_keys_dump( const struct hdql_CollectionKey * key
                    , char * buf, size_t bufLen
                    , hdql_Context_t ctx
                    );

int
hdql_query_keys_destroy( struct hdql_CollectionKey * keys
                       , hdql_Context_t ctx );


/*
 * Flat key view
 */

void
hdql_query_print_key_to_buf(
          const struct hdql_Query * query
        , const struct hdql_CollectionKey * key
        , hdql_Context_t ctx
        , void * userdata
        , size_t queryLevel
        , size_t queryNoInLevel
        );


size_t
hdql_keys_flat_view_size( const struct hdql_Query * q
                        , const struct hdql_CollectionKey * keys
                        , hdql_Context_t ctx
                        );

struct hdql_KeyView {
    hdql_ValueTypeCode_t               code;
    const struct hdql_CollectionKey  * keyPtr;
    const struct hdql_ValueInterface * interface;
};

int
hdql_keys_flat_view_update( const struct hdql_Query * q
                          , const struct hdql_CollectionKey * keys
                          , struct hdql_KeyView * dest
                          , hdql_Context_t ctx );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_QUERY_KEY_H */
