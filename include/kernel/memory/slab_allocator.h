#pragma once

#include <kernel.h>
#include <memory/page_allocator.h>
#include <memory/slab_allocator.h>
#include <util.h>

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

#define MAX_SLAB_SIZE 1024
#define SLAB_SIZES    X(4) X(8) X(16)

#define X(SIZE) SLAB(SIZE)
SLAB_SIZES
#undef X

#undef SLAB

#define X(SIZE) extern struct slab_##SIZE root_slab##SIZE;
SLAB_SIZES
#undef X

#define BIG_SLAB(SIZE)                                                                                                 \
    struct cache_entry_##SIZE {                                                                                        \
        union {                                                                                                        \
            struct cache_entry_##SIZE *next;                                                                           \
            char storage[SIZE];                                                                                        \
        };                                                                                                             \
    };                                                                                                                 \
                                                                                                                       \
    struct big_slab_##SIZE;                                                                                            \
    struct big_cache_##SIZE {                                                                                          \
        /*struct slab_##SIZE *slab;*/                                                                                  \
        struct big_cache_##SIZE *next_cache;                                                                           \
        struct cache_entry_##SIZE *first_free;                                                                         \
        struct cache_entry_##SIZE *entries;                                                                            \
    };                                                                                                                 \
                                                                                                                       \
    struct big_slab_##SIZE {                                                                                           \
        struct big_cache_##SIZE *first_empty;                                                                          \
        struct big_cache_##SIZE *first_partial;                                                                        \
        struct big_cache_##SIZE *first_full;                                                                           \
    };                                                                                                                 \
                                                                                                                       \
    void create_slab_big_##SIZE(struct big_slab_##SIZE *);                                                             \
    void *slab_alloc_big_##SIZE(struct big_slab_##SIZE *);                                                             \
    void slab_free_big_##SIZE(struct big_slab_##SIZE *, void *);                                                       \
    void slab_dbg_big_##SIZE(struct big_slab_##SIZE *);

#define BIG_SLAB_SIZES X(32) X(64) X(128) X(256) X(512) X(1024)

#define X(SIZE) BIG_SLAB(SIZE)
BIG_SLAB_SIZES
#undef X

#undef BIG_SLAB

#define X(SIZE) extern struct big_slab_##SIZE root_slab##SIZE;
BIG_SLAB_SIZES
#undef X

#define _SLAB_GENERIC_CASE(f, SIZE) struct slab_##SIZE * : f##_##SIZE

#define _SLAB_GENERIC_PARAMS(f) _SLAB_GENERIC_CASE(f, 4), _SLAB_GENERIC_CASE(f, 8), _SLAB_GENERIC_CASE(f, 16)

#define _BIG_SLAB_GENERIC_CASE(f, SIZE) struct big_slab_##SIZE * : f##_big_##SIZE

#define _BIG_SLAB_GENERIC_PARAMS(f)                                                                                    \
    _BIG_SLAB_GENERIC_CASE(f, 32), _BIG_SLAB_GENERIC_CASE(f, 64), _BIG_SLAB_GENERIC_CASE(f, 128),                      \
        _BIG_SLAB_GENERIC_CASE(f, 256), _BIG_SLAB_GENERIC_CASE(f, 512), _BIG_SLAB_GENERIC_CASE(f, 1024)

#define _SLAB_GENERIC_1(f, X)    _Generic((X), _SLAB_GENERIC_PARAMS(f), _BIG_SLAB_GENERIC_PARAMS(f))(X)
#define _SLAB_GENERIC_2(f, X, Y) _Generic((X), _SLAB_GENERIC_PARAMS(f), _BIG_SLAB_GENERIC_PARAMS(f))(X, Y)

#define create_slab(X)  _SLAB_GENERIC_1(create_slab, X)
#define slab_alloc(X)   _SLAB_GENERIC_1(slab_alloc, X)
#define slab_dbg(X)     _SLAB_GENERIC_1(slab_dbg, X)
#define slab_free(X, Y) _SLAB_GENERIC_2(slab_free, X, Y)

void init_root_slabs(void);

extern inline void *slab_malloc_dynamic(size_t);

#define _if_const_assert(x, msg) sizeof(struct { _Static_assert(!(__builtin_constant_p(x) ? (x) : 0), msg); })
#define __slab_malloc_s(size, MAX_SLAB_SIZE)                                                                           \
    __builtin_choose_expr(                                                                                             \
        __builtin_constant_p(size) ? 0 : 1, slab_malloc_dynamic(size),                                                 \
        __builtin_choose_expr(                                                                                         \
            __builtin_constant_p(size) ? size > MAX_SLAB_SIZE : 0,                                                     \
            _if_const_assert(size > MAX_SLAB_SIZE, "No slab large enough to allocate " #size                           \
                                                   " bytes (largest slab is " #MAX_SLAB_SIZE " bytes)"),               \
            __builtin_choose_expr(                                                                                     \
                __builtin_constant_p(size) ? size <= 4 : 0, slab_alloc_4(&root_slab4),                                 \
                __builtin_choose_expr(                                                                                 \
                    __builtin_constant_p(size) ? size <= 8 : 0, slab_alloc_8(&root_slab8),                             \
                    __builtin_choose_expr(                                                                             \
                        __builtin_constant_p(size) ? size <= 16 : 0, slab_alloc_16(&root_slab16),                      \
                        __builtin_choose_expr(                                                                         \
                            __builtin_constant_p(size) ? size <= 32 : 0, slab_alloc_big_32(&root_slab32),              \
                            __builtin_choose_expr(                                                                     \
                                __builtin_constant_p(size) ? size <= 64 : 0, slab_alloc_big_64(&root_slab64),          \
                                __builtin_choose_expr(                                                                 \
                                    __builtin_constant_p(size) ? size <= 128 : 0, slab_alloc_big_128(&root_slab128),   \
                                    __builtin_choose_expr(                                                             \
                                        __builtin_constant_p(size) ? size <= 256 : 0,                                  \
                                        slab_alloc_big_256(&root_slab256),                                             \
                                        __builtin_choose_expr(                                                         \
                                            __builtin_constant_p(size) ? size <= 512 : 0,                              \
                                            slab_alloc_big_512(&root_slab512),                                         \
                                            __builtin_choose_expr(__builtin_constant_p(size) ? size <= 1024 : 0,       \
                                                                  slab_alloc_big_1024(&root_slab1024),                 \
                                                                  (void *)NULL)))))))))))

// Double-expansion to allow #MAX_SLAB_SIZE to be the VALUE, not the NAME.
#define _slab_malloc_s(size, MAX_SLAB_SIZE) __slab_malloc_s(size, MAX_SLAB_SIZE)
#define slab_malloc(size)                   _slab_malloc_s(size, MAX_SLAB_SIZE)

#define slab_new(T) (T *)slab_malloc(sizeof(T))

#ifdef TESTS

void slab_test_suite(void);

#endif
