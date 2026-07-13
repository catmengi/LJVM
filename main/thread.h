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

#include "interpreter.h"
#include "list.h"
#include "monitor.h"

#include <stdbool.h>

#ifdef TARGET_ESPIDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <pthread.h>
#endif

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

enum{
    THREAD_ARG_SELF = 0,
    THREAD_ARG_METHOD = 1,
    THREAD_ARG_ARGS = 2,

    THREAD_ARG_MAX,
};

typedef struct Thread_t{
    struct list_head list; //List used for monitor operations / safepoint waiting / waiting for thread exit (join)
    struct list_head joiners; //List of threads that want to join us
    struct list_head gc_list; //required for GC scanning
    void* startup_args[THREAD_ARG_MAX]; //Startup arguments array (thread_start want so pass them somehow + init() can modify them)

    Object_t* jlThread; //java.lang.Thread. Need for GC

    #ifdef TARGET_ESPIDF
    TaskHandle_t task;
    #else
    pthread_t task;
    pthread_mutex_t notify_mutex;
    pthread_cond_t notify_condvar;
    #endif

    Interpreter_t interpreter;
    unsigned wake_recursion; //if 0 then thread is NOT in waiting state
}Thread_t;


typedef struct Object_t Object_t;

Thread_t* thread_self_get();
Thread_t* thread_alloc(Object_t* jlThread);

void thread_start(Thread_t* thread, Method_t* method, int32_t* args);
void thread_sleep(uint32_t ms);

uint64_t thread_time_ns_get();

void thread_safepoint_request();
void thread_safepoint_check();
void thread_safepoint_release();