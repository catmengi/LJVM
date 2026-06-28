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

#include "thread.h"
#include "class.h"
#include "config.h"
#include "heap.h"
#include "interpreter.h"
#include "list.h"
#include "monitor.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>

static bool is_initialised = false;

static __thread Thread_t* s_current_thread = NULL;

static LIST_HEAD(s_safepoint_threads);
static atomic_flag s_safepoint_threads_list_guard = ATOMIC_FLAG_INIT;
static atomic_bool s_safepoint_requested = false;
static atomic_size_t s_active_threads = 0;
//static atomic_size_t s_parked_threads = 0;

void threads_init(){
    s_safepoint_requested = false;
    s_active_threads = 0;
    //s_parked_threads = 0;
}


inline Thread_t* thread_self_get(){return s_current_thread;}
void thread_notify_wait(){
    atomic_fetch_sub(&s_active_threads, 1);
    //atomic_fetch_add_explicit(&s_parked_threads, 1, memory_order_seq_cst);

    #ifdef TARGET_LINUX
    pthread_mutex_lock(&thread_self_get()->notify_mutex);
    assert(pthread_cond_wait(&thread_self_get()->notify_condvar, &thread_self_get()->notify_mutex) == 0);
    pthread_mutex_unlock(&thread_self_get()->notify_mutex);
    #else
    static_assert("TODO: freeRTOS");
    #endif

    //atomic_fetch_sub_explicit(&s_parked_threads, 1, memory_order_seq_cst);
    atomic_fetch_add(&s_active_threads, 1);
}

void thread_notify_timedwait(uint64_t ns){
    atomic_fetch_sub(&s_active_threads, 1);
    //atomic_fetch_add_explicit(&s_parked_threads, 1, memory_order_seq_cst);


    //TODO: make it in fact timed
    #ifdef TARGET_LINUX
    pthread_mutex_lock(&thread_self_get()->notify_mutex);
    assert(pthread_cond_wait(&thread_self_get()->notify_condvar, &thread_self_get()->notify_mutex) == 0);
    pthread_mutex_unlock(&thread_self_get()->notify_mutex);
    #else
    static_assert("TODO: freeRTOS");
    #endif

    //atomic_fetch_sub_explicit(&s_parked_threads, 1, memory_order_seq_cst);
    atomic_fetch_add(&s_active_threads, 1);
}

void thread_notify_send(Thread_t* thread){
    #ifdef TARGET_LINUX
    pthread_mutex_lock(&thread->notify_mutex);
    pthread_cond_signal(&thread->notify_condvar);
    pthread_mutex_unlock(&thread->notify_mutex);
    #else
    static_assert("TODO: freeRTOS");
    #endif
}

void thread_safepoint_request(){
    //atomic_fetch_sub(&s_active_threads, 1);
    if(!atomic_exchange(&s_safepoint_requested, true)){
        while(atomic_load(&s_active_threads) != 1){
            usleep(1000); //Should be enough
        }
    } else thread_safepoint_check();
    //atomic_fetch_add(&s_active_threads, 1);
}

inline void thread_safepoint_check(){
    if(atomic_load(&s_safepoint_requested) == true){
        while(atomic_flag_test_and_set(&s_safepoint_threads_list_guard)) {}
        list_add(&thread_self_get()->list, &s_safepoint_threads);
        atomic_flag_clear(&s_safepoint_threads_list_guard);

        while(atomic_load(&s_safepoint_requested) == true) thread_notify_wait();
    }
}

inline void thread_safepoint_release(){
    atomic_store(&s_safepoint_requested, false);

    Thread_t *thread = NULL, *tmp = NULL;
    list_for_each_entry_safe(thread, tmp, &s_safepoint_threads, list){
        list_del_init(&thread->list);
        thread_notify_send(thread);
    }
}


Thread_t* thread_alloc(){
    Thread_t* new_thread = calloc(1, sizeof(*new_thread));

    if(new_thread){
        INIT_LIST_HEAD(&new_thread->joiners);
        INIT_LIST_HEAD(&new_thread->list);
        INIT_LIST_HEAD(&new_thread->gc_list);

        #ifdef TARGET_LINUX
        pthread_mutex_init(&new_thread->notify_mutex, NULL);
        pthread_cond_init(&new_thread->notify_condvar, NULL);
        #endif

        new_thread->init = NULL; //TODO: set default method to startup interpreter
        new_thread->wake_recursion = 0;
    }

    return new_thread;
}

#ifdef TARGET_LINUX
static void*
#else
static void
#endif
thread_task(void* params){
    Thread_t* thread = ((void**)params)[0];
    s_current_thread = thread;

    atomic_fetch_add_explicit(&s_active_threads, 1, memory_order_seq_cst);
    interpreter_ctx_init(&thread->interpreter);
    heap_gc_thread_register(thread);

    if(thread->init) thread->init();

    Method_t* method = ((void**)params)[1];
    int32_t* method_args = ((void**)params)[2];


    InterpreterFrame_t* launch_frame = interpreter_frame_push(method);
    assert(launch_frame);

    memcpy(launch_frame->locals, method_args, method->args_slots * sizeof(int32_t));
    interpreter_execute();

    thread_kill();
}

void thread_start(Thread_t* thread, Method_t* method, int32_t* args){
    thread->startup_args[0] = thread;
    thread->startup_args[1] = method;
    thread->startup_args[2] = args;

    #ifdef TARGET_LINUX
    assert(pthread_create(&thread->task, NULL, thread_task, thread->startup_args) == 0);
    #else
    assert(xTaskCreate(thread_task, "java_thread", 4096, thread->startup_args, THREAD_DEFAULT_PRIORITY, &thread->task) == pdPASS);
    #endif
}

void thread_free(Thread_t* thread);

_Noreturn void thread_kill(){
    atomic_fetch_sub_explicit(&s_active_threads, 1, memory_order_seq_cst);

    Thread_t* thread = thread_self_get();
    Thread_t* wakeup = NULL, *tmp = NULL;
    list_for_each_entry_safe(wakeup, tmp, &thread->joiners, list){
        list_del_init(&wakeup->list);
        thread_notify_send(wakeup);
    }

    thread_free(thread);

    #ifdef TARGET_LINUX
    pthread_detach(pthread_self());
    pthread_exit(NULL);
    #else
    vTaskDelete(NULL);
    #endif
}

void thread_free(Thread_t* thread){
    heap_gc_thread_unregister(thread);
    free(thread);
}

void thread_join(Thread_t* join_to){
    Thread_t* thread = thread_self_get();

    list_del_init(&thread->list);
    list_add(&thread->list, &join_to->joiners);

    thread_notify_wait();
    thread_safepoint_check();
}

uint64_t thread_time_ns_get(){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return ts.tv_nsec + ((uint64_t)ts.tv_sec * 1e+9);
}

void thread_sleep(uint32_t ms){
    atomic_fetch_sub_explicit(&s_active_threads, 1, memory_order_seq_cst);

    usleep(ms * 1000000);

    atomic_fetch_add_explicit(&s_active_threads, 1, memory_order_seq_cst);
    thread_safepoint_check();
}