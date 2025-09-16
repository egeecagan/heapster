#ifndef HEAPSTER_INTERNAL_H
#define HEAPSTER_INTERNAL_H

#define ALIGNMENT 8
#define CTRL_CHR   0xC0FFEE     // kahvesiz kod olmaz kral.

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>


typedef struct block_header {
    size_t size;  // size of the payload (user-allocated memory) not including the header itself
    int free;     // allocation flag: 1 = free (available in free list), 0 = allocated (owned by user)

    // if requested size is 30 bytes for ex the allocator will still allocate the min size
    // by this way we can calculate wasted space and log it
    size_t requested_size;

    // doubly linked list pointers for managing free blocks (only valid when free == 1)
    struct block_header *next;  
    struct block_header *prev;  

    // even the next or previous block is free or not theese pointers look to them
    // reason is if we have coalescing it helps to combine free blocks to a single block
    struct block_header *phys_prev;
    struct block_header *phys_next;

    uint32_t magic;  // a constant value for spotting the double frees and non heapster_malloc' ed pointers

    /*
    * arena is a contiguous memory region managed by the allocator, with its own free lists and blocks.
    * and this number is about which areana a block is in.
    */
    int arena_id;    

} block_header_t;

typedef struct arena {
    // arena id si
    int id;

    // for thread safe alocation from pthread.h kullanip kullanmayacagim supheli
    pthread_mutex_t lock;       

    // free blocklari tutan listin basi
    struct block_header *free_list_head;  

    // policy olarak find_next_fir icin cursor pointer
    struct block_header *next_fit_cursor;

    // arena ici free olup olmayan tum blocklar
    void *start;
    void *end;

    // includes headers and payloads direk arenanin tum size i 
    size_t size; 
} arena_t;


// minimum block size (header only, without payload)
// even a block is only a header this is the min size of the whole block


// bu macro align edilebilir deger cikartir her halukarda bu da align etme formulu k&r a gore
#define BLOCK_HEADER_SIZE \
    ((sizeof(block_header_t) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))


#define BLOCK_MIN_SIZE (BLOCK_HEADER_SIZE)

void *block_to_payload(block_header_t *block);
block_header_t *payload_to_block(void *payload);

#endif /* HEAPSTER_INTERNAL_H */
