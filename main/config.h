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

#define TARGET_LINUX
//#define TARGET_ESPIDF

#define KB * 1024
#define MB * 1024 * 1024

#define VM_PERMA_ARENA_SIZE ((1024 + 512) KB)
#define VM_LINKER_TMP_ARENA_SIZE (256 KB)
#define VM_PARSER_ARENA_SIZE (256 KB)
#define VM_GC_ARENA_SIZE (2 MB)

#define STRINGPOOL_ENTRY_ITEMS_COUNT 1024
#define CLASSTABLE_ENTRY_ITEMS_COUNT 1024

#define VM_ARENA_SIZE VM_PERMA_ARENA_SIZE + VM_LINKER_TMP_ARENA_SIZE + VM_GC_ARENA_SIZE + VM_PARSER_ARENA_SIZE + (256 KB)

#define VM_PERMA_ARENA_ID 15923
#define VM_LINKER_TMP_ARENA_ID 6647
#define VM_GC_ARENA_ID 1156
#define VM_PARSER_ARENA_ID 1984

#define THREAD_STACK_SIZE 8 KB
#define THREAD_LOWEST_QUOTA 64
#define THREAD_DEFAULT_PRIORITY 5

void JEspresso_init();