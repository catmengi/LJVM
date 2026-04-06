#pragma once

#include <stdlib.h>
#include "bumper.h"

typedef struct bstable_t bstable_t;
typedef struct bstable_t{
    bump_allocator_t* arena;
    
    const void** elements;

    size_t size; //Maximum elements count
    size_t count; //Number of inserted elements
    size_t last_sorted; //Store the count number when sort was performed

    int (*cmp)(const void* a, const void* b);
    void* (*find)(bstable_t* bstable, const void* key);
}bstable_t;

int bstable_init(bstable_t* bstable, bump_allocator_t* arena, size_t size, int (*cmp)(const void* a, const void* b),
                void* (*find)(bstable_t* bstable, const void* key));
int bstable_insert(bstable_t* bstable, const void* element);
void* bstable_find(bstable_t* bstable, const void* key);
void* bstable_find_raw(bstable_t* bstable, const void* find_template);
