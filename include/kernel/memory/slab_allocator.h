#pragma once

#include <memory/page_allocator.h>
#include <memory/slab_allocator.h>

#define SLAB(SIZE)                                                                                                     \
    struct cache_entry_##SIZE {                                                                                        \
        union {                                                                                                        \
            struct cache_entry_##SIZE *next;                                                                           \
            char storage[SIZE];                                                                                        \
        };                                                                                                             \
    };                                                                                                                 \
                                                                                                                       \
    struct slab_##SIZE;                                                                                                \
    struct cache_##SIZE {                                                                                              \
        /*struct slab_##SIZE *slab;*/                                                                                  \
        struct cache_##SIZE *next_cache;                                                                               \
        struct cache_entry_##SIZE *first_free;                                                                         \
        struct cache_entry_##SIZE entries[];                                                                           \
    };                                                                                                                 \
                                                                                                                       \
    struct slab_##SIZE {                                                                                               \
        struct cache_##SIZE *first_empty;                                                                              \
        struct cache_##SIZE *first_partial;                                                                            \
        struct cache_##SIZE *first_full;                                                                               \
    };                                                                                                                 \
                                                                                                                       \
    void create_slab_##SIZE(struct slab_##SIZE *);                                                                     \
    void *slab_alloc_##SIZE(struct slab_##SIZE *);                                                                     \
    void slab_free_##SIZE(struct slab_##SIZE *, void *);                                                               \
    void slab_dbg_##SIZE(struct slab_##SIZE *);

SLAB(4)
SLAB(8)
SLAB(16)
SLAB(32)

#undef SLAB

#define _SLAB_GENERIC_CASE(f, SIZE) struct slab_##SIZE * : f##_##SIZE
#define _SLAB_GENERIC_PARAMS(f)                                                                                        \
    _SLAB_GENERIC_CASE(f, 4), _SLAB_GENERIC_CASE(f, 8), _SLAB_GENERIC_CASE(f, 16), _SLAB_GENERIC_CASE(f, 32)
#define _SLAB_GENERIC_1(f, X)    _Generic((X), _SLAB_GENERIC_PARAMS(f))(X)
#define _SLAB_GENERIC_2(f, X, Y) _Generic((X), _SLAB_GENERIC_PARAMS(f))(X, Y)

#define create_slab(X)  _SLAB_GENERIC_1(create_slab, X)
#define slab_alloc(X)   _SLAB_GENERIC_1(slab_alloc, X)
#define slab_dbg(X)     _SLAB_GENERIC_1(slab_dbg, X)
#define slab_free(X, Y) _SLAB_GENERIC_2(slab_free, X, Y)

extern struct slab_4 root_slab4;
extern struct slab_8 root_slab8;
extern struct slab_16 root_slab16;
extern struct slab_32 root_slab32;
void init_root_slabs(void);

#ifdef TESTS

void slab_test_suite(void);

#endif