// policy.c
#include <stddef.h>
#include <stdint.h> // uintptr_t icin gerekli 

#include "internal.h"
#include "internal_f.h"
#include "heapster.h"

/*
Ne ise yarar bu dosya?
Kullanici diyelim ki 100 birimlik bir alan istedi.
Kernel once arena bulur (burasi arena bulmakla ilgili degil).
Bu dosya bulunan arenadaki free list içinde hangi blogun seçilecegine karar verir.
Her strateji farkli seçim yontemidir:

first fit   -> ilk sigan block'a yerlesir
next fit    -> en son bakilan block’tan devam eder, gerekirse basa sarar
best fit    -> uygun olan en küçük block’u seçer
worst fit   -> uygun olan en büyük block’u seçer

yoksa null döner ona göre farklı arena denenir ya da yeni arena açılır

fonksiyonlarin aldigi size paremetresi sadece payload size i header dahil degil

normal durumda her zaman blockun block is aligned fonksiyonundan gecmesi lazim cunku
diger kisimlar da alignment hep korunuyor ama split ya da coalescing esnasinda bozulabilir
yani galiba
*/

// hizali mi kontrolü (her payload ALIGNMENT’e gore)
static inline int block_is_aligned(block_header_t *b) {
    if (!b) return 0;
    uintptr_t addr = (uintptr_t)block_to_payload(b);
    return (addr % ALIGNMENT) == 0;
}

/* first fit */
static block_header_t *find_first_fit(arena_t *arena, size_t size) {
    for (block_header_t *cur = arena->free_list_head; cur; cur = cur->next) {
        if (cur->free && cur->size >= size && block_is_aligned(cur)) {
            return cur;
        }
    }
    return NULL;
}

/* next fit */
static block_header_t *find_next_fit(arena_t *arena, size_t size) {
    if (!arena->next_fit_cursor) {
        arena->next_fit_cursor = arena->free_list_head;
    }

    if (!arena->next_fit_cursor) {
        return NULL; // bos arena
    }
    
    block_header_t *start = arena->next_fit_cursor;
    block_header_t *cur   = start;

    do {
        if (cur->free && cur->size >= size && block_is_aligned(cur)) {
            arena->next_fit_cursor = cur->next ? cur->next : arena->free_list_head;
            return cur;
        }

        cur = cur->next ? cur->next : arena->free_list_head;

    } while (cur && cur != start);

    return NULL;
}

/* best fit */
static block_header_t *find_best_fit(arena_t *arena, size_t size) {
    block_header_t *best = NULL;
    size_t best_size = (size_t)-1;

    for (block_header_t *cur = arena->free_list_head; cur; cur = cur->next) {
        if (cur->free && cur->size >= size && block_is_aligned(cur)) {
            if (cur->size < best_size) {
                best_size = cur->size;
                best = cur;
            }
        }
    }
    return best;
}

/* worst fit */
static block_header_t *find_worst_fit(arena_t *arena, size_t size) {
    block_header_t *worst = NULL;
    size_t worst_size = 0;

    for (block_header_t *cur = arena->free_list_head; cur; cur = cur->next) {
        if (cur->free && cur->size >= size && block_is_aligned(cur)) {
            if (cur->size > worst_size) {
                worst_size = cur->size;
                worst = cur;
            }
        }
    }
    return worst;
}

block_header_t *policy_find_block(arena_t *arena, size_t size) {
    if (!arena) {
        return NULL;
    }

    switch (heapster_get_policy()) {
        case HEAPSTER_FIRST_FIT:  
            return find_first_fit(arena, size);

        case HEAPSTER_NEXT_FIT:   
            return find_next_fit(arena, size);
            
        case HEAPSTER_BEST_FIT:   
            return find_best_fit(arena, size);

        case HEAPSTER_WORST_FIT:  
            return find_worst_fit(arena, size);

        default: 
            return find_first_fit(arena, size);
    }
}
