#ifndef HEAPSTER_STATS_H
#define HEAPSTER_STATS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t total_bytes;        // total size of an arena (arena header + block header + total payload(user available area))
    size_t free_bytes;         // su anda free durumda olan payload toplami ( total - arena header - block header - used )
    size_t used_bytes;         // user allocated payload total

    size_t largest_free_block; // en buyuk free block'un payload boyutu
    size_t free_block_count;   // free block sayisi
    size_t allocated_block_count; // su anda kullanilan block sayisi

    size_t wasted_bytes;       // requested_size < block->size oldugunda olusan internal fragmentation
    double fragmentation_ratio; // (1 - largest_free_block / free_bytes) gibi bir oran

    uint64_t malloc_calls;      // malloc cagri sayisi
    uint64_t free_calls;       // free cagri sayisi
    uint64_t realloc_calls;    // realloc cagri sayisi
    uint64_t calloc_calls;     // calloc cagri sayisi
} heapster_stats_t;

#endif 
