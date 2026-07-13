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

#include "memman.h"
#include "bumper.h"
#include "config.h"
#include "list.h"

#include <assert.h>
#include <stdatomic.h>

static bump_allocator_t s_arena = {0};
static struct list_head s_subarena_list = {0};
static atomic_flag s_spinlock = ATOMIC_FLAG_INIT;

#define MEMMAN_CRTIICAL_ENTER() ({while(atomic_flag_test_and_set(&s_spinlock)){}})
#define MEMMAN_CRITICAL_EXIT() atomic_flag_clear(&s_spinlock)

void memman_init(){
    INIT_LIST_HEAD(&s_subarena_list);
    assert(bumper_create(&s_arena, VM_ARENA_SIZE) == 0);
}

//WARNING: it does NOT check for existance of the arena with same ID
bump_allocator_t* memman_create(int id, size_t size){
    MEMMAN_CRTIICAL_ENTER();
    void* memory = bumper_calloc(&s_arena, 1, size + sizeof(MemmanChunk_t));
    if(!memory){
        MEMMAN_CRITICAL_EXIT();
        return NULL;
    } 

    MemmanChunk_t* chunk = memory;

    INIT_LIST_HEAD(&chunk->list);
    assert(bumper_create_from(&chunk->arena, memory + sizeof(*chunk), size) == 0);
    chunk->id = id;

    list_add(&chunk->list, &s_subarena_list);

    MEMMAN_CRITICAL_EXIT();
    return &chunk->arena;
}

bump_allocator_t* memman_get(int id){
    MEMMAN_CRTIICAL_ENTER();

    MemmanChunk_t *chunk = NULL;
    list_for_each_entry(chunk, &s_subarena_list, list){
        if(chunk->id == id){
            MEMMAN_CRITICAL_EXIT();
            return &chunk->arena;
        }
    }

    MEMMAN_CRITICAL_EXIT();
    return NULL;
}