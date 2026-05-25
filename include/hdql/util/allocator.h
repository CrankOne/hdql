#ifndef H_HDQL_ALLOCATOR_H
#define H_HDQL_ALLOCATOR_H

#include "hdql/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_Allocator {
    /** User payload for allocator of type */
    void * userdata;
    /**\return ptr to mem block on success, NULL on failure */
    void * (*alloc)(size_t size, void * userdata);
    /**\return 0 on success, 1 on failure */
    int (*free)(void * data, void * userdata);
};

HDQL_API extern const struct hdql_Allocator hdql_gHeapAllocator;

HDQL_API void hdql_alloc_arena_init(struct hdql_Allocator *);
HDQL_API void hdql_alloc_arena_destroy(struct hdql_Allocator *);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_ALLOCATOR_H */
