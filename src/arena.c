#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "internal.h"
#include "heapster.h"
#include "internal_f.h"

// inside the source code user can access the arenas with arena_get_list() method
static arena_t *arena_list_head = NULL; 


pthread_mutex_t arena_list_lock = PTHREAD_MUTEX_INITIALIZER;

// if user wanted size is less then this use sbrk to increase end of heap.
static size_t mmap_threshold = 128 * 1024; 
/*
arena create'nin parametresi size bu sizedan 
az ise sbrk fazla ya da esit ise mmap kullanilir
getter ve setter methodlar ile disaridan erisilebilir
*/

static uint64_t arena_id_counter = 1;  
/* 
tek amacım her arenaya farklı id gitmesidir silinen 
arena icin totalden idleri degistirmek gibi bir 
amacım yok ondan sadece increment ve bence int (
8 byte) bunun icin yeterli bir miktar 2^64 farkli 
deger yapar
*/

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

/* 
bu fonksiyon arena icin gerekli adres baslangicini alir ve arena_header
block_header i olusturup bu adresin basina sirasiyla koyar. c de struct
fieldlari pointerda adrese sirasiyla konur yani arena pointerina cevir-
dikten sonra p->field dedigin an direk adresin basina koyar ve oyle devam
eder. 
*/
arena_t *arena_init(void *addr, size_t size, int use_mmap) {
    uintptr_t raw      = (uintptr_t)addr + ARENA_HEADER_SIZE; 
    uintptr_t aligned  = (raw + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); 
    
    size_t overhead   = aligned - (uintptr_t)addr; // arena_header_size + padding degeridir
    /*
    addr mmap den page aligned ve alignof(max_align_t) ikisinede bolunecek sekilde gelir garanti sekilde
    addr a arena_header_size ekledik bu header size da alignof(max_align_t) a gore align bir degerdir
    [a mod n == 0 && b mod n == 0 -> (a + b) mod n == 0] bu kurali kullanarak aslinda aligned == raw oldugunu
    anlariz cunku zaten raw aligned bir deger bir daha align etmek bir sey degistirmez burda altta hesaplanan
    overhead de bize sadece arena_header_size i geri verir aslinda
    */
    if (raw != aligned) {
        fprintf(stderr, "[fatal error] misaligned memory start in arena_init padding needed: %zu bytes.\n", (size_t)(aligned - raw));
    }

    // bizde memory layout soyle
    // |arena header| overhead (alignment kaybi) | block header | block payload ... |
    // |<---------------------------------- size ---------------------------------->|
    // |arena start|                                                      |arena_end|

    if (!addr || size < overhead + BLOCK_MIN_SIZE) {
        fprintf(stderr, "[fatal error] very small size argument passed to the arena_init");
        return NULL;  
    }

    arena_t *arena = (arena_t *)addr;

    arena->id = arena_id_counter;
    pthread_mutex_init(&arena->lock, NULL);

    arena->start = (char *)addr;
    arena->end   = (char *)addr + size;
    arena->size  = size;

    arena->free_list_head = NULL;
    arena->next_fit_cursor = NULL;
    arena->is_mmap = use_mmap;

    arena_stats_reset(arena);

    void *block_addr   = (void *)aligned;
    // ilk block headerinin baslayacagi adres

    // mmap ile aldigimiz size - (arena header + padding) // yani blcok icin kalan header ve payload alani
    size_t total_block_size  = size - (aligned - (uintptr_t)addr);

    // block_init ile olusan blockta alignment ile ugrasmamak icin assagiya yuvarlanir
    size_t aligned_total_block_size = total_block_size & ~(ALIGNMENT - 1); 

    size_t loss = total_block_size - aligned_total_block_size; // either 0 or bigger

    block_header_t *first_block = block_init(block_addr, aligned_total_block_size);
    arena->block_count = 1;
    if (!first_block) {
        return NULL;
    }

    first_block->arena_id = arena_id_counter++;

    arena->free_list_head   = first_block;
    arena->next_fit_cursor  = first_block;

    arena->stats.total_bytes = size;
    arena->stats.free_bytes  = first_block->size;
    arena->stats.largest_free_block = first_block->size;
    arena->stats.free_block_count = 1;
    arena->stats.allocated_block_count = 0;
    
    return arena;
}

