#include "my_memory.h"

// Memory allocator implementation
// Implement all other functions here...

void* global_base = NULL;
size_t global_memory_size = 0;
size_t global_header_size = 0;
size_t global_min_chunk_size = 0;
int global_object_per_slab = 0;
enum malloc_type global_mode_type = MALLOC_BUDDY;

static inline size_t order_to_size(int order); // returns the block size from the given order number

static inline int size_to_order(size_t size_rounded); // reverse process from above

static inline size_t next_powerof2(size_t n); // function for rounding up the power 2

static inline size_t pointer_to_offset(void* p); // convert the pointer to byte offset so we know which byte block starts

static inline void* offset_to_pointer(size_t off); // convert back to pointer from give byte offset

static inline size_t buddy_of(size_t off, size_t block_size); //finds the offset of the block pair after splitting them to power of 2

// this for free list operation
static void freelist_insert_sort(int order, size_t block_off); // create node and insert into the free list and sort them

static bool freelist_remove(int order, size_t block_off); // find node with given offset and remove

static bool freelist_pop_lowest(int order, size_t* out_off); // pop the head, removes the lowest address of free block

// this for split and merging
static bool split(int want_order, int* from_order, size_t* out_off); 
    // find the lowest order that has free block that is greater than the order that we want
    // split them, now one parent has two child, then we insert both to the lower order
    // we pop the buddy of the lowest address until we reach the order that we want

static size_t merge(size_t off, int* io_order); 
    // while buddy exist, remove the buddy node, take the min of offset and buddy offset, then +1 order
    
static inline header_t* header_from_user_ptr(void* user_ptr); // find the header from the pointer of the block when doing free

static inline header_t* header_block(void* block_start); // give the header location at the start of the block when doing malloc

typedef struct free_node {
    struct free_node* next;
    size_t offset;
} free_node_t;

#define MAX_ORDERS 32
static free_node_t* global_free_lists[MAX_ORDERS];
static int global_max_order;

void buddy_malloc(int user_size);
    // user size + global header 8
    // max of result and min chunk 512
    // call next power 2 to know which block size to use
    // call size to order to find the order from the freelist
    // call the freelist pop, if none, call split
    // find the start block
    // write the header at the start block
    // return the pointer, which + 8

void buddy_free(void* user_ptr);
    // find the header, call header from userptr
    // find the order from the header
    // find header location, call headerblock
    // insert the offset into global freelist
    // try merging, call buddy_of, if present, remove it and merge them together
