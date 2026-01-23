#pragma once
#include <sys/types.h>

#include <stdlib.h>
#include <stdint.h>

#define SYS_ALLOC malloc
#define SYS_FREE free

typedef struct{
    uint8_t* memory;
    uint8_t* last_end;
    
    size_t size;
}bump_allocator_t;

int bumper_create(bump_allocator_t* arena, size_t size);
void bumper_destroy(bump_allocator_t* arena);
void bumper_reset(bump_allocator_t* arena);

void* bumper_alloc(bump_allocator_t* arena, size_t size);
void* bumper_calloc(bump_allocator_t* arena, size_t n, size_t sizeofn);
char* bumper_strdup(bump_allocator_t* arena, const char* str);
void bumper_unwind(bump_allocator_t* arena, size_t size); //Pushes last_end pointer back by size

size_t bumper_size(bump_allocator_t* arena);
void* bumper_arena_end(bump_allocator_t* arena);
void* bumper_arena_start(bump_allocator_t* arena);