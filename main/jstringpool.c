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

#include "config.h"
#include "heap.h"
#include "jerror.h"
#include "stringpool.h"
#include "jstringpool.h"

#include <string.h>
#include <assert.h>

static JavaStringPoolEntry_t s_stringpool[JAVASTRINGPOOL_SIZE] = {0};

void jstringpool_init(){
    memset(s_stringpool, 0, sizeof(s_stringpool));
}

JavaStringPoolEntry_t* jstringpool_get_pool(){
    return s_stringpool;
}

static uint32_t hash(uint16_t name_id) {
    uint32_t x = (uint32_t)name_id;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
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
}CalculatedIndex_t;

static CalculatedIndex_t calculate_index(uint16_t name_id){
    uint32_t start_pos = hash(name_id) % JAVASTRINGPOOL_SIZE;
    CalculatedIndex_t retval = {0};

    for(uint32_t i = 0; i < JAVASTRINGPOOL_SIZE; i++){
        uint32_t current_idx = (start_pos + i) % JAVASTRINGPOOL_SIZE;
        if(s_stringpool[current_idx].object == NULL){
            retval.index = current_idx;
            retval.flags.is_found = 0;
            retval.flags.is_error = 0;
            goto exit;
        } else if(s_stringpool[current_idx].name_id == name_id){
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

//Hack to properly intern java strings beetween classes
Error_t jstringpool_get(uint16_t name_id, Object_t** output){
    Error_t err = JERR_OK;

    CalculatedIndex_t index = calculate_index(name_id);
    if(index.flags.is_found == 0 && index.flags.is_error == 0){

        //Hard coded garbage that i need to refactor in future, probably with something like sun.Unsafe
        char* string = stringpool_get(name_id);
        Class_t* string_class = NULL;
        Object_t* String = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/String"), &string_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(string_class, &String)) == JERR_OK, err, err, exit);

        int32_t* storage = NULL;
        FAIL_SET_JUMP((err = heap_class_object_get_fields(String, &storage)) == JERR_OK, err, err, exit);

        Field_t *value, *offset, *count;
        assert((value = class_find_instance_field(string_class, stringpool_add("value@[C"))));
        assert((offset = class_find_instance_field(string_class, stringpool_add("offset@I"))));
        assert((count = class_find_instance_field(string_class, stringpool_add("count@I"))));

        storage[offset->offset] = 0;
        storage[count->offset] = strlen(string); //TODO: proper utf8 support

        Class_t* chararray_class = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("[C"), &chararray_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_alloc(chararray_class, storage[count->offset], (Object_t**)&storage[value->offset])) == JERR_OK, err, err, exit);

        int16_t* elements = NULL;
        FAIL_SET_JUMP((err = heap_array_object_get_elements((Object_t*)storage[value->offset], (void**)&elements)) == JERR_OK, err, err, exit);

        //TODO: proper utf8 support! 
        for(unsigned i = 0; i < storage[count->offset]; i++){
            elements[i] = string[i];
        }

        s_stringpool[index.index].name_id = name_id;
        s_stringpool[index.index].object = String;
        *output = String;

    } else if(index.flags.is_found){
        *output = s_stringpool[index.index].object;
    } else return JERR_OOM;

exit:
    return err;
}
