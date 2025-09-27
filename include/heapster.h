/*
 * heapster — custom heap memory allocator
 */

#ifndef HEAPSTER_H
#define HEAPSTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEAPSTER_FIRST_FIT     = 0,
    HEAPSTER_NEXT_FIT      = 1,
    HEAPSTER_BEST_FIT      = 2,
    HEAPSTER_WORST_FIT     = 3
} heapster_policy_t;

void *heapster_malloc(size_t size);
void heapster_free(void *ptr);
void *heapster_realloc(void *ptr, size_t new_size);
void *heapster_calloc(size_t nmemb, size_t size);

void heapster_set_policy(heapster_policy_t policy);
heapster_policy_t heapster_get_policy(void);

void heapster_set_mmap_threshold(size_t bytes);
size_t heapster_get_mmap_threshold(void);

int heapster_init(size_t arena_size, heapster_policy_t policy);
int heapster_finalize(void);

#ifdef __cplusplus
}
#endif

#endif /* HEAPSTER_H */
