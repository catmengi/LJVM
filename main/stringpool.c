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

#include "stringpool.h"
#include "bumper.h"
#include "config.h"
#include "list.h"
#include "memman.h"

#include <stdatomic.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>


static bump_allocator_t* s_arena = NULL;

static struct list_head s_entry_list = {0};
static size_t s_entry_count = 0;

#define POOL_CRITICAL_ENTER(spinlock) ({while(atomic_flag_test_and_set(&(spinlock))){}})
#define POOL_CRITICAL_EXIT(spinlock) atomic_flag_clear(&(spinlock))

static StringpoolEntry_t* insert_entry();
void stringpool_init(){
    assert((s_arena = memman_get(VM_PERMA_ARENA_ID)));

    INIT_LIST_HEAD(&s_entry_list);
    s_entry_count = 0;

    assert(insert_entry());  //Add initial entry (other wise it wouldnt work)
}

//NON THREAD SAFE!
static StringpoolEntry_t* insert_entry(){
    StringpoolEntry_t* entry = bumper_calloc(s_arena, 1, sizeof(*entry));
    if(!entry){
        return NULL;
    } 

    INIT_LIST_HEAD(&entry->list);
    //entry->spinlock = (atomic_flag)ATOMIC_FLAG_INIT;

    list_add_tail(&entry->list, &s_entry_list);
    s_entry_count++;

    return entry;
}

//It might return non-null entry but with NULL data since it performs NO validation. NON THREAD SAFE
static StringpoolItem_t* find_slot(uint32_t name_id){
    uint32_t bucket_index = name_id / STRINGPOOL_ENTRY_ITEMS_COUNT;
    uint32_t slot_index = name_id % STRINGPOOL_ENTRY_ITEMS_COUNT;

    if(bucket_index >= s_entry_count){
        return NULL;
    }

    struct list_head* cur = s_entry_list.next;
    for(unsigned i = 0; i < bucket_index; i++, cur = cur->next) {}

    StringpoolEntry_t* entry = list_entry(cur, StringpoolEntry_t, list);

    return &entry->items[slot_index];
}


static uint32_t djb2_hash(char *str) {
        uint32_t hash = 5381;
        int c;
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
}

//THREAD SAFE
int32_t stringpool_add(char* string){
    if(!string) return -1;

    uint32_t nameid_offset = 0, n = 0;
    unsigned start_pos = djb2_hash(string) % STRINGPOOL_ENTRY_ITEMS_COUNT;

    StringpoolEntry_t* entry = NULL;

    list_for_each_entry(entry, &s_entry_list, list){

insert:
        for(unsigned i = 0; i < STRINGPOOL_ENTRY_ITEMS_COUNT; i++){
            uint32_t index = (start_pos + i) % STRINGPOOL_ENTRY_ITEMS_COUNT;
            StringpoolItem_t* item = &entry->items[index];

            if(item->cstr == NULL){
                item->cstr = bumper_strdup(s_arena, string);
                if(!item->cstr){
                    return -1;
                }

                return index + nameid_offset;
            } else if(strcmp(item->cstr, string) == 0){
                return index + nameid_offset;
            }
        }

        nameid_offset += STRINGPOOL_ENTRY_ITEMS_COUNT;

        if(++n == s_entry_count){ 
            StringpoolEntry_t* new = insert_entry();
            if(!new){
                return -1;
            } else {
                entry = new; //goto go brrrrrrrr
                goto insert; //Should not fail
            }
        }
    }

    return -1;
}

//THREAD SAFE
char* stringpool_get(int32_t name_id){
    StringpoolItem_t* item = find_slot(name_id);
    return item ? item->cstr : NULL;
}