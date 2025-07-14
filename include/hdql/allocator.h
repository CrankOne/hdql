#ifndef H_HDQL_ALLOCATOR_H
#define H_HDQL_ALLOCATOR_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hdql_Allocator {
    void * userdata;
    void * (*alloc)(size_t size, void * userdata);
    void (*free)(void * data, void * userdata);
};

extern const struct hdql_Allocator hdql_gHeapAllocator;

void hdql_alloc_arena_init(struct hdql_Allocator *);
void hdql_alloc_arena_destroy(struct hdql_Allocator *);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_HDQL_ALLOCATOR_H */
