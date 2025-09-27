
#include "heapster.h"
#include "internal.h"
#include "internal_f.h"

/*
main function definitions of public api
*/

static heapster_policy_t current_policy = HEAPSTER_FIRST_FIT;
static pthread_mutex_t policy_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t default_arena_size = 128 * 1024; // 128 KB 

void heapster_set_policy(heapster_policy_t policy) {
    pthread_mutex_lock(&policy_lock);
    current_policy = policy;
    pthread_mutex_unlock(&policy_lock);
}

heapster_policy_t heapster_get_policy(void) {
    pthread_mutex_lock(&policy_lock);
    heapster_policy_t policy = current_policy;
    pthread_mutex_unlock(&policy_lock);
    return policy;
}

int heapster_init(size_t arena_size, heapster_policy_t policy) {
    size_t min_size = sizeof(arena_t) + BLOCK_HEADER_SIZE + ALIGNMENT;

    if (arena_size < min_size) {
        fprintf(stderr, "[heapster] arena_size too small, using minimum %zu\n", min_size);
        arena_size = min_size;
    }

    default_arena_size = arena_size;
    heapster_set_policy(policy);

    arena_t *arena = arena_create(default_arena_size);
    return arena ? 0 : -1;
}

int heapster_finalize(void) {
    pthread_mutex_lock(&arena_list_lock);

    last_cleanup();

    pthread_mutex_unlock(&arena_list_lock);

    pthread_mutex_destroy(&policy_lock);
    pthread_mutex_destroy(&arena_list_lock);

    return 0;
}

// requested size bu parametre iste kullanicinin block payloadinda bu kadar yer olmasu lazim minimum
void *heapster_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // burda bu deger 8 16 gibi bir degere gore align edilir arena.c icinde page size a gore align olur
    size_t aligned_payload_size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

    arena_t *arena = arena_get_list();
    block_header_t *block = NULL;

    while (arena) {
        block = arena_find_free_block(arena, aligned_payload_size);
        if (block) {
            break;
        }
        arena = arena->next;
    }

    if (!block) {
        size_t arena_size = aligned_payload_size + BLOCK_HEADER_SIZE + ARENA_HEADER_SIZE;

        if (arena_size < ARENA_MIN_SIZE) {
            arena_size = ARENA_MIN_SIZE;
        }

        arena_t *new_arena = arena_create(arena_size);

        if (!new_arena) {
            return NULL; 
        }

        block = arena_find_free_block(new_arena, aligned_payload_size);
        arena = arena_get_list(); // arena zaten basa eklendi ya arena_create
    }

    if (!block) {
        return NULL;
    }

    // block splitten sonra kalan block block min sizedan kucukse split etme degilse split et
    if (block->size  >= aligned_payload_size + BLOCK_MIN_SIZE) {

        block_header_t *splitted_first_part = block_split(arena, block, aligned_payload_size);
        splitted_first_part->requested_size = size;
        return block_to_payload(splitted_first_part);

    } else {
        block->free = 0;
        block->requested_size = size;    
        block->magic = CTRL_CHR; 
        return block_to_payload(block);
        // ama block size i block->size kadar ve artik split yapamazsin
    }
}

// n member and each member has the size of size
void *heapster_calloc(size_t nmemb, size_t size) {

    size_t total = nmemb * size;
    if (size != 0 && total / size != nmemb) {
        // carpim islemi overflow yapti
        return NULL;
    }

    void *ptr = heapster_malloc(total); 
    if (!ptr) {
        return NULL;
    }

    memset(ptr, 0, total); 
    return ptr;
}