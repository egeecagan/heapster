# 🗿 Heapster: A Custom Dynamic Memory Allocator (C)

**Heapster** is a comprehensive custom memory allocator written in C, providing replacement implementations for the standard C library functions (`malloc`, `free`, `realloc`, `calloc`).

Developed as a deep-dive learning project, Heapster focuses on implementing advanced **system programming concepts**, **concurrency mechanisms**, memory alignment, and **memory management strategies** essential for low-level engineering roles. The primary goal was to achieve a profound, hands-on understanding of these core functions.

## 📐 Core Architectural Features

Heapster's design focuses on efficiency, low fragmentation, and multi-threaded scalability, showcasing mastery of complex memory management challenges.

---
### 1. Custom Memory Arenas
Instead of relying on a single global lock for all memory operations, **Custom Memory Arenas** manage independent, isolated memory regions. This design is crucial for **reducing lock contention overhead** in a concurrent environment, allowing multiple threads to allocate memory simultaneously from different arenas. This directly addresses **scalability issues** common in single-heap allocators.

---
### 2. Block Headers and Metadata
Each memory block (both free and used) contains dedicated **Block Headers** (and often footers, known as Boundary Tags). This metadata structure is critical for:
* **Tracking State:** Storing the current block size and its free/used status.
* **Navigation:** For free blocks, the header holds **links (pointers)** to the next and previous free blocks in the **Explicit Free List** of each arena. This allows the allocator to find an appropriate free block very quickly without scanning the entire heap.

---
### 3. Block Splitting
When a memory request is smaller than the smallest suitable free block found, the allocator performs **Block Splitting**. It allocates the requested size from the beginning of the free block and converts the remaining unused portion into a new, smaller free block. This action prevents large chunks of memory from being unnecessarily reserved for small requests, thus **minimizing internal fragmentation**.

---
### 4. Block Coalescing
To combat **external fragmentation** (where total free memory is high, but scattered in small, unusable chunks), **Block Coalescing** is performed immediately when a block is freed. The allocator checks the metadata of its adjacent neighbors. If a neighbor is also free, the blocks are merged into a **single, larger contiguous free block**. This guarantees that the largest possible free block is always available to satisfy future large memory requests.

---
### 5. Multiple Allocation Strategies
Heapster supports a configurable strategy pattern, allowing the user to select from several fundamental allocation policies. This showcases the ability to analyze and implement trade-offs between different performance goals:
* **First-Fit:** Fast allocation; searches for the first available block large enough.
* **Next-Fit:** Starts searching from the previous allocation point, often leading to better spatial locality.
* **Best-Fit:** Searches the entire list to find the *smallest* block that fits the request, minimizing wasted space (internal fragmentation).
* **Worst-Fit:** Searches for the *largest* block to maximize the size of the leftover free block.

---
### 6. Hybrid OS Memory Management
The allocator utilizes a **hybrid approach** to interact with the operating system's memory:
* **`sbrk()` (Heap Extension):** Used for obtaining smaller, typically contiguous chunks of memory to extend the existing heap managed by the arenas.
* **`mmap()` (Page Allocation):** Used for very large memory requests, which are allocated directly from the OS as **page-aligned virtual memory**. This bypasses the heap structure for large allocations, reducing fragmentation within the main heap and improving efficiency for massive blocks.

---
### 7. Concurrency Mechanism (Thread Safety Attempt)
A key focus of this project was exploring methods for safe concurrent memory access. **Thread Safety** was attempted by protecting critical sections (like updating free lists or modifying arena structures) with POSIX **`pthread_mutexes`**. This mechanism aims to ensure data integrity when multiple threads call `malloc` or `free` simultaneously. *While the architecture is designed for thread safety via arenas and mutexes, a dedicated stress test suite is required to validate its robustness under all concurrent workloads.*

---
### 8. Strict Memory Alignment
To ensure **maximum performance and portability** across different hardware architectures, Heapster enforces strict memory alignment rules:
* **Page Alignment:** All large memory allocations obtained via `mmap()` are **page-aligned** to optimize virtual memory operations and reduce page-level fragmentation.
* **Internal Alignment:** All internal metadata (headers) and the user-facing payload are aligned to the system's maximum alignment boundary (e.g., `alignof(max_align_t)`). This prevents unaligned memory access issues and ensures optimal data access speeds for modern CPUs.

---
## 🚀 How to Use It

To build and integrate the Heapster library into your C project:

1.  Create a build folder in the project root and navigate into it:
    ```bash
    mkdir build && cd build
    ```
2.  Run CMake and compile the static library:
    ```bash
    cmake ..
    make
    ```
3.  The static library file (`libheapster.a`) will now be inside the `build` folder. Copy this file, along with `heapster.h` (from the `include` directory), to your target project.
4.  Compile your application (e.g., `main.c`) by linking against the library and the `pthread` library:
    ```bash
    clang main.c -o main -I. -L. -lheapster -lpthread
    ```
    You can now use the replacement functions (`malloc`, `calloc`, etc.) in your public API.

## ⚠️ Limitations and Learning Focus

While this allocator successfully implements complex features, it remains a **learning project** and is **not intended for production use**. The focus was on architectural understanding, specifically:

* **Concurrency Validation:** The current implementation of thread safety requires further rigorous stress testing and benchmarking to confirm lock overhead and overall reliability under heavy contention.
* **Performance:** Performance has not yet been fully benchmarked against highly optimized production allocators (e.g., glibc's malloc).

While this allocator is **not intended for production use**, it serves as a robust educational tool for understanding and implementing:
* Custom Memory Arenas and heap partitioning.
* Block headers, payloads, and **Boundary Tags**.
* Low-level mechanics of splitting and merging free blocks.
* Heap management using `sbrk()` and page allocation via `mmap()`.
* Basic C concurrency using `pthread_mutexes`.




