/*
JEspressoVM - project to bring java bytecode execution to esp32 (and others)

Copyright (C) 2026  Vladislav Potrashkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

#include "bumper.h"

#include <stdatomic.h>
#include <sys/types.h>
#include <string.h>

#define BUMPER_CRTIICAL_ENTER(arena) ({while(atomic_flag_test_and_set(&(arena)->spinlock)){}})
#define BUMPER_CRITICAL_EXIT(arena) atomic_flag_clear(&(arena)->spinlock)

int bumper_create(bump_allocator_t* arena, size_t size){
    void* arena_memory = SYS_ALLOC(size);
    if(arena_memory == NULL) return 1;

    arena->spinlock = (atomic_flag)ATOMIC_FLAG_INIT;
    arena->last_end = arena->memory = arena_memory;
    arena->size = size;
    memset(arena->memory, 0, arena->size);

    return 0;
}

int bumper_create_from(bump_allocator_t* arena, void* arena_memory, size_t size){
    arena->spinlock = (atomic_flag)ATOMIC_FLAG_INIT;
    arena->last_end = arena->memory = arena_memory;
    arena->size = size;

    return 0;
}

void bumper_destroy(bump_allocator_t* arena){
    SYS_FREE(arena->memory);
}

void bumper_reset(bump_allocator_t* arena){
    BUMPER_CRTIICAL_ENTER(arena);
    arena->last_end = arena->memory;
    BUMPER_CRITICAL_EXIT(arena);
}

void* bumper_alloc(bump_allocator_t* arena, size_t size){
    BUMPER_CRTIICAL_ENTER(arena);

    if((ssize_t)(arena->size - (arena->last_end - arena->memory)) < size){
        BUMPER_CRITICAL_EXIT(arena);
        return NULL;
    }

    void* new_memory = arena->last_end;
    arena->last_end += size;

    BUMPER_CRITICAL_EXIT(arena);
    return new_memory;
}

void bumper_unwind(bump_allocator_t* arena, size_t size){
    BUMPER_CRTIICAL_ENTER(arena);

    if((arena->last_end - arena->memory) < size){
        BUMPER_CRITICAL_EXIT(arena);
        return;
    }
    arena->last_end -= size;

    BUMPER_CRITICAL_EXIT(arena);
}

size_t bumper_size(bump_allocator_t* arena){
    return arena->size;
}

size_t bumper_used(bump_allocator_t* arena){
    BUMPER_CRTIICAL_ENTER(arena);
    size_t size = arena->last_end - arena->memory;
    BUMPER_CRITICAL_EXIT(arena);

    return size;
}

void* bumper_arena_end(bump_allocator_t* arena){
    BUMPER_CRTIICAL_ENTER(arena);
    void* end = arena->memory + arena->size;
    BUMPER_CRITICAL_EXIT(arena);

    return end;
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