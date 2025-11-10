#include "api.h"
#include "my_memory.h"

void my_setup(enum malloc_type type, int memory_size, void *start_of_memory,
              int header_size, int min_mem_chunk_size, int n_objs_per_slab) {
    //save the arguments to global
    global_mode_type = type;
    global_base = start_of_memory;
    global_memory_size = (size_t)memory_size;
    global_header_size = (size_t)header_size;
    global_min_chunk_size = (size_t)min_mem_chunk_size;
    global_object_per_slab = n_objs_per_slab;

    buddy_init();
    if (global_mode_type == MALLOC_SLAB) slab_init();  //buddy helps in slab

    return;
}

void my_cleanup() {
    // free nodes in free_list
    if (global_mode_type == MALLOC_SLAB) slab_cleanup();
    buddy_cleanup();
    return;
}