/*
 * heapster — custom heap memory allocator
 */

#ifndef HEAPSTER_H
#define HEAPSTER_H

#include <stddef.h>
#include <stdint.h>  
#include <stdio.h>   

typedef enum {
    HEAPSTER_FIRST_FIT = 0,
    HEAPSTER_BEST_FIT  = 1,
    HEAPSTER_BUDDY     = 2  
} heapster_policy_t;


typedef struct {
    size_t total_heap_bytes;    /* bytes acquired from OS (sbrk/mmap) */
    size_t used_bytes;          /* bytes given to users (payload) */
    size_t free_bytes;          /* bytes free in arenas */
    size_t largest_free_block;  /* size of largest free block */
    size_t free_block_count;    /* # of free blocks */
    size_t alloc_block_count;   /* # of allocated blocks */
    double fragmentation_ratio; /* e.g., 1 - largest_free_block/free_bytes */
} heapster_stats_t;


void *heapster_malloc(size_t size);
void  heapster_free(void *ptr);
void *heapster_realloc(void *ptr, size_t new_size);
void *heapster_calloc(size_t nmemb, size_t size);


void              heapster_set_policy(heapster_policy_t policy);
heapster_policy_t heapster_get_policy(void);
void   heapster_set_mmap_threshold(size_t bytes);
size_t heapster_get_mmap_threshold(void);


void heapster_get_stats(heapster_stats_t *out_stats);
void heapster_dump_heap(FILE *out);


int  heapster_init(void);
void heapster_finalize(void);

#endif 
