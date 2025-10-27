#include <string.h>
#include <stdio.h>

#include "stats.h"
#include "internal.h"
#include "internal_f.h"

void arena_stats_reset(arena_t *arena) {
    if (!arena) {
        return;
    }
    memset(&arena->stats, 0, sizeof(arena->stats));
}

// tum arenalari tarayip global stats hesapla
void heapster_stats_update_global(heapster_stats_t *global_stats) {
    
}

static inline void arena_stats_print(const arena_t *arena) {
    if (!arena) {
        printf("[heapster] (no arena)\n");
        return;
    }

    const heapster_stats_t *stats = &arena->stats;

    printf("===== heapster statistics (arena id %llu) =====\n", (unsigned long long)arena->id);
    printf("total heap bytes     : %zu\n", stats->total_bytes);
    printf("used bytes           : %zu\n", stats->used_bytes);
    printf("free bytes           : %zu\n", stats->free_bytes);
    printf("largest free block   : %zu\n", stats->largest_free_block);
    printf("free block count     : %zu\n", stats->free_block_count);
    printf("allocated blocks     : %zu\n", stats->allocated_block_count);
    printf("wasted bytes         : %zu\n", stats->wasted_bytes);
    printf("fragmentation ratio  : %.2f\n", stats->fragmentation_ratio);
    printf("alloc calls          : %llu\n", (unsigned long long)stats->malloc_calls);
    printf("free calls           : %llu\n", (unsigned long long)stats->free_calls);
    printf("realloc calls        : %llu\n", (unsigned long long)stats->realloc_calls);
    printf("calloc calls         : %llu\n", (unsigned long long)stats->calloc_calls);
    printf("================================\n");
}