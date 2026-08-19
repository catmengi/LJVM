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

#include "class.h"
#include "config.h"
#include "parser.h"
#include "stringpool.h"
#include "monitor.h"
#include "memman.h"
#include "classtable.h"

#include <assert.h>

void JEspresso_init(){
    memman_init();
    assert(memman_create(VM_PERMA_ARENA_ID, VM_PERMA_ARENA_SIZE));
    assert(memman_create(VM_LINKER_TMP_ARENA_ID, VM_LINKER_TMP_ARENA_SIZE));
    assert(memman_create(VM_GC_ARENA_ID, VM_GC_ARENA_SIZE));
    assert(memman_create(VM_PARSER_ARENA_ID, VM_PARSER_ARENA_SIZE));

    stringpool_init();
    parser_init();
    //monitors_init();
    classtable_init();
    classes_init();
    //interpreter_init();
}