#include "bumper.h"

#include <sys/types.h>
#include <string.h>

int bumper_create(bump_allocator_t* arena, size_t size){
    void* arena_memory = SYS_ALLOC(size);
    if(arena_memory == NULL) return 1;

    arena->last_end = arena->memory = arena_memory;
    arena->size = size;

    return 0;
}

int bumper_create_from(bump_allocator_t* arena, void* arena_memory, size_t size){
    arena->last_end = arena->memory = arena_memory;
    arena->size = size;

    return 0;
}

void bumper_destroy(bump_allocator_t* arena){
    SYS_FREE(arena->memory);
}

void bumper_reset(bump_allocator_t* arena){
    arena->last_end = arena->memory;
}

void* bumper_alloc(bump_allocator_t* arena, size_t size){
    if((ssize_t)(arena->size - (arena->last_end - arena->memory)) < size)
        return NULL;

    void* new_memory = arena->last_end;
    arena->last_end += size;

    return new_memory;
}

void bumper_unwind(bump_allocator_t* arena, size_t size){
    if((arena->last_end - arena->memory) < size)
        return;

    arena->last_end -= size;
}

size_t bumper_size(bump_allocator_t* arena){
    return arena->size;
}

void* bumper_arena_end(bump_allocator_t* arena){
    return arena->memory + arena->size;
}

void* bumper_arena_start(bump_allocator_t* arena){
    return arena->memory;
}

void* bumper_calloc(bump_allocator_t* arena, size_t n, size_t sizeofn){
    void* memory = bumper_alloc(arena,n * sizeofn);
    if(memory){
        memset(memory,0,n * sizeofn);
    }
    return memory;
}

char* bumper_strdup(bump_allocator_t* arena, const char* str){
    char* new_string = NULL;
    if(str){
        new_string = bumper_alloc(arena,strlen(str) + 1);
        if(new_string){
            strcpy(new_string,str);
        }
    }
    return new_string;
}