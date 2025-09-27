# ☀︎ Heapster: a custom memory allocator (for learning purposes )
heapster is a custom dynamic memory allocator written in c
it provides replacements for the standard malloc, free, realloc, and calloc functions implemented on top of low level system calls such as sbrk() and mmap()

the main goal of this project is for my better understanding of memory allocation functions

 ## features
- custom arenas to manage memory regions
- block headers for tracking size, free/used state, and links
- block splitting when a larger block is partially allocated
- block coalescing when adjacent free blocks are merged
- multiple allocation strategies -> first-fit, next-fit, best-fit, worst-fit
- support for both sbrk (heap extension) and mmap (page-aligned large allocations)
- thread safety via pthread mutexes

## current status
as of 27 september 2025:
- malloc, calloc, realloc and free are working
- splitting and coalescing are functional
- multiple arenas are supported
- statistics and reporting are planned but not yet implemented

---
this allocator is not intended for production use 
it was built as a learning project to better understand:

- memory arenas
- block headers and payloads
- splitting and merging of free blocks
- heap management with sbrk
- page allocation with mmap
- basic thread safety in c
