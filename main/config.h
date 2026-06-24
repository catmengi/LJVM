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

#define KB * 1024
#define MB * 1024 * 1024

#define CLASS_PERMAMENT_ARENA 1 MB
#define CLASS_TEMPOPARY_ARENA 256 KB
#define PARSER_ARENA 256 KB
#define STRINGPOOL_ARENA 512 KB

#define THREAD_STACK_SIZE 2 KB
#define THREAD_MAX_COUNT 16
#define THREAD_LOWEST_QUOTA 64
#define THREAD_DEFAULT_PRIORITY 5

#define OBJECT_HEAP_SIZE 2 MB
#define MAX_MONITORS 512

void JEspresso_init();