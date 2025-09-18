
#include "heapster.h"

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
    
    if (arena_size > 0) {
        default_arena_size = arena_size;
    }
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