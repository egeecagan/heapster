/*
 * heapster — custom heap memory allocator
 */

#ifndef HEAPSTER_H
#define HEAPSTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "internal.h"  // internal struct definitions and macros except functions
#include "stats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEAPSTER_FIRST_FIT     = 0,
    HEAPSTER_NEXT_FIT      = 1,
    HEAPSTER_BEST_FIT      = 2,
    HEAPSTER_WORST_FIT     = 3
} heapster_policy_t;

extern pthread_mutex_t arena_list_lock;

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

/* heap information */
void heapster_arena_stats(arena_t *arena);
void heapster_get_stats(heapster_stats_t *out_stats);

/* verifies heap integrity by checking free lists and detecting possible double free errors */
int heapster_validate_heap(void);



/* managing the area */
arena_t *arena_create(size_t size);
void arena_destroy(arena_t *arena);

/* hook mechanism (for profiling) 
typedef void (*heapster_hook_t)(const char *func, void *ptr, size_t size);
void heapster_set_hooks(heapster_hook_t on_alloc, heapster_hook_t on_free);
*/

/* heapster life cycle */
int heapster_init(size_t arena_size, heapster_policy_t policy);
int heapster_finalize(void);

arena_t *arena_get_list(void);
int last_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* HEAPSTER_H */
