#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#include "api.h"

// Declare your own data structures and functions here...
// based on the main.c
extern void* global_base; //RAM 
extern size_t global_memory_size; // 8MB
extern size_t global_header_size; //8
extern size_t global_min_chunk_size; //512
extern int global_object_per_slab; //64
extern enum malloc_type global_mode_type;

void* buddy_malloc(int user_size);
void buddy_free(void* user_ptr);
void* slab_malloc(int user_size);
void slab_free(void* user_ptr);


typedef struct {
    uint32_t tag;
    uint32_t order;
} header_t;

void buddy_init(void);
void buddy_cleanup(void);
void  slab_init(void);
void  slab_cleanup(void);