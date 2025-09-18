// public api ile disari acmak istemedigim fonksiyonlar burda olcak

#ifndef HEAPSTER_INTERNAL_F_H
#define HEAPSTER_INTERNAL_F_H

#include "internal.h"
#include "stats.h"

void *block_to_payload(block_header_t *block);
block_header_t *payload_to_block(void *payload);
block_header_t *policy_find_block(arena_t *arena, size_t size);
void arena_stats_reset(arena_t *arena);
void heapster_stats_update_global(heapster_stats_t *stats);
static inline void arena_stats_print(const arena_t *arena);

#endif