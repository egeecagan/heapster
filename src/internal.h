#ifndef HEAPSTER_INTERNAL_H
#define HEAPSTER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct block_header {
    size_t size;  // size of the payload (user-allocated memory) not including the header itself
    int free;     // allocation flag: 1 = free (available in free list), 0 = allocated (owned by user)

    // if requested size is 30 bytes for ex the allocator will still allocate the min size
    // by this way we can calculate wasted space and log it
    size_t requested_size;
    size_t alignment;

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
    // inside this i will have my free pointer
    struct block_header *free_list_head;

} arena_t;


// minimum block size (header only, without payload)
// even a block is only a header this is the min size of the whole block
#define BLOCK_MIN_SIZE (sizeof(block_header_t)) 

#endif /* HEAPSTER_INTERNAL_H */
