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
#include "jerror.h"

#include <stdatomic.h>

typedef struct Thread_t Thread_t;
typedef struct Object_t Object_t;
typedef struct Monitor_t{
    atomic_flag spinlock; 

    struct list_head list;
    struct list_head enter_set; //list of threads that awaiting the objects unlocking
    struct list_head wait_set; //Object.wait()

    Object_t* owner_object;
    Thread_t* owner;
    uint32_t recursion;
}Monitor_t;

void monitors_init();

Error_t monitor_enter(Object_t* object);
Error_t monitor_exit(Monitor_t* monitor);
Error_t monitor_exit_force(Monitor_t* monitor);

Error_t monitor_wait(Object_t* object);
Error_t monitor_waitTimeout(Object_t* object,int64_t timeout);
Error_t monitor_notify(Object_t* object);
Error_t monitor_notifyAll(Object_t* object);

void monitor_free(Object_t* object);