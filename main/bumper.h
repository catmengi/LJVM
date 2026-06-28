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
int bumper_create_from(bump_allocator_t* arena, void* arena_memory, size_t size);
void bumper_destroy(bump_allocator_t* arena);
void bumper_reset(bump_allocator_t* arena);

void* bumper_alloc(bump_allocator_t* arena, size_t size);
void* bumper_calloc(bump_allocator_t* arena, size_t n, size_t sizeofn);
char* bumper_strdup(bump_allocator_t* arena, const char* str);
void bumper_unwind(bump_allocator_t* arena, size_t size); //Pushes last_end pointer back by size

size_t bumper_size(bump_allocator_t* arena);
size_t bumper_used(bump_allocator_t* arena);
void* bumper_arena_end(bump_allocator_t* arena);
void* bumper_arena_start(bump_allocator_t* arena);