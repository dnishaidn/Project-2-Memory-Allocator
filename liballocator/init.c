#include "api.h"
#include "my_memory.h"

void my_setup(enum malloc_type type, int memory_size, void *start_of_memory,
              int header_size, int min_mem_chunk_size, int n_objs_per_slab) {
    //save the arguments to global
    global_max_level = floor(log2(global_memory_size/ global_min_chunk));
    g_free_list[] = NULL;
    //create free_list block for whole block (note we gonna use levels as index for the free_list so it's easier to free them afterwards)
    // set the offset to 0 and insert it to the free_list at the highest index cuz it's empty

    return;
}

void my_cleanup() {
    // free nodes in free_list
    return;
}
