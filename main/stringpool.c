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

//========================== PREEMTIVE SUPPORT 
#ifdef TARGET_ESPIDF
#include "freertos/freeRTOS.h"
#include "freertos/sem.h"
static SemaphoreHandle_t s_stringpool_lock = NULL;
#else
#include <pthread.h>
static pthread_mutex_t s_stringpool_lock = {0};
#endif

static void stringpool_enter_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_lock(&s_stringpool_lock);
    #else
    xSemaphoreTakeRecursive(s_stringpool_lock, portMAX_DELAY);
    #endif
}

static void stringpool_exit_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_unlock(&s_stringpool_lock);
    #else
    xSemaphoreGiveRecursive(s_stringpool_lock);
    #endif    
}

//=================================================

static uint32_t djb2_hash(char *str) {
        uint32_t hash = 5381;
        int c;
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
}

void stringpool_init(){
    if(!s_initialised){
        #ifdef TARGET_LINUX
        pthread_mutexattr_t attr = {0};
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&s_stringpool_lock, &attr);
        #else
        s_stringpool_lock = xSemaphoreCreateRecursiveMutex();
        assert(s_stringpool_lock);
        #endif

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
    int value = -1;
    if(!string) return -1;

    stringpool_enter_critical();

    SPoolCalculatedIndex_t index = calculate_index(string);
    if(index.flags.is_found) {value = index.index; goto exit;}
    if(index.flags.is_error) goto exit;

    char* string_copy = bumper_strdup(&s_arena, string);
    if(string_copy){
        s_strings[index.index] = string_copy;
        value = index.index;

        goto exit;
    };

exit:
    stringpool_exit_critical();
    return value;
}

char* stringpool_get(int index){
    if(index < 0 || index >= STRINGPOOL_SIZE)
        return NULL;

    return s_strings[index];
}

