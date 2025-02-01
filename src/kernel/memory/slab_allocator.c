#include "stddef.h"
#include <common.h>
#include <kernel.h>
#include <memory/slab_allocator.h>
#include <stdio.h>

#include <memory_mgmt.h>

#ifdef SLAB_DEBUG
#define SLAB_DBG(...) KDBG("[SLAB-CORE] " __VA_ARGS__)
#else
#define SLAB_DBG(...)
#endif

struct slab_4 root_slab4;
struct slab_8 root_slab8;
struct slab_16 root_slab16;
struct slab_32 root_slab32;

void init_root_slabs(void) {
    kprintf("Initializing root slab allocators...\n");
    create_slab(&root_slab4);
    create_slab(&root_slab8);
    create_slab(&root_slab16);
    create_slab(&root_slab32);
}

#define SLAB(SIZE, PAGES_PER_SLAB)                                                                                     \
    void create_slab_##SIZE(struct slab_##SIZE *slab) {                                                                \
        size_t capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;                           \
        paddr_t page = alloc_pages(1);                                                                                 \
        struct cache_##SIZE *cache = (struct cache_##SIZE *)page;                                                      \
        SLAB_DBG("Capacity of cache at 0x%p is %d objects of %d bytes each (PAGE size minus %d-byte header).\n",       \
                 cache, capacity, SIZE, sizeof(struct cache_##SIZE));                                                  \
        slab->first_full = NULL;                                                                                       \
        slab->first_empty = cache;                                                                                     \
        /* cache->slab = slab;*/                                                                                       \
        cache->first_free = cache->entries;                                                                            \
        for (size_t i = 1; i < capacity; i++)                                                                          \
            cache->entries[i - 1].next = &cache->entries[i];                                                           \
        cache->entries[capacity - 1].next = NULL;                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    void *slab_alloc_##SIZE(struct slab_##SIZE *slab) {                                                                \
        struct cache_##SIZE *cache = slab->first_partial;                                                              \
        if (cache == NULL) {                                                                                           \
            if (slab->first_empty != NULL) {                                                                           \
                SLAB_DBG("Consumed last partial, getting next empty and moving to partial list.\n");                   \
                cache = slab->first_empty;                                                                             \
                slab->first_empty = cache->next_cache;                                                                 \
                cache->next_cache = NULL;                                                                              \
                slab->first_partial = cache;                                                                           \
            } else {                                                                                                   \
                SLAB_DBG("OUT OF CACHES, allocating a new page...\n");                                                 \
                size_t capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;                   \
                cache = (struct cache_##SIZE *)alloc_pages(1);                                                         \
                SLAB_DBG(                                                                                              \
                    "Capacity of cache at 0x%p is %d objects of %d bytes each (PAGE size minus %d-byte header).\n",    \
                    cache, capacity, SIZE, sizeof(struct cache_##SIZE));                                               \
                /*cache->slab = slab;*/                                                                                \
                cache->next_cache = NULL;                                                                              \
                cache->first_free = cache->entries;                                                                    \
                for (size_t i = 1; i < capacity; i++)                                                                  \
                    cache->entries[i - 1].next = &cache->entries[i];                                                   \
                cache->entries[capacity - 1].next = NULL;                                                              \
                slab->first_partial = cache;                                                                           \
            }                                                                                                          \
        }                                                                                                              \
        if (cache == NULL) /* TODO */                                                                                  \
            PANIC("UNREACHABLE: OUT OF CACHES!\n");                                                                    \
                                                                                                                       \
        struct cache_entry_##SIZE *free = cache->first_free;                                                           \
        if (free == NULL) /* TODO */                                                                                   \
            PANIC("UNREACHABLE: CACHE OUT OF SLOTS\n");                                                                \
                                                                                                                       \
        struct cache_entry_##SIZE *next = free->next;                                                                  \
        if (next == NULL) { /* TODO */                                                                                 \
            SLAB_DBG("CONSUMED LAST CACHE SLOT. Moving to full list.\n");                                              \
            slab->first_partial = cache->next_cache;                                                                   \
            cache->next_cache = slab->first_full;                                                                      \
            slab->first_full = cache;                                                                                  \
        }                                                                                                              \
                                                                                                                       \
        cache->first_free = next;                                                                                      \
        return (void *)free->storage;                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    void slab_free_##SIZE(struct slab_##SIZE *slab, void *ptr) {                                                       \
        struct cache_##SIZE *cache = NULL;                                                                             \
        /* Search first in partial list... */                                                                          \
        for (cache = slab->first_partial;                                                                              \
             cache != NULL && !(ptr > (void *)cache && ptr < (void *)((paddr_t)cache + PAGE_SIZE * PAGES_PER_SLAB));   \
             cache = cache->next_cache)                                                                                \
            ;                                                                                                          \
        if (cache == NULL)                                                                                             \
            for (cache = slab->first_full;                                                                             \
                 cache != NULL &&                                                                                      \
                 !(ptr > (void *)cache && ptr < (void *)((paddr_t)cache + PAGE_SIZE * PAGES_PER_SLAB));                \
                 cache = cache->next_cache)                                                                            \
                ;                                                                                                      \
        if (cache == NULL)                                                                                             \
            PANIC("Could not find cache that owns pointer 0x%p!", ptr);                                                \
        struct cache_entry_##SIZE *e = cache->first_free;                                                              \
        for (; e != NULL && e != ptr; e = e->next)                                                                     \
            ;                                                                                                          \
        if (e != NULL)                                                                                                 \
            PANIC("Double-free!!\n");                                                                                  \
        /* TODO: check if this is a double-free! */                                                                    \
        /*SLAB_DBG("Cache is likely the one at 0x%p\n", cache);*/                                                      \
        struct cache_entry_##SIZE *entry = (struct cache_entry_##SIZE *)ptr;                                           \
        bool full = cache->first_free == NULL;                                                                         \
        entry->next = cache->first_free;                                                                               \
        cache->first_free = entry;                                                                                     \
        /* TODO: count cache use, check if it needs to be moved from the full list to the partial list, or from the    \
         * partial list to the empty list. */                                                                          \
        size_t count = 0, capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;                \
        for (struct cache_entry_##SIZE *e = entry; e != NULL; e = e->next)                                             \
            count++;                                                                                                   \
        /*SLAB_DBG("Slab appears to have %d free entries (out of %d total, or %d%% free)...\n", count, capacity,       \
                 (count * 100) / capacity);*/                                                                          \
        if (full || count == capacity) {                                                                               \
            /* Move previously full slab to partial list (if full==true) or move previously partial slab to empty list \
             * (if full==false). */                                                                                    \
            struct cache_##SIZE *prev = full ? slab->first_full : slab->first_partial;                                 \
            if (prev == cache)                                                                                         \
                prev = NULL;                                                                                           \
            else                                                                                                       \
                for (; prev != NULL && prev->next_cache != cache; prev = prev->next_cache)                             \
                    ;                                                                                                  \
            /*if (prev->next_cache == cache)                                                                           \
                break;*/                                                                                               \
                                                                                                                       \
            if (prev == NULL) {                                                                                        \
                SLAB_DBG("\tCache is the first entry in %s...\n", full ? "full list" : "partial list");                \
                if (full) {                                                                                            \
                    slab->first_full = cache->next_cache;                                                              \
                    SLAB_DBG("\tslab->first_full is now 0x%p...\n", slab->first_full);                                 \
                } else                                                                                                 \
                    slab->first_partial = cache->next_cache;                                                           \
            } else {                                                                                                   \
                prev->next_cache = cache->next_cache;                                                                  \
            }                                                                                                          \
                                                                                                                       \
            cache->next_cache = full ? slab->first_partial : slab->first_empty;                                        \
            if (full)                                                                                                  \
                slab->first_partial = cache;                                                                           \
            else                                                                                                       \
                slab->first_empty = cache;                                                                             \
            SLAB_DBG("Moved %s %s slab to the head of the %s list!\n", prev == NULL ? "the last" : "a",                \
                     full ? "full" : "partial", full ? "partial" : "free");                                            \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    void slab_dbg_##SIZE(struct slab_##SIZE *slab) {                                                                   \
        kprintf("Debug of slab_" #SIZE " at 0x%p:\n", slab);                                                           \
        size_t count = 0, free = 0, capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;      \
        for (struct cache_##SIZE *c = slab->first_empty; c != NULL; c = c->next_cache) {                               \
            count++;                                                                                                   \
            free += capacity;                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        for (struct cache_##SIZE *c = slab->first_partial; c != NULL; c = c->next_cache) {                             \
            count++;                                                                                                   \
            for (struct cache_entry_##SIZE *e = c->first_free; e != NULL; e = e->next)                                 \
                free++;                                                                                                \
        }                                                                                                              \
                                                                                                                       \
        for (struct cache_##SIZE *c = slab->first_full; c != NULL; c = c->next_cache)                                  \
            count++;                                                                                                   \
                                                                                                                       \
        kprintf(" - Capacity: %d entries per cache, %d caches total, %d total capacity,\n"                             \
                "                            of which %d entries are unallocated (%d.%d%% free).\n",                   \
                capacity, count, capacity * count, free, (free * 100) / (capacity * count),                            \
                ((free * 1000) / (capacity * count)) % 10);                                                            \
        if (slab->first_empty == NULL)                                                                                 \
            kprintf(" - No empty caches.\n");                                                                          \
        else {                                                                                                         \
            kprintf(" - Empty caches:\n");                                                                             \
            for (struct cache_##SIZE *cache = slab->first_empty; cache != NULL; cache = cache->next_cache)             \
                kprintf("    - Cache at 0x%p.\n", cache);                                                              \
        }                                                                                                              \
        if (slab->first_partial == NULL)                                                                               \
            kprintf(" - No partial caches.\n");                                                                        \
        else {                                                                                                         \
            kprintf(" - Partial caches:\n");                                                                           \
            for (struct cache_##SIZE *cache = slab->first_partial; cache != NULL; cache = cache->next_cache) {         \
                size_t count = 0, capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;        \
                for (struct cache_entry_##SIZE *e = cache->first_free; e != NULL; e = e->next)                         \
                    count++;                                                                                           \
                kprintf("    - Cache at 0x%p has %d of %d free entries (%d.%d%% free).\n", cache, count, capacity,     \
                        (count * 100) / capacity, ((count * 1000) / capacity) % 10);                                   \
            }                                                                                                          \
        }                                                                                                              \
        if (slab->first_full == NULL)                                                                                  \
            kprintf(" - No full caches.\n");                                                                           \
        else {                                                                                                         \
            kprintf(" - Full caches:\n");                                                                              \
            for (struct cache_##SIZE *cache = slab->first_full; cache != NULL; cache = cache->next_cache) {            \
                kprintf("    - Cache at 0x%p.\n", cache);                                                              \
            }                                                                                                          \
        }                                                                                                              \
    }

SLAB(4, 1)
SLAB(8, 1)
SLAB(16, 1)
SLAB(32, 2)

#undef SLAB

#ifdef TESTS

#define SLAB(SIZE, PAGES_PER_SLAB)                                                                                     \
    void slab_test_suite_##SIZE(void) {                                                                                \
        struct slab_##SIZE slab;                                                                                       \
        printf("[SLAB-TESTS] Allocating slab%d...\n", SIZE);                                                           \
        create_slab(&slab);                                                                                            \
                                                                                                                       \
        /* Check that objects are being allocated at appropriate addresses. */                                         \
        printf("[SLAB-TESTS] Allocating first object...\n");                                                           \
        struct test *test1 = slab_alloc(&slab);                                                                        \
        printf("[SLAB-TESTS] Allocated first object at 0x%p.\n", test1);                                               \
        printf("[SLAB-TESTS] Allocating second object...\n");                                                          \
        struct test *test2 = slab_alloc(&slab);                                                                        \
        printf("[SLAB-TESTS] Allocated second object at 0x%p.\n", test2);                                              \
                                                                                                                       \
        /* Consume all of the current slots (minus th) */                                                              \
        size_t capacity = (PAGE_SIZE * PAGES_PER_SLAB - sizeof(struct cache_##SIZE)) / SIZE;                           \
        printf("[SLAB-TESTS] Allocating %d more objects...\n", capacity - 3);                                          \
        struct cache_entry_##SIZE *test_range_start = NULL;                                                            \
        void *last = NULL;                                                                                             \
        for (size_t i = 0; i < (capacity - 3); i++) {                                                                  \
            void *ptr = slab_alloc(&slab);                                                                             \
            if (i == 0)                                                                                                \
                test_range_start = ptr;                                                                                \
            else if ((ptr - last) != SIZE) {                                                                           \
                PANIC("[SLAB-TESTS] Non-contiguous allocations (on item %d, expected `0x%p - 0x%p == %d`, but got %d " \
                      "instead)!",                                                                                     \
                      i, ptr, last, SIZE, ptr - last);                                                                 \
            }                                                                                                          \
            last = ptr;                                                                                                \
        }                                                                                                              \
                                                                                                                       \
        /* Consume the last slot, and then one more, to test allocating a new buffer. */                               \
        printf("[SLAB-TESTS] Allocating one more...\n");                                                               \
        struct test *test_last = slab_alloc(&slab);                                                                    \
        printf("[SLAB-TESTS] Allocated object #%d at 0x%p.\n", capacity, test_last);                                   \
        printf("[SLAB-TESTS] And another...\n");                                                                       \
        struct test *test_last_plus_one = slab_alloc(&slab);                                                           \
        printf("[SLAB-TESTS] Allocated object #%d at 0x%p.\n", capacity + 1, test_last_plus_one);                      \
        slab_dbg(&slab);                                                                                               \
                                                                                                                       \
        /* Free a slot. This will free the one allocated slot in the second buffer. */                                 \
        printf("[SLAB-TESTS] Freeing the one allocated object in the second (partial) cache.\n");                      \
        slab_free(&slab, test_last_plus_one);                                                                          \
        slab_dbg(&slab);                                                                                               \
                                                                                                                       \
        /* Free some more. This will the first slot in the first buffer. */                                            \
        printf("[SLAB-TESTS] Freeing a single allocated object in the first (full) cache.\n");                         \
        slab_free(&slab, test_last);                                                                                   \
        slab_dbg(&slab);                                                                                               \
        printf("[SLAB-TESTS] Freeing %d objects...\n", capacity - 3);                                                  \
        for (size_t i = 0; i < (capacity - 3); i++) {                                                                  \
            slab_free(&slab, test_range_start + i);                                                                    \
        }                                                                                                              \
        /* Print some debug info. */                                                                                   \
        slab_dbg(&slab);                                                                                               \
                                                                                                                       \
        printf("[SLAB-TESTS] Freeing second-to-last-object\n");                                                        \
        slab_free(&slab, test1);                                                                                       \
                                                                                                                       \
        /* printf("Double-freeing 0x%p...\n", test1);                                                                  \
         * slab_free(&slab, test1);                                                                                    \
         * slab_dbg(&slab); */                                                                                         \
                                                                                                                       \
        printf("[SLAB-TESTS] Freeing last allocated object...\n");                                                     \
        slab_free(&slab, test2);                                                                                       \
        slab_dbg(&slab);                                                                                               \
    }

SLAB(4, 1)
SLAB(8, 1)
SLAB(16, 1)
SLAB(32, 2)

#undef SLAB

void slab_test_suite(void) {
    slab_test_suite_4();
    slab_test_suite_8();
    slab_test_suite_16();
    slab_test_suite_32();
}

#endif