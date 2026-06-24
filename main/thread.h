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
#include "config.h"
#include "list.h"
#include "jerror.h"
#include "monitor.h"

#include <stdint.h>

typedef struct Method_t Method_t;
typedef struct Monitor_t Monitor_t;
typedef struct Object_t Object_t;

#define SHADOW_CLEAR_REF(bitmap, idx)  ((bitmap)[(idx) >> 5] &= ~(1U << ((idx) & 31)))
#define SHADOW_SET_REF(bitmap, idx)    ((bitmap)[(idx) >> 5] |= (1U << ((idx) & 31)))
#define SHADOW_GET_REF(bitmap, idx)    (((bitmap)[(idx) >> 5] & (1U << ((idx) & 31))) ? 1 : 0)

typedef struct CallFrame_t CallFrame_t;
typedef struct CallFrame_t{
    size_t frame_size; //Used because of arena
    struct list_head held_monitors; //Inside of CallFrame_t because of java semantics

    uint8_t* pc;

    uint32_t sp;
    int32_t* stack;
    uint32_t* shadow_stack;

    int32_t* locals;
    uint32_t* shadow_locals;

    Method_t* method;
    CallFrame_t* prev;
}CallFrame_t;

/*typedef enum{
    THREAD_PSEUDO,
    THREAD_ACTIVE,
}ThreadState_t;*/

typedef struct Thread_t{
    struct list_head list;
    struct list_head sleep_list; //Hack for future Object.wait() with timeout
    struct list_head gc_list;
    struct list_head joiners; //List of threads that want to join us

    //ThreadState_t state;
    int opcode_quota;
    unsigned wake_recursion; //if 0 then thread is NOT in waiting state

    bump_allocator_t frame_allocator; //Initialised on VM startup
    CallFrame_t* top_frame;

    uint64_t wakeup_on; //Time when thread should wakeup if in sleep list
    char stackbuf[THREAD_STACK_SIZE];   
}Thread_t;


typedef struct Object_t Object_t;
void threads_init();

Error_t thread_schedule();
Thread_t* thread_alloc();

void thread_start(Thread_t* thread, Method_t* method, int32_t* args);
void thread_kill(Thread_t* thread);
void thread_sleep(Thread_t* thread, uint32_t ms);

Error_t thread_throw_exception(Thread_t* thread, Object_t* exception_object);
Error_t java_method_invoke(Method_t* method, int32_t* arguments, void* return_value);