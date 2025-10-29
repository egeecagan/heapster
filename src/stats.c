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

