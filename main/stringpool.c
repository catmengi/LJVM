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

#include <string.h>
#include <assert.h>
#include <stdbool.h>

static bump_allocator_t s_arena = {0};
static char** s_strings = NULL;
static bool s_initialised = false;

static uint32_t djb2_hash(char *str) {
        uint32_t hash = 5381;
        int c;
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
}

void stringpool_init(){
    if(!s_initialised){
        assert(bumper_create(&s_arena, STRINGPOOL_ARENA) == 0);
        assert((s_strings = bumper_calloc(&s_arena, STRINGPOOL_SIZE, sizeof(*s_strings))));
        s_initialised = true;
    } else {
        memset(s_strings, 0, STRINGPOOL_SIZE * sizeof(*s_strings));
        bumper_reset(&s_arena);   
    }
}

typedef struct{
    uint32_t index;
    struct{
        union{
            uint8_t all;
            struct{
                unsigned is_error:1;
                unsigned is_found:1;
            };
        };
    }flags;
}SPoolCalculatedIndex_t;

static SPoolCalculatedIndex_t calculate_index(char* string){
    uint32_t start_pos = djb2_hash(string) % STRINGPOOL_SIZE;
    SPoolCalculatedIndex_t retval = {0};

    for(uint32_t i = 0; i < STRINGPOOL_SIZE; i++){
        uint32_t current_idx = (start_pos + i) % STRINGPOOL_SIZE;
        if(s_strings[current_idx] == NULL){
            retval.index = current_idx;
            retval.flags.is_found = 0;
            retval.flags.is_error = 0;
            goto exit;
        } else if(strcmp(string, s_strings[current_idx]) == 0){
            retval.index = current_idx;
            retval.flags.is_found = 1;
            retval.flags.is_error = 0;
            goto exit;
        }
    }

    retval.flags.is_error = 1;
exit:
    return retval;
}

int stringpool_add(char* string){
    if(!string) return -1;

    SPoolCalculatedIndex_t index = calculate_index(string);
    if(index.flags.is_found) return index.index;
    if(index.flags.is_error) return -1;

    char* string_copy = bumper_strdup(&s_arena, string);
    if(string_copy){
        s_strings[index.index] = string_copy;
        return index.index; 
    } else return -1;
}

char* stringpool_get(int index){
    if(index < 0 || index >= STRINGPOOL_SIZE)
        return NULL;

    return s_strings[index];
}

