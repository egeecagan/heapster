# ☀︎ Heapster: a custom memory allocator

**heapster** is a custom dynamic memory allocator written in **c**
it provides replacements for malloc, free, realloc, and calloc, implemented on top of system calls like sbrk() and mmap()

the allocator will support multiple strategies and include block splitting, coalescing, and fragmentation reporting


