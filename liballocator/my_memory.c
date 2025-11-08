#include "my_memory.h"

// Memory allocator implementation
// Implement all other functions here...

void* global_base = NULL;
size_t global_memory_size = 0;
size_t global_header_size = 0;
size_t global_min_chunk_size = 0;
int global_object_per_slab = 0;
enum malloc_type global_mode_type = MALLOC_BUDDY;

static inline size_t order_to_size(int order){
    return (size_t)global_min_chunk_size << order;
} // returns the block size from the given order number

static inline int size_to_order(size_t size_rounded){
    int order = 0;
    size_t size = global_min_chunk_size;
    while (size < size_rounded) {
        size <<= 1;
        order++;
    }
    return order;
} // reverse process from above

static inline size_t next_powerof2(size_t n){
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    #if UINPTR_MAX > 0xFFFFFFFFu
        n |= n >> 32;
    #endif
    n++;
    return n;
} // function for rounding up the power 2

static inline size_t pointer_to_offset(void* p){
    return (size_t)((char*)p - (char*)global_base);
} // convert the pointer to byte offset so we know which byte block starts

static inline void* offset_to_pointer(size_t off){
    (void*)((char*)global_base + off);
} // convert back to pointer from give byte offset

static inline size_t buddy_of(size_t off, size_t block_size){
    return (off ^ block_size);
} //finds the offset of the block pair after splitting them to power of 2

// this for free list operation
static void freelist_insert_sort(int order, size_t block_off){
    free_node_t* node = (free_node_t*)malloc(sizeof(free_node_t));
    n -> offset = block_off;
    free_node_t** head = &global_free_lists[order];
    if (*head == NULL || (*head)->offset > block_off){
        n->next = *head;
        *head = n;
        return;
    }
    free_node_t* current = *head;
    while (current->next && current->next->offset < block_off) current = current->next;
    n->next = cur->next;
    cur->next = n;
} // create node and insert into the free list and sort them

static bool freelist_remove(int order, size_t block_off){
    free_node_t** head  = &global_free_lists[order];
    free_node_t* current = *head;
    free_node_t* prev = NULL;
    while (current){
        if (current->offset == block_off){
            if (prev) prev->next = current->next;
            else *head = current->next;
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
} // find node with given offset and remove

static bool freelist_pop_lowest(int order, size_t* out_off){
    free_node_t** head = &global_free_lists[order];
    if (*head == NULL) return false;
    free_node_t* n = *head;
    *out_off = n->offset;
    *head = n->next;
    free(n);
    return true;
} // pop the head, removes the lowest address of free block

// this for split and merging
static bool split(int want_order, int* from_order, size_t* out_off){
    // find the lowest order that has free block that is greater than the order that we want
    // split them, now one parent has two child, then we insert both to the lower order
    // we pop the buddy of the lowest address until we reach the order that we want
    int order;
    size_t off;
    for (order = want_order; order <= global_max_order; ++o) {
        if (freelist_pop_lowest(order, &off)) {
            break;
        }
    }
    if (order > global_max_order) return false;
    while (order > want_order) {
        size_t size = order_to_size(order);
        size_t half = size >> 1;
        size_t right_off = off + half;
        freelist_insert_sort(order - 1, right_off);
        order -= 1;
    }
    *from_order = order;
    *out_off = off;
    return true;    
}

static size_t merge(size_t off, int* io_order){
    // while buddy exist, remove the buddy node, take the min of offset and buddy offset, then +1 order
    int order = *io_order;
    while (1) {
        size_t size = order_to_size(order);
        size_t b_off = buddy_of(off, size);
        if (!freelist_remove(o, b_off)) break;
        off = (b_off < off) ? b_off : off;
        order += 1;
        if (order > global_max_order) break;
    }
    *io_order = order;
    return off;    
}
    
static inline header_t* header_from_user_ptr(void* user_ptr){
    return (header_t*)((char*)user_ptr - global_header_size);
} // find the header from the pointer of the block when doing free

static inline header_t* header_block(void* block_start){
    return (header_t*)block_start;
} // give the header location at the start of the block when doing malloc

typedef struct free_node {
    struct free_node* next;
    size_t offset;
} free_node_t;

#define MAX_ORDERS 32
static free_node_t* global_free_lists[MAX_ORDERS];
static int global_max_order;

// internal tag
#define TAG_BUDY 0x42554459u

void buddy_malloc(int user_size){
    // user size + global header 8
    // max of result and min chunk 512
    // call next power 2 to know which block size to use
    // call size to order to find the order from the freelist
    // call the freelist pop, if none, call split
    // find the start block
    // write the header at the start block
    // return the pointer, which + 8
    if (user_size <= 0) return NULL;
    size_t need = (size_t)user_size + global_header_size;
    if (need < global_min_chunk_size) need = global_min_chunk_size;
    size_t blk = next_powerof2(need);
    int want_order = size_to_order(blk);

    size_t off;
    if (!freelist_pop_lowest(want_order, &off)) {
        int from_order = -1;
        if (!split(want_order, &from_order, &off)) return NULL;
    }

    void* block_start = offset_to_pointer(off);
    header_t* hdr = header_block(block_start);
    hdr->tag = TAG_BUDY;
    hdr->order = (uint32_t)want_order;
    return (void*)((char*)block_start + global_header_size);   
}

void buddy_free(void* user_ptr){
    // find the header, call header from userptr
    // find the order from the header
    // find header location, call headerblock
    // insert the offset into global freelist
    // try merging, call buddy_of, if present, remove it and merge them together
    header_t* hdr = header_from_user_ptr(user_ptr);
    int order = (int)hdr->order;
    size_t off = pointer_to_offset((void*)hdr);
    off = merge(off, &order);
    freelist_insert_sort(order, off);
}

void buddy_init(void) {
    for (int i = 0; i < MAX_ORDERS; ++i) global_free_lists[i] = NULL;
    size_t blocks = global_memory_size / global_min_chunk_size;
    int maxo = 0;
    while ((size_t)(1ull << maxo) < blocks && maxo + 1 < MAX_ORDERS) maxo++;
    global_max_order = maxo;
    freelist_insert_sort(global_max_order, 0);
}

void buddy_cleanup(void) {
    for (int i = 0; i < MAX_ORDERS; ++i) {
        free_node_t* cur = global_free_lists[i];
        while (cur) {
            free_node_t* nxt = cur->next;
            free(cur);
            cur = nxt;
        }
        global_free_lists[i] = NULL;
    }
}
