#include <sys/mman.h>
#include <unistd.h>

#include "internal.h"
#include "heapster.h"
#include "internal_f.h"

static arena_t *arena_list_head = NULL; // program ici olan tum arenalarin bastan sona linked list hali getter/setter ile ulasilir
pthread_mutex_t arena_list_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t mmap_threshold = 128 * 1024; // arena create bu sizedan az ise sbrk fazla ya da esit ise mmap

static int arena_id_counter = 1;  

/* 
tek amacım her arenaya farklı id gitmesi silinen 
arena için totalden idleri değiştirmek gibi bir 
amacım yok ondan sadece increment ve bence int (
4 byte) bunun icin yeterli bir miktar 2^32 farkli 
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

// burda size paremetresi bizim backendde mmap ya da sbrk sonucu aldigimiz size miktari kullanicinin istedigi degil (alloc_size)
arena_t *arena_init(void *addr, size_t size, int use_mmap) {
// bu size a arena header block header her sey dahil
    uintptr_t raw      = (uintptr_t)addr + ARENA_HEADER_SIZE; // mmap dondugu adres + arena header
    uintptr_t aligned  = (raw + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    // aligned = arenanin payload kisminin baslangic adresi gibi dusun

    size_t overhead   = aligned - (uintptr_t)addr; // bu farki alignment yaptigimiz icin kaybederiz

    if (!addr || size < overhead + BLOCK_HEADER_SIZE) {
        return NULL;  // bastaki kontrole ekstra aligned sekilde kontrol ekledim 
        // sizeof(arena_t) aligned a uymayan bir adres olmayabilir ama overhead uyar
    }

    // aklima takilan soru su block header size hem alignmenta uyan bir deger olucak da sizeof(arena_t) bu olmazsa

    arena_t *arena = (arena_t *)addr;

    arena->id = arena_id_counter;
    pthread_mutex_init(&arena->lock, NULL); // bunun sayesinde arena struct i ici lcok u kullanilabilir hale getiririz

    arena->start = (char *)addr;
    arena->end   = (char *)addr + size;
    arena->size  = size;

    arena->free_list_head = NULL;
    arena->next_fit_cursor = NULL;
    arena->is_mmap = use_mmap;

    // c de adresi cast ettikten sonra fieldlari koymaya basladigimiz an adrese sirasiyla konur rastgele gitmez daha guzel aciklanabilir ama benden bu kadar

    arena_stats_reset(arena);

    // bizde memory layout soyle
    // |arena header| overhead (alignment kaybi) | block header | block payload ... |
    // |<--------- size --------->|

    void *block_addr   = (void *)aligned;
    // ilk blogun baslayacagi adres (headerinin)

    // mmap ile aldigimiz size - arada kalan miktar cope gider alignmentta
    size_t block_size  = size - (aligned - (uintptr_t)addr);

    block_header_t *first_block = block_init(block_addr, block_size);
    if (!first_block) {
        return NULL;
    }

    first_block->arena_id = arena_id_counter++;

    arena->free_list_head   = first_block;
    arena->next_fit_cursor  = first_block;
    
    return arena;
}

// parametre olan size kullanicinin istedigi miktar olucak burda
arena_t *arena_create(size_t size) {

    size_t page_size = sysconf(_SC_PAGE_SIZE);  // bende 4096 * 4 cikiyor mmap bunun kati kadar verir hep ve adres page aligned olur galiba
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1); // kernelden alinan miktar gercek

    void *addr = NULL;
    arena_t *arena = NULL;
    // kullanici mmap den size ister ama mmap page aligned bir adres ve page size in kati olacak sekilde adres verir
    // misal kullanici 4070 byte istedi mmap page size 4096 ise 4096 verir ve bunu aligned bir adres olarak verir (%99)
    if (size >= heapster_get_mmap_threshold()) {
        addr = mmap(NULL, alloc_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1, 0);

        // adresin basindan ilk olarak arena header gelcek sonra block heaeder gelcek sonra block payload ...
        if (addr == MAP_FAILED) {
            return NULL;
        }
        arena = arena_init(addr, alloc_size, 1);
    } else {
    // kaba minimum (overhead burada hesaplanamaz, çünkü addr daha alınmadı)
        size_t min_size = ARENA_HEADER_SIZE + BLOCK_HEADER_SIZE + ALIGNMENT;
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


void arena_destroy(arena_t *arena) {
    if (!arena) return;

    pthread_mutex_lock(&arena_list_lock);

    if (arena_list_head == arena) {
        arena_list_head = arena->next;
    } else {
        arena_t *prev = arena_list_head;
        while (prev && prev->next != arena) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = arena->next;
        }
    }

    pthread_mutex_unlock(&arena_list_lock);

    pthread_mutex_destroy(&arena->lock);

    if (arena->is_mmap) {
        munmap(arena, arena->size);
    } else {
        // sbrk shrink logic buraya gelecek
    }
}

// payload size i parametre burda
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

arena_t *arena_get_list(void) {
    return arena_list_head; // dikkat: disarida lock kullanman gerekebilir
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
