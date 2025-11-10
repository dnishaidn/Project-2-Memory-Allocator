#include "my_memory.h"

// Memory allocator implementation
// Implement all other functions here...

void* global_base = NULL;
size_t global_memory_size = 0;
size_t global_header_size = 0;
size_t global_min_chunk_size = 0;
int global_object_per_slab = 0;


enum malloc_type global_mode_type = MALLOC_BUDDY;


typedef struct free_node {
    struct free_node* next;
    size_t offset;
} free_node_t;

#define MAX_ORDERS 32
static free_node_t* global_free_lists[MAX_ORDERS];
static int global_max_order;

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
    return (void*)((char*)global_base + off);
} // convert back to pointer from give byte offset

static inline size_t buddy_of(size_t off, size_t block_size){
    return (off ^ block_size);
} //finds the offset of the block pair after splitting them to power of 2

// this for free list operation
static void freelist_insert_sort(int order, size_t block_off){
    free_node_t* node = (free_node_t*)malloc(sizeof(free_node_t));
    node -> offset = block_off;
    free_node_t** head = &global_free_lists[order];
    if (*head == NULL || (*head)->offset > block_off){
        node->next = *head;
        *head = node;
        return;
    }
    free_node_t* current = *head;
    while (current->next && current->next->offset < block_off) current = current->next;
    node->next = current->next;
    current->next = node;
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
    for (order = want_order; order <= global_max_order; ++order) {
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
        if (!freelist_remove(order, b_off)) break;
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



// internal tag
#define TAG_BUDY 0x42554459u

void *buddy_malloc(int user_size){
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
    // clear each order free list
    // find the max num of blocks that can fit inside memory
    // find largest power of two blocks, and set it equal gmo
    // call free list
    for (int i = 0; i < MAX_ORDERS; ++i) global_free_lists[i] = NULL; 
    size_t blocks = global_memory_size / global_min_chunk_size; 
    int maxorder = 0;
    while ((size_t)(1ull << maxorder) < blocks && maxorder + 1 < MAX_ORDERS) maxorder++;
    global_max_order = maxorder;
    freelist_insert_sort(global_max_order, 0);
}

void buddy_cleanup(void) {
    // for each order list, we will start at the head of each list, save the next pointer, then free the current node
    // we jump to the next node, and clear the head
    for (int i = 0; i < MAX_ORDERS; ++i) {
        free_node_t* current = global_free_lists[i];
        while (current) {
            free_node_t* nxt = current->next;
            free(current);
            current = nxt;
        }
        global_free_lists[i] = NULL;
    }
}


// ======================= Slab Allocation =======================

typedef struct sdt_row {
    size_t type_bytes;        
    size_t slab_size_bytes;   
    int    total_objects;     
    int    used_objects;      
    void*  slab_ptr;          
    struct sdt_row* next;
} sdt_row_t;

typedef struct slab {
    struct slab* next;        // slabs for this class
    void*  slab_start;        // buddy base 
    size_t slab_size;         // actual buddy block size (power-of-two)
    size_t actual_obj_size;          
    size_t capacity;          // # objects
    size_t used;              // # in use
    sdt_row_t* sdt;           // SDT 
} slab_t;
 
typedef struct slab_class {               //class represents a a specific slab and its type size,
    size_t actual_obj_size;               // eg. class[1] = store type_size 600bytes
    free_node_t* free_objs;               // eg. class[2] = store type_size 1000bytes
    slab_t* slabs;            
} slab_class_t;

#define MAX_CLASSES 128
static slab_class_t g_classes[MAX_CLASSES];
static int global_type_count = 0;

static void class_insert_sorted(free_node_t** head, size_t off) {
    free_node_t* node = (free_node_t*)malloc(sizeof(free_node_t));
    node->offset = off;
    if (*head == NULL || (*head)->offset > off) {
        node->next = *head; *head = node; return;
    }
    free_node_t* cur = *head;
    while (cur->next && cur->next->offset < off) cur = cur->next;
    node->next = cur->next; cur->next = node;
}
static bool class_pop_lowest(free_node_t** head, size_t* out_off) {
    if (*head == NULL) return false;
    free_node_t* n = *head; *out_off = n->offset; *head = n->next; free(n); return true;
}
static bool class_remove(free_node_t** head, size_t off) {
    free_node_t* cur = *head; free_node_t* prev = NULL;
    while (cur) {
        if (cur->offset == off) {
            if (prev) prev->next = cur->next; else *head = cur->next;
            free(cur); return true;
        }
        prev = cur; cur = cur->next;
    }
    return false;
}


static int find_class(size_t actual_obj_size) {
    for (int i = 0; i < global_type_count; ++i) {
        if (g_classes[i].actual_obj_size == actual_obj_size) return i;         // returning the class
    }
    g_classes[global_type_count] = (slab_class_t){          // create a new class if the slab for that type does not exist yet
        .actual_obj_size = actual_obj_size,                                   
        .free_objs = NULL,
        .slabs = NULL
    };

    return global_type_count++;
}

static slab_t* slab_new(size_t type_bytes){
    size_t actual_obj_size   = global_header_size + type_bytes;               
    size_t size_of_slab  = (size_t)global_object_per_slab * actual_obj_size;

    void* slab_from_buddy = buddy_malloc(size_of_slab);  // ask buddy for slab
    if (!slab_from_buddy) return NULL;


    void* slab_start = (uint8_t*)slab_from_buddy - global_header_size;     // buddy header is here

    header_t* bh = (header_t*)slab_start;
    size_t real_slab_size = ((size_t)global_min_chunk_size) << bh->order;


    size_t cap = (size_t)global_object_per_slab;                      // create the slab
    slab_t* s = (slab_t*)malloc(sizeof(*s));
    *s = (slab_t){ .next=NULL, .slab_start=slab_start, .slab_size=real_slab_size,
                   .actual_obj_size=actual_obj_size, .capacity=cap, .used=0, .sdt=NULL };

    // create class for that type_size 
    int class_id = find_class(actual_obj_size);
  
    s->next = g_classes[class_id].slabs;
    g_classes[class_id].slabs = s;

    
    uint8_t* base = (uint8_t*)slab_start + global_header_size;
    for (size_t i = 0; i < cap; ++i){
        size_t off = pointer_to_offset(base + i * actual_obj_size);
        class_insert_sorted(&g_classes[class_id].free_objs, off);
    }

    //update SDT
    s->sdt = (sdt_row_t*)malloc(sizeof(sdt_row_t));
    *(s->sdt) = (sdt_row_t){ .type_bytes=type_bytes, .slab_size_bytes=real_slab_size,
                             .total_objects=(int)cap, .used_objects=0,
                             .slab_ptr=slab_start, .next=NULL };
    return s;
}







