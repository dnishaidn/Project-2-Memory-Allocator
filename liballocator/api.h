#pragma once

// Allocation type
enum malloc_type {
    MALLOC_BUDDY = 0, // Buddy allocator
    MALLOC_SLAB = 1,  // Slab allocator
};

// APIs
void my_setup(enum malloc_type type, int memory_size, void *start_of_memory,
              int header_size, int min_mem_chunk_size, int n_objs_per_slab);
void my_cleanup();

void *my_malloc(int size);
void my_free(void *ptr);
