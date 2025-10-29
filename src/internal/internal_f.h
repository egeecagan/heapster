// public api ile disari acmak istemedigim fonksiyonlar burda olcak

#ifndef HEAPSTER_INTERNAL_F_H
#define HEAPSTER_INTERNAL_F_H

#include "internal.h"
#include "stats.h"

//block.c
block_header_t *block_init(void *addr, size_t arena_size);
void *block_to_payload(block_header_t *block);
block_header_t *payload_to_block(void *payload);
void block_dump_free_list(arena_t *arena);
void block_remove_from_free_list(arena_t *arena, block_header_t *block);
block_header_t *block_split(arena_t *arena, block_header_t *block, size_t size);
block_header_t *block_coalesce(arena_t *arena, block_header_t *block);
int block_validate(block_header_t *block);

//arena.c
arena_t *arena_create(size_t size);
arena_t *arena_get_list(void);
int last_cleanup(void);
block_header_t *arena_find_free_block(arena_t *arena, size_t size);
void arena_destroy(arena_t *arena);

//stats.c
void arena_stats_reset(arena_t *arena);
void heapster_stats_update_global(heapster_stats_t *stats);

// policy.c
block_header_t *policy_find_block(arena_t *arena, size_t size);

void arena_dump(arena_t *arena);

#endif // end of HEAPSTER_INTERNAL_F_H