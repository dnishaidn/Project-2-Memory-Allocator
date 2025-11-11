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

// For slab we did not utilize bitmap for the slab pointer of the sdt as we are not familiar on how to implement them to the code.
// Instead, we utilize a stack-based free list similar to the buddy allocator and uses pointer to navigate through the linked list

#ifndef MAX_SLABS
#define MAX_SLABS 4096
#endif
                                    //
typedef struct sdt {
    size_t type_bytes;        // size of each obj in slab
    size_t slab_size;         //size of the slab itself
    int    objs_in_slab;        // should be = global_object_per_slab
    int    used;            // no of objs in slab

    int    free_top;        // index for next free spot
    size_t *free_offs;      // array holding the offsets
    int    alive;           // 1 if alive, 0 if free
    size_t slab_off;        // where slab start
} sdt;

static sdt slabs[MAX_SLABS];
static int slab_count = 0;



static int make_slab(size_t type_bytes) {
    int slab_id = -1;
    for (int i = 0; i < slab_count; ++i) {           //find inactive slab to use
         if (!slabs[i].alive) {
            slab_id = i; break;
        }
     }

    if (slab_id < 0) {
        if (slab_count >= MAX_SLABS)             //check if can make new one (slab_count< max_slabs)
            return -1;
        slab_id = slab_count++;
    }

    int cap = global_object_per_slab;
    size_t bytes_slab_use = (size_t)cap * type_bytes;
    void* slab_from_buddy = buddy_malloc(bytes_slab_use);    //#### use buddy allocator to give slab large enough for the memory
    if (!slab_from_buddy)
        return -1;

    uint8_t* slab_start = (uint8_t*)slab_from_buddy - global_header_size;      //find the real slab start
    header_t* hdr = (header_t*)slab_start;
    size_t real_slab_size = ((size_t)global_min_chunk_size) << hdr->order;     // find the slab sizee

    sdt *S = &slabs[slab_id];                  //creating a slab S and fill in the characteristic of that slab
    *S = (sdt){0};                             //reset all field to zero first lol
    S->alive    = 1;
    S->slab_off = pointer_to_offset(slab_start);
    S->slab_size = real_slab_size; 
    S->type_bytes = type_bytes;
    S->objs_in_slab = cap;
    S->used     = 0;
    S->free_top = 0;
    S->free_offs = (size_t*)malloc((size_t)cap * sizeof(size_t));          //create array to track of free object locations
 
    
    if (!S->free_offs) {
        buddy_free(slab_from_buddy);          //clean up if fail the array alloaction 
        S->alive = 0;
        return -1;
    }

    uint8_t* base = slab_start + global_header_size;         //fill the free list will all objs slots
    for (int i = cap - 1; i >= 0; --i) {
        S->free_offs[S->free_top++] = pointer_to_offset(base + (size_t)i * type_bytes);
    }
    return slab_id;               
}

void slab_init(void) {
    for (int i = 0; i < MAX_SLABS; ++i) {            //helps initialize the slabs for other functions
        slabs[i].alive = 0;
        slabs[i].slab_off = 0;
        slabs[i].type_bytes = 0;
        slabs[i].objs_in_slab = 0;

        slabs[i].used = 0;
        slabs[i].free_top = 0;
        slabs[i].free_offs = NULL;
    }
    slab_count = 0;
}

void* slab_malloc(int user_size) {
    if (user_size <= 0) return NULL;

    size_t type_bytes = (size_t)user_size + (size_t)global_header_size;   //add header for the user size

    int slab_id = -1;     
    for (int i = 0; i < slab_count; i++) {
    sdt *s = &slabs[i];  //basically just checking if the objects size that the slab houses, matches our needed size                                                      

    if (!s->alive)
        continue;   
    if (s->type_bytes != type_bytes)
        continue;   
    if (s->free_top == 0)
        continue;   
    slab_id = i;    // found a good match
    break;
    }

    if (slab_id < 0) {                                    // if there is no suitable slab for that size, create new one!!
        slab_id = make_slab(type_bytes); 
        if (slab_id < 0) return NULL;
    }

    sdt *S = &slabs[slab_id];                    //pointer S point to slab to allocate
    size_t obj_off = S->free_offs [--S->free_top];                        // take the last free offset and decrsea free top 
    S->used++;

    uint8_t* object_hdr = (uint8_t*)offset_to_pointer(obj_off);

    header_t* h = (header_t*)object_hdr;                 //write thos object_hdr at the start of mem block
    h->order = (uint32_t)slab_id;

    return object_hdr + global_header_size;               //return pointerr
}

void slab_free(void* user_ptr) {
    if (user_ptr == NULL) {
        return;
    }

    uint8_t *object_hdr = (uint8_t*)user_ptr - global_header_size;

    header_t *h = (header_t*)object_hdr;

    int sid = (int)h->order;
    if (sid < 0) {
        return;
    }
    if (sid >= slab_count) {
        return;
    }

    sdt *s = &slabs[sid];   //getting pointer for that slab
    if (!s->alive) {
        return;
    }

    uint8_t *slab_ptr = (uint8_t*)offset_to_pointer(s->slab_off);          //convert offset back to ptr
    uint8_t *where_start = slab_ptr + global_header_size;

    size_t difference = (size_t)(object_hdr - where_start);    // compute how far obj hdr is from  the first obj hdr
    if ((difference % s->type_bytes) != 0) {
        return;
    }

    if (s->free_top < s->objs_in_slab) {                        //pushing obj back into the free stack
        size_t off = pointer_to_offset(object_hdr);         // store offset and bump the stack top
        s->free_offs[s->free_top] = off;
        s->free_top += 1;
    }

    if (s->used > 0) {
        s->used -= 1;
    }

    if (s->used == 0) {                                      //return the empty slab memory back to buddy allocator
        int all_free = (s->free_top == s->objs_in_slab);
        if (all_free) {
            buddy_free(where_start);

            if (s->free_offs != NULL) {                      // free the array
                free(s->free_offs);
                s->free_offs = NULL;
            }

            s->alive = 0;                                  // reset back all slab
            s->slab_off = 0;
            s->type_bytes = 0;
            s->objs_in_slab = 0;
            s->free_top = 0;
        }
    }
}


void slab_cleanup(void) {
    for (int i = 0; i < slab_count; i++) {           //kinda similar to slab_free
        sdt *s = &slabs[i];
        if (!s->alive)                             //skip empty slab
            continue;

        uint8_t *slab_ptr = (uint8_t *)offset_to_pointer(s->slab_off);
        uint8_t *usable_ptr = slab_ptr + global_header_size;

        buddy_free(usable_ptr);                  //using buddy free to return the slab memory

        if (s->free_offs) {                      //free and null the offset
            free(s->free_offs);
            s->free_offs = NULL;
        }

        s->alive = 0;                          //reset
        s->slab_off = 0;
        s->type_bytes = 0;
        s->objs_in_slab = 0;
        s->used = 0;
        s->free_top = 0;
    }

    slab_count = 0;
}
