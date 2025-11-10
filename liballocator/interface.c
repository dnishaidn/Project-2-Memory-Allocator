#include "api.h"
#include "my_memory.h"

// Interface implementation
// Implement APIs here...

void *my_malloc(int size) {
    if (global_mode_type == MALLOC_BUDDY) {
        return buddy_malloc(size); }
    else {
        return slab_malloc(size);
    }
    
    return NULL;
}

void my_free(void *ptr) {
    if (!ptr) return;
    if (global_mode_type == MALLOC_BUDDY){
        buddy_free(ptr);
    }
    else {
        return slab_free(ptr);
    }

    return NULL;
}