#include "internal.h"
#include "internal_f.h"
#include "heapster.h"

#include <stdbool.h>

static inline bool block_is_in_free_list(arena_t *arena, block_header_t *b) {
    if (!arena || !b || b->free != 1) {
        return false;
    }

    block_header_t *curr = arena->free_list_head;
    while (curr) {
        if (curr == b) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}

/*
bu fonksiyonu hangi arena icin cagirdiysak cagirma isleminden sonra ilk yapilmasi gereken sey
block->arena_id yi set etmek olmalidir

parametre olan adres alligned olabilir olmayadabilir ona gore onlem al tum program boyunca
anladigim kadariyla header ici alignment sarj degil ama payload allign olmak zorunda

bu functiona gelen adres cagiran tarafindan ALIGNMENT 'a gore hizzali gelmek zorundadir yoksa
alignment karsiti hareket performans kaybina yol acar
*/
block_header_t *block_init(void *addr, size_t total_block_size) {

    // block icin verilen tum size block'un overhead + alignli header + min payload icermeli
    if (!addr || total_block_size < BLOCK_MIN_SIZE) {
        return NULL;
    }
                         
    block_header_t *block = (block_header_t *)addr;

    block->size = total_block_size - BLOCK_HEADER_SIZE;  // sadece payload boyutu
    block->requested_size = 0;
    block->free = 1;

    block->next = NULL;
    block->prev = NULL;

    block->phys_prev = NULL;
    block->phys_next = NULL;
    block->magic = CTRL_CHR;
    block->arena_id = -1;

    return block;
}

/* insert the block into the free list of the given arena in an address ordered manner */
void block_add_to_free_list(arena_t *arena, block_header_t *block) {
    
    if (!arena || !block) {
        return;
    }

    if (block_is_in_free_list(arena, block)) {
        return;
    }

    block->free = 1;
    block->requested_size = 0;

    block_header_t *current = arena->free_list_head;
    block_header_t *prev = NULL;

    while (current && current < block) {
        prev = current;
        current = current->next;
    }

    block->next = current;
    block->prev = prev;

    if (current) {
        current->prev = block;
    }

    if (prev) {
        prev->next = block;
    } else {
        arena->free_list_head = block;
    }
    
    if (block->arena_id != arena->id) {
        block->arena_id = arena->id;
    }

}

/* remove a block from the free list */
void block_remove_from_free_list(arena_t *arena, block_header_t *block) {
    if (!arena || !block) {
        return;
    }
    
    if (!block_is_in_free_list(arena, block)) {
        return;
    }

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        arena->free_list_head = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    if (arena->next_fit_cursor == block) {
        arena->next_fit_cursor = arena->free_list_head ? arena->free_list_head : NULL;
    }

    block->next = NULL;
    block->prev = NULL;
}

/* 
split: allocate leading part, keep trailing remainder free and on the free list 
size parametresi allocate edilen on parcanin payload miktari
*/
block_header_t *block_split(arena_t *arena, block_header_t *block, size_t size) {
    if (!arena || !block || block->free == 0) {
        return NULL;
    }
    
    // yukari yuvarlama 100 ise 112'ye yuvarlanir (alignment 16 ise) ve bu size allocate olcak free = 0 preceding block olarak
    size_t aligned_size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

    if (block->size <= aligned_size + BLOCK_MIN_SIZE) {  // suan ki block paylaod kendisi icin miktar + header + kalan block kadar yer icermeli
        return NULL;
    }

    if (block->free == 1 && block_is_in_free_list(arena, block)) {
        block_remove_from_free_list(arena, block);
    }

    void *new_addr = (char *)block + BLOCK_HEADER_SIZE + size;
    block_header_t *new_block = (block_header_t *)new_addr;

    // trailing free parca
    new_block->size = block->size - (aligned_size + BLOCK_HEADER_SIZE);
    new_block->free = 1;
    new_block->requested_size = 0;

    new_block->next = NULL;
    new_block->prev = NULL;
    // fiziksel zincir
    new_block->phys_prev = block;
    new_block->phys_next = block->phys_next;
    if (new_block->phys_next) {
        new_block->phys_next->phys_prev = new_block;
    }

    new_block->magic = block->magic;
    new_block->arena_id = block->arena_id;

    // allocate edilen on parca
    block->size = aligned_size;
    block->free = 0;
    block->requested_size = 0;  // malloc tarafinda gercek requested_size set edilmeli
    block->phys_next = new_block;

    // trailing free parca free list'e adres sirasina gore eklenir
    block_add_to_free_list(arena, new_block);

    return block; // not: caller genelde allocate edilen 'block' ile ilgilenir
}

/* coalesce: merge with left and then as many rights as possible; returns leftmost merged free block */
block_header_t *block_coalesce(arena_t *arena, block_header_t *block) {
    if (!arena || !block || block->free != 1) {
        return NULL;
    }

    if (block->phys_prev && block->phys_prev->free == 1) {
        block_header_t *prev = block->phys_prev;

        if (block_is_in_free_list(arena, block)) {
            block_remove_from_free_list(arena, block);
        }

        prev->size += BLOCK_HEADER_SIZE + block->size;
        prev->requested_size = 0;

        prev->phys_next = block->phys_next;
        if (prev->phys_next) {
            prev->phys_next->phys_prev = prev;
        }

        block = prev; 
    }

    while (block->phys_next && block->phys_next->free == 1) {
        block_header_t *next = block->phys_next;

        if (block_is_in_free_list(arena, next)) {
            block_remove_from_free_list(arena, next);
        }

        block->size += BLOCK_HEADER_SIZE + next->size;
        block->requested_size = 0;

        block->phys_next = next->phys_next;
        if (block->phys_next) {
            block->phys_next->phys_prev = block;
        }
    }

    block_add_to_free_list(arena, block);

    return block; 
}

/* get pointer to user payload from a block header */
void *block_to_payload(block_header_t *block) {
    if (!block || block->size == 0) {
        return NULL;
    }
    return (void *)((char *)block + BLOCK_HEADER_SIZE);
}

/* get block header from user payload pointer */
block_header_t *payload_to_block(void *payload) {
    if (!payload) {
        return NULL;
    }
    return (block_header_t *)((char *)payload - BLOCK_HEADER_SIZE);
}

/* check if a block is valid (magic number, alignment, etc.) */
int block_validate(block_header_t *block) {
    if (!block) {
        return -1;
    }

    if (block->magic != CTRL_CHR) {
        return -2;
    }

    void *payload = block_to_payload(block);

    if (((uintptr_t)payload % ALIGNMENT) != 0)
        return -3;

    if (block->size < BLOCK_MIN_SIZE)
        return -4;

    if (block->free != 0 && block->free != 1)
        return -5;

    return 1;
}

/* print free list */
void block_dump_free_list(arena_t *arena) {
    if (!arena || !arena->free_list_head) {
        printf("[heapster] free list empty\n");
        return;
    }

    printf("[heapster] Free list for arena %d:\n", arena->id);

    block_header_t *curr = arena->free_list_head;
    int index = 0;

    while (curr) {
        printf("  [%d] block=%p size=%zu free=%d\n",
               index, (void *)curr, curr->size, curr->free);

        printf("       free_list: prev=%p next=%p\n",
               (void *)curr->prev, (void *)curr->next);

        printf("       physical : prev=%p next=%p\n",
               (void *)curr->phys_prev, (void *)curr->phys_next);

        curr = curr->next;
        index++;
    }
}
