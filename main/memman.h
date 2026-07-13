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

#include "bumper.h"
#include "list.h"

typedef struct{
    struct list_head list;
    int id;

    bump_allocator_t arena;
}MemmanChunk_t;

void memman_init();

//WARNING: it does NOT check for existance of the arena with same ID
bump_allocator_t* memman_create(int id, size_t size);
bump_allocator_t* memman_get(int id);