/* 
*   size =  (kullanicinin istedigi payload size i up align edilmis) + 
*           (block_header_size up align edilmis)                    + 
*           (arena_header_size up align edilmis)
*/
arena_t *arena_create(size_t size) {

    size_t page_size = sysconf(_SC_PAGE_SIZE);  
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1); // kernelden alinan miktar gercek yukari align edilcek page size a gore

    void *addr = NULL;
    arena_t *arena = NULL;
    // kullanici mmap den size ister ama mmap page aligned bir adres ve page size in kati olacak sekilde adres verir
    // ama bu verilen adres zaten oldugun sistemde alignof(max_align_t) bunun align istegini karsilar
    if (size >= heapster_get_mmap_threshold()) {
        addr = mmap(NULL, alloc_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1, 0);

        // adresin basindan ilk olarak arena header gelcek sonra block heaeder gelcek sonra block payload ...
        if (addr == MAP_FAILED) {
            return NULL;
        }
        arena = arena_init(addr, alloc_size, 1); // girilen adres page aligned bir adres ayni zamanda alignof(max_align_t) aligned
    } else {

    // kaba minimum (overhead burada hesaplanamaz cunku addr daha alınmadı)
        size_t min_size = ARENA_HEADER_SIZE + BLOCK_MIN_SIZE;
        if (size < min_size) {
            size = min_size;
        }

        void *cur = sbrk(0);
        if (sbrk(size) == (void *)-1) {
            return NULL;
        }
        addr = cur;
        arena = arena_init(addr, size, 0);
    }

    if (!arena) {
        return NULL;
    } 

    arena->requested_size = size; // runtime icin anlami yok ama debug da ise yarar
    
    pthread_mutex_lock(&arena_list_lock);
    arena->next = arena_list_head;  
    arena_list_head = arena;
    pthread_mutex_unlock(&arena_list_lock);

    return arena;
}

