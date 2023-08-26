#include "hdql/key.h"
#include "hdql/types.h"
#include <string.h>
#include <stdlib.h>

#if 0
#define HDQL_KEY_UNSET_TYPE   0x0
#define HDQL_KEY_INT_TYPE     0x1
#define HDQL_KEY_LUINT_TYPE   0x2
#define HDQL_KEY_FLOAT_TYPE   0x4
#define HDQL_KEY_DOUBLE_TYPE  0x5
#define HDQL_KEY_STRING_TYPE  0x10

int
hdql_collection_key_set_string( struct hdql_CollectionKeyValue * dest
                              , const char * strKey
                              , hdql_Context_t
                              ) {
    if(HDQL_KEY_UNSET_TYPE == dest->typeCode) {
        size_t sz = strlen(strKey);
        ++sz;
        dest->data.asString = (char*) malloc(sz);  // TODO: use context allocations
        memcpy(dest->data.asString, strKey, sz);
        dest->stringSize = sz;
        dest->typeCode = HDQL_KEY_STRING_TYPE;
        return 0;
    }
    if(HDQL_KEY_STRING_TYPE == dest->typeCode) {
        size_t sz = strlen(strKey);
        ++sz;
        if(sz > dest->stringSize) {
            free(dest->data.asString);
            dest->data.asString = (char *) malloc(sz);
            dest->stringSize = sz;
        }
        memcpy(dest->data.asString, strKey, sz);
        return 0;
    }
    return -1;
}

void
hdql_collection_key_as_string( struct hdql_CollectionKeyValue * dest
                             , char * buf
                             , size_t buflen
                             , hdql_Context_t
                             ) {
    // TODO
}

void
hdql_collection_key_destroy( struct hdql_CollectionKeyValue *
                           , hdql_Context_t ctx
                           ) {
    // TODO
}
#endif

