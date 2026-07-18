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

#include <stdint.h>
#include <stdbool.h>
#include "class.h"
#include "thread.h"

#define GC_MARK_SENTINEL (void*)0xFFFFFFFF

typedef struct Object_t Object_t;
typedef struct Object_t{
    struct list_head list; //this list gonna be used for GCing, i.e. for scanning objects without big recursion going
    uint32_t ident;
    Object_t* forward;
    Monitor_t* monitor;
    Class_t* class;
}Object_t;

//Object_t header
//==================
//(int32_t fields[class->fields_count[1]] || int32_t array_length)
//==========================================
//int(32/64)_t element[array->count];

void heap_init();

void heap_gc_thread_register(Thread_t* thread);
void heap_gc_thread_unregister(Thread_t* thread);

void heap_gc_start();

uint32_t heap_object_get_hashcode(Object_t* object);

Error_t heap_class_object_alloc(Class_t* class, Object_t** output);
Error_t heap_class_object_get_fields(Object_t* object, void** output);

int heap_array_type_size(JavaValueType_t type);
Error_t heap_array_object_alloc(Class_t* class, int32_t length, Object_t** output);
Error_t heap_array_object_get_length(Object_t* object, int32_t* output);
Error_t heap_array_object_get_elements(Object_t* object, void** output);