// arena header haric tum data si 0 ile degisir (silinir) ve arena header sonrasi block header ekler tek buyuk block
void arena_clear(arena_t *arena) {
    if (!arena) {
        return;
    }

    pthread_mutex_lock(&arena->lock);

    void *clear_start = (char *)arena + ARENA_HEADER_SIZE;
    size_t clear_size = arena->size - ARENA_HEADER_SIZE;

    arena->free_list_head = NULL;
    arena->next_fit_cursor = NULL;
    arena_stats_reset(arena);

    memset(clear_start, 0, clear_size);

    uintptr_t raw     = (uintptr_t)arena + ARENA_HEADER_SIZE;
    uintptr_t aligned = (raw + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

    void *block_addr = (void *)aligned;
    size_t total_block_size = arena->size - (aligned - (uintptr_t)arena);
    size_t aligned_total_block_size = total_block_size & ~(ALIGNMENT - 1);

    block_header_t *first_block = block_init(block_addr, aligned_total_block_size);
    if (first_block) {
        first_block->arena_id = arena->id;
        arena->free_list_head = first_block;
        arena->next_fit_cursor = first_block;
        arena->block_count = 1;

        arena->stats.total_bytes = arena->size;
        arena->stats.free_bytes = first_block->size;
        arena->stats.largest_free_block = first_block->size;
        arena->stats.free_block_count = 1;
        arena->stats.allocated_block_count = 0; // Bu zaten reset ile 0'lanmış olabilir, ancak açıkça belirtmek güvenlidir.

    }

    pthread_mutex_unlock(&arena->lock);
}

void arena_destroy(arena_t *arena) {
    if (!arena) return;

    if (arena->is_mmap) {
        pthread_mutex_lock(&arena_list_lock);
        if (arena_list_head == arena) {
            arena_list_head = arena->next;
        } else {
            arena_t *prev = arena_list_head;
            while (prev && prev->next != arena) prev = prev->next;
            if (prev) prev->next = arena->next;
        }
        pthread_mutex_unlock(&arena_list_lock);

        pthread_mutex_destroy(&arena->lock);
        munmap(arena, arena->size);
        return;
    }

    uintptr_t diff = (uintptr_t)sbrk(0) - (uintptr_t)arena->end;

    if (diff == 0) {
        pthread_mutex_lock(&arena_list_lock);
        if (arena_list_head == arena) {
            arena_list_head = arena->next;
        } else {
            arena_t *prev = arena_list_head;
            while (prev && prev->next != arena) prev = prev->next;
            if (prev) prev->next = arena->next;
        }
        pthread_mutex_unlock(&arena_list_lock);

        pthread_mutex_destroy(&arena->lock);
        sbrk(-arena->size);
    } else {
        pthread_mutex_lock(&arena->lock);
        arena_clear(arena); 
        pthread_mutex_unlock(&arena->lock);
    }
}

// parametre olan size block icin olan payload size'i
block_header_t *arena_find_free_block(arena_t *arena, size_t block_payload_size) {
    if (!arena || block_payload_size == 0) {
        return NULL;
    }

    pthread_mutex_lock(&arena->lock);

    block_header_t* b =  policy_find_block(arena, block_payload_size); 

    pthread_mutex_unlock(&arena->lock);

    return b;
}

void arena_dump(arena_t *arena) {
    if (!arena) {
        printf("[heapster] arena is NULL\n");
        return;
    }

    // Kilit yönetimini ekleyin
    pthread_mutex_lock(&arena->lock); 

    printf("===== arena %llu =====\n", (unsigned long long)arena->id);
    printf("start addr        : %p\n", arena->start);
    printf("end addr          : %p\n", arena->end);
    printf("total arena size  : %zu bytes\n", arena->size);
    printf("free list head    : %p\n", (void *)arena->free_list_head);
    printf("next fit cursor   : %p\n", (void *)arena->next_fit_cursor);
    
    // *********************************************************
    // İSTATİSTİK RAPORLAMA VE FRAGMENTASYON HESAPLAMASI (EKLENDİ)
    // *********************************************************
    double fragmentation_ratio = 0.0;
    if (arena->stats.free_bytes > 0) {
        fragmentation_ratio = 1.0 - ((double)arena->stats.largest_free_block / arena->stats.free_bytes);
    }

    printf("\n--- stats ---\n");
    printf("total bytes    : %zu\n", arena->stats.total_bytes);
    printf("used bytes     : %zu\n", arena->stats.used_bytes);
    printf("free bytes     : %zu\n", arena->stats.free_bytes);
    printf("allocated blocks: %zu\n", arena->stats.allocated_block_count);
    printf("free blocks    : %zu\n", arena->stats.free_block_count);
    printf("largest free blk: %zu\n", arena->stats.largest_free_block);
    printf("wasted (internal) : %zu\n", arena->stats.wasted_bytes);
    printf("fragmentation ratio: %.4f\n", fragmentation_ratio);
    
    printf("malloc calls   : %llu\n", (unsigned long long)arena->stats.malloc_calls);
    printf("free calls     : %llu\n", (unsigned long long)arena->stats.free_calls);
    printf("realloc calls  : %llu\n", (unsigned long long)arena->stats.realloc_calls);
    printf("calloc calls   : %llu\n", (unsigned long long)arena->stats.calloc_calls);
    
    // Blok serbest listesini dök
    block_dump_free_list(arena);

    // Kilit yönetimini ekleyin
    pthread_mutex_unlock(&arena->lock); 

    printf("====================\n\n");
}

arena_t *arena_get_list(void) {
    return arena_list_head;
}

int last_cleanup(void) {
    arena_t *cur = arena_list_head;
    while (cur) {
        arena_t *next = cur->next;
        arena_destroy(cur);
        cur = next;
    }

    arena_list_head = NULL; // tüm arenalar yok edildi
    return 0;
}

void heapster_status(void) {

    printf("arena stats explanation: \n");

    pthread_mutex_lock(&arena_list_lock); 

    if (!arena_list_head) {
        fprintf(stderr, "[fatal error] arena list head is NULL\n"); 
        pthread_mutex_unlock(&arena_list_lock); 
        return;
    } 

    arena_t *head = arena_list_head;
    while (head) {
        arena_dump(head);
        // arena_stats_print(head); // BU SATIRI KALDIRIN
        head = head->next;
    }

    pthread_mutex_unlock(&arena_list_lock); 
}