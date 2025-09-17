#include "internal.h"
#include "heapster.h"
#include <sys/mman.h>

static size_t mmap_threshold = 128 * 1024; // 128 kb

void heapster_set_mmap_threshold(size_t bytes) {
    if (bytes < 4096) {
        mmap_threshold = 4096;
    } else {
        mmap_threshold = bytes;
    }
}

size_t heapster_get_mmap_threshold(void) {
    return mmap_threshold;
}

// size kullanicinin malloc ile isttedigi sey degil mmap ya da sbrk nin bize verdigi miktar
arena_t *arena_init(void *addr, size_t size, int id, int use_mmap) {
    if (!addr || size < sizeof(arena_t) + BLOCK_HEADER_SIZE) {
        return NULL;
    }

    arena_t *arena = (arena_t *)addr;

    arena->id = id;
    pthread_mutex_init(&arena->lock, NULL); // bunun sayesinde arena struct i ici lcok u kullanilabilir hale getiririz

    arena->start = (char *)addr;
    arena->end   = (char *)addr + size;
    arena->size  = size;

    arena->free_list_head = NULL;
    arena->next_fit_cursor = NULL;
    arena->is_mmap = use_mmap;

    heapster_stats_reset(&arena->stats);

    uintptr_t raw      = (uintptr_t)addr + sizeof(arena_t);
    uintptr_t aligned  = (raw + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    // aligned arenanin payload kisminin baslangic adresi gibi dusun

    void *block_addr   = (void *)aligned;
    // ilk blogun baslayacagi adres (headerinin)

    // mmap ile aldigimiz size - arada kalan miktar cope gider alignmentta
    size_t block_size  = size - (aligned - (uintptr_t)addr);

    block_header_t *first_block = block_init(block_addr, block_size);
    if (!first_block) {
        return NULL;
    }

    first_block->arena_id = id;

    arena->free_list_head   = first_block;
    arena->next_fit_cursor  = first_block;

    return arena;
}

arena_t *arena_create(size_t size, int id) {
    void *addr = NULL;

    if (size >= mmap_threshold) {
        addr = mmap(NULL, size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1, 0);
        if (addr == MAP_FAILED) {  // aslinda buda bir (void *)-1 bunun sebebi bellekte baya sona yakin bir isaretci olmasi
            return NULL;
        }
        return arena_init(addr, size, id, 1);
    } else {
        void *cur = sbrk(0);
        if (sbrk(size) == (void *)-1) {
            return NULL;
        }
        addr = cur;
        return arena_init(addr, size, id, 0);
    }

    
}

void arena_destroy(arena_t *arena) {
    if (!arena) return;

    pthread_mutex_destroy(&arena->lock);

    if (arena->is_mmap) {
        munmap(arena, arena->size);
    } else {
        /*
            sbrk normalde heap sinirini genisletir ama bu teoride riskli deniyor cunku
            sadece geri alma islemi o sinir bos ise gerceklesir yoksa kalir. 

            buraya 0 dan bir algoritma olusturmam lazim suan degil
        */
    }
}

block_header_t *arena_find_free_block(arena_t *arena, size_t size) {
    if (!arena || size == 0) {
        return NULL;
    }

    pthread_mutex_lock(&arena->lock);

    block_header_t* b =  policy_find_block(arena, size); // null donerse yeni arena olusturmasal minimum sararsal

    pthread_mutex_unlock(&arena->lock);

    return b;
}

void arena_dump(arena_t *arena) {
    if (!arena) {
        printf("[heapster] arena is NULL\n");
        return;
    }

    pthread_mutex_lock(&arena->lock);

    printf("===== arena %d =====\n", arena->id);
    printf("start addr     : %p\n", arena->start);
    printf("end addr       : %p\n", arena->end);
    printf("total size     : %zu bytes\n", arena->size);
    printf("free list head : %p\n", (void *)arena->free_list_head);
    printf("next fit cursor: %p\n", (void *)arena->next_fit_cursor);

    printf("--- stats ---\n");
    printf("used bytes     : %zu\n", arena->stats.used_bytes);
    printf("free bytes     : %zu\n", arena->stats.free_bytes);
    printf("alloc calls    : %llu\n", (unsigned long long)arena->stats.alloc_calls);
    printf("free calls     : %llu\n", (unsigned long long)arena->stats.free_calls);

    block_dump_free_list(arena);

    pthread_mutex_unlock(&arena->lock);

    printf("====================\n\n");
}

