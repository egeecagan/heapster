#include "internal.h"
#define CTRL_CHR 0xC0FFEE // kahvesiz kod olmaz kral.
#define ALIGNMENT 8

block_header_t *block_init(void *addr, size_t size) {
    if (!addr || size < sizeof(block_header_t)) {
        return NULL;
    }

    block_header_t *block = (block_header_t *)addr;

    // henuz istenen size yok sadece block headerdan olusuyor
    block->size = size - sizeof(block_header_t);

    // sadece arena basina verilcek kullaniciyla iliskisi yok ondan istenmedi henuz
    block->requested_size = 0;

    block->free = 1;

    block->next = NULL;
    block->prev = NULL;

    block->phys_prev = NULL;
    block->phys_next = NULL;

    block->magic = CTRL_CHR;
    block->arena_id = 0;

    return block;
}

// if the block is larger than the wanted size aligned this f will split the block
block_header_t *block_split(block_header_t *block, size_t size) {
    if (!block || block->free == 0) {
        return NULL;
    }

    size = (size + ALIGNMENT) & ~ALIGNMENT;

    if (block->size < size + sizeof(block_header_t) + ALIGNMENT) {
        return NULL;
    }

    void *new_addr = (char *)block + sizeof(block_header_t) + size;
    block_header_t *new_block = (block_header_t *)new_addr;

    // new block kullanicinin istedigi block degil ayrimda ayrilan block yani arkada kalan free = 1 olan
    new_block->size = block->size - size - sizeof(block_header_t);
    new_block->free = 1;
    new_block->requested_size = 0; // free ya kullanici henuz istemedi ondan requested size = 0 

    // todo : henuz free liste eklemedik o liste ekleme fonksiyonunda baglicaz unutma 
    new_block->next = NULL;
    new_block->prev = NULL;

    new_block->phys_prev = block;
    new_block->phys_next = block->phys_next;

    new_block->magic = block->magic;     
    new_block->arena_id = block->arena_id;
    
    block->size = size;
    block->free = 0;
    
    block->phys_next = new_block;

    return new_block;

}

/* merge block with its next physical neighbor if free ilk bastakine pointer */
block_header_t *block_coalesce(block_header_t *block) {
    if (!block || block->free == 0) {
        return NULL;
    }

    if (block->next && block->next->free == 1) {
        block_header_t *new_start = block;

        new_start->free = 1;
        new_start->size = block->size + block->next->size;

        // requested size artık anlamlı degil zaten free ya
        new_start->requested_size = 0;

        // fiziksel baglantılar
        new_start->phys_next = block->next->phys_next;
        if (new_start->phys_next) {
            new_start->phys_next->phys_prev = new_start;
        }

        // free list baglantıları doubly oldugu icin sikintili
        new_start->next = block->next->next;
        if (new_start->next) {
            new_start->next->prev = new_start;
        }

        new_start->prev = block->prev;
        if (new_start->prev) {
            new_start->prev->next = new_start;
        }

        // diger alanlar
        new_start->magic = block->magic;
        new_start->arena_id = block->arena_id;

        return new_start;
    }

    return block;
}


/* insert a block into the free list */
void block_add_to_free_list(arena_t *arena, block_header_t *block) {
    if (!arena || !block) return;

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
}

/* remove a block from the free list */
void block_remove_from_free_list(arena_t *arena, block_header_t *block) {
    if (!arena || !block) return;

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        arena->free_list_head = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    block->next = NULL;
    block->prev = NULL;
}

/* get pointer to user payload from a block header */
static inline void *block_to_payload(block_header_t *block) {
    if (!block || block->size == 0) {
        return NULL;
    } 
    return (void *) ((char *)block + sizeof(block_header_t)) ;
}

/* get block header from user payload pointer */
static inline block_header_t *payload_to_block(void *payload) {
    if (!payload) {
        return NULL;
    }
    return (block_header_t *)((char *)payload - sizeof(block_header_t));
}

/* check if a block is valid (magic number, alignment, etc.) */
int block_validate(block_header_t *block) {
    if (!block) return -1;

    if (block->magic != CTRL_CHR)
        return -2;

    void *payload = block_to_payload(block);

    if (((uintptr_t)payload % block->alignment) != 0)
        return -3;

    if (block->size < BLOCK_MIN_SIZE)
        return -4;

    if (block->free != 0 && block->free != 1)
        return -5;

    return 1;
}

/* print current free list */
void block_dump_free_list(arena_t *arena) {
    // pass
}
