#include "hdql/allocator.h"

#include <memory.h>
#include <assert.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>

/*
 * Allocator, forwarding calls to system malloc()/free()
 */

static void * _hdql_std_malloc(size_t sz, void * unused) {
    assert(sz > 0);
    return malloc(sz);
}

static void _hdql_std_free(void * ptr, void * unused) {
    free(ptr);
}

const struct hdql_Allocator hdql_gHeapAllocator = {
    .userdata = NULL, 
    .alloc = _hdql_std_malloc,
    .free = _hdql_std_free
};


/*
 * Arena allocator
 */

#ifndef PAGE_SIZE
#   define PAGE_SIZE 4096  /* Fallback if sysconf fails */
#endif

struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used;
    size_t capacity;
    char data[];
};

typedef struct {
    struct ArenaBlock *head;
    size_t pageSize;
} Arena;

static void *arena_alloc(size_t size, void *userdata) {
    Arena *arena = (Arena *)userdata;

    // Align to pointer-size
    size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    struct ArenaBlock *block = arena->head;

    if (!block || block->used + size > block->capacity) {
        size_t allocSize = size + sizeof(struct ArenaBlock);
        allocSize = ((allocSize + arena->pageSize - 1) / arena->pageSize) * arena->pageSize;

        struct ArenaBlock * newBlock
            = (struct ArenaBlock * ) mmap(NULL, allocSize, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!newBlock || newBlock == MAP_FAILED)
            return NULL;

        newBlock->next = block;
        newBlock->used = 0;
        newBlock->capacity = allocSize - sizeof(struct ArenaBlock);

        arena->head = newBlock;
        block = newBlock;
    }

    void *mem = block->data + block->used;
    block->used += size;
    return mem;
}

static void arena_free(void *data, void *userdata) {
    // No-op: individual free not supported
    (void)data;
    (void)userdata;
}

void
hdql_alloc_arena_init(struct hdql_Allocator * alloc) {
    Arena * arena = (Arena*) malloc(sizeof(Arena));
    arena->head = NULL;
    arena->pageSize = sysconf(_SC_PAGESIZE);
    if (arena->pageSize <= 0) arena->pageSize = PAGE_SIZE;

    alloc->alloc = arena_alloc;
    alloc->free = arena_free;
    alloc->userdata = arena;
}

void
hdql_alloc_arena_destroy(struct hdql_Allocator * alloc) {
    Arena * arena = (Arena*) alloc->userdata;
    struct ArenaBlock * blk = arena->head;
    while (blk) {
        struct ArenaBlock *next = blk->next;
        munmap(blk, blk->capacity + sizeof(struct ArenaBlock));
        blk = next;
    }
    free(arena);
}

