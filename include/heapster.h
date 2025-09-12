/*
 * heapster — custom heap memory allocator
 */

#ifndef HEAPSTER_H
#define HEAPSTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEAPSTER_FIRST_FIT     = 0,
    HEAPSTER_NEXT_FIT      = 1,
    HEAPSTER_BEST_FIT      = 2,
    HEAPSTER_WORST_FIT     = 3,
    HEAPSTER_BUDDY         = 4,
    HEAPSTER_SEGREGATED    = 5,
    HEAPSTER_SLAB          = 6
} heapster_policy_t;

typedef struct {
    size_t total_heap_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t largest_free_block;
    size_t free_block_count;
    size_t alloc_block_count;
    double fragmentation_ratio;
    uint64_t alloc_calls;
    uint64_t free_calls;
    uint64_t realloc_calls;
    uint64_t calloc_calls;
} heapster_stats_t;

/* basic api */
void *heapster_malloc(size_t size);
void heapster_free(void *ptr);
void *heapster_realloc(void *ptr, size_t new_size);
void *heapster_calloc(size_t nmemb, size_t size);

/* policy controls */
void heapster_set_policy(heapster_policy_t policy);
heapster_policy_t heapster_get_policy(void);
void heapster_set_mmap_threshold(size_t bytes);
size_t heapster_get_mmap_threshold(void);

/* three policy settings */
void heapster_set_buddy_order(int min_order, int max_order);
void heapster_set_segregated_classes(size_t *class_sizes, int class_count);
void heapster_set_slab_cache_size(size_t obj_size, int cache_count);

/* heap information */
void heapster_get_stats(heapster_stats_t *out_stats);
void heapster_dump_heap(FILE *out);

/* verifies heap integrity by checking free lists and detecting possible double free errors */
int heapster_validate_heap(void);

/* debug logging */
void heapster_enable_logging(int enabled);

/* managing the area */
int heapster_create_arena(size_t size);
void heapster_destroy_arena(int arena_id);

/* hook mechanism (for profiling) 
typedef void (*heapster_hook_t)(const char *func, void *ptr, size_t size);
void heapster_set_hooks(heapster_hook_t on_alloc, heapster_hook_t on_free);
*/

/* alignment api extra */
int heapster_posix_memalign(void **memptr, size_t alignment, size_t size);

/* heapster life cycle */
int heapster_init(size_t arena_size, heapster_policy_t policy);
int heapster_finalize(void);

#ifdef __cplusplus
}
#endif

#endif /* HEAPSTER_H */
