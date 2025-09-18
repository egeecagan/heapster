#ifndef HEAPSTER_STATS_H
#define HEAPSTER_STATS_H

#include "internal.h"

// global stat struct her arenada gomulu olcak 
typedef struct {
    size_t total_bytes;        // arena toplam boyutu (header + payload dahil)
    size_t used_bytes;         // kullaniciya ayrilmis (allocated) payload toplami
    size_t free_bytes;         // su anda free durumda olan payload toplami

    // arena 1000 birim 100 birim header mesela 400 ü user malloc ile aldı 500 de free

    size_t largest_free_block; // en buyuk free block'un payload boyutu
    size_t free_block_count;   // free block sayisi
    size_t allocated_block_count; // su anda kullanilan block sayisi

    size_t wasted_bytes;       // requested_size < block->size oldugunda olusan internal fragmentation
    double fragmentation_ratio; // (1 - largest_free_block / free_bytes) gibi bir oran

    uint64_t alloc_calls;      // malloc benzeri cagri sayisi
    uint64_t free_calls;       // free cagri sayisi
    uint64_t realloc_calls;    // realloc cagri sayisi
    uint64_t calloc_calls;     // calloc cagri sayisi
} heapster_stats_t;



#endif /* HEAPSTER_STATS_H */
