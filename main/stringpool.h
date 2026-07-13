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
#include "list.h"
#include "config.h"
#include "interpreter.h"

typedef struct Object_t Object_t;
typedef struct{
    char* cstr;
    _Atomic(Object_t*) jstr;
}StringpoolItem_t;

typedef struct{
    struct list_head list;
    //atomic_flag spinlock;
    StringpoolItem_t items[STRINGPOOL_ENTRY_ITEMS_COUNT];
}StringpoolEntry_t;

void stringpool_init();
int32_t stringpool_add(char* string);
char* stringpool_get(int32_t index);
Object_t* stringpool_get_java(Interpreter_t* ctx, int32_t name_id);