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

typedef struct Object_t Object_t;
typedef struct{
    char* cstr;
}StringpoolItem_t;

typedef struct{
    struct list_head list;
    //atomic_flag spinlock;
    StringpoolItem_t items[STRINGPOOL_ENTRY_ITEMS_COUNT];
}StringpoolEntry_t;

void stringpool_init();

//Return: ID of "string" inside stringpool or negative value on error (-1)
int32_t stringpool_add(char* string);

//Return: null-terminated string that associated with ID("index") or NULL
char* stringpool_get(int32_t index);