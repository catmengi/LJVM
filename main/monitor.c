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

#include "monitor.h"
#include "config.h"
#include "heap.h"
#include "jerror.h"
#include "thread.h"

#include <limits.h>

#ifdef TARGET_ESPIDF
#include "freertos/freeRTOS.h"
#include "freertos/sem.h"
static SemaphoreHandle_t s_monitor_lock = NULL;
#else
#include <pthread.h>
static pthread_mutex_t s_monitor_lock = {0};
#endif

static Monitor_t s_monitors[MAX_MONITORS] = {0};
static struct list_head s_free_monitors;
static bool is_initialised = false;

//=============== INTERNAL HELPERS ===============
static void monitor_enter_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_lock(&s_monitor_lock);
    #else
    xSemaphoreTakeRecursive(s_monitor_lock, portMAX_DELAY);
    #endif
}

static void monitor_exit_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_unlock(&s_monitor_lock);
    #else
    xSemaphoreGiveRecursive(s_monitor_lock);
    #endif    
}

static void monitor_init(Monitor_t* monitor){
    monitor->owner = NULL;
    INIT_LIST_HEAD(&monitor->list);
    INIT_LIST_HEAD(&monitor->enter_set);
    INIT_LIST_HEAD(&monitor->wait_set);
}

extern void thread_notify_wait();
extern void thread_notify_timedwait(uint64_t ns);
extern void thread_notify_send(Thread_t* thread);

//==============================================

void monitors_init(){
    INIT_LIST_HEAD(&s_free_monitors);

    if(!is_initialised){
        #ifdef TARGET_LINUX
        pthread_mutexattr_t attr = {0};
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&s_monitor_lock, &attr);
        #else
        s_monitor_lock = xSemaphoreCreateRecursiveMutex();
        assert(s_monitor_lock);
        #endif
        is_initialised = true;
    }

    for(unsigned i = 0; i < MAX_MONITORS; i++){
        Monitor_t* monitor = &s_monitors[i];
        monitor_init(monitor);

        list_add(&monitor->list, &s_free_monitors);
    }
}

static Monitor_t* monitor_alloc(){
    monitor_enter_critical();

    Monitor_t* unused = NULL;
    list_for_each_entry(unused, &s_free_monitors, list){
        list_del(&unused->list);
        monitor_init(unused);

        monitor_exit_critical();
        return unused;
    }

    monitor_exit_critical();
    return NULL;
}

Error_t monitor_enter(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;

    if(object->monitor == NULL){
        object->monitor = monitor_alloc();
        if(!object->monitor) return JERR_OOM;
    }

    monitor_enter_critical();
    
    Monitor_t* monitor = object->monitor;
    if(monitor->owner == NULL || monitor->owner == thread){

        if(monitor->recursion == 0){
            list_del_init(&monitor->list);
            list_add(&monitor->list, &thread->interpreter.frame->held_monitors);
        }

        monitor->owner_object = object;
        monitor->owner = thread;
        monitor->recursion = thread->wake_recursion > 0 ? thread->wake_recursion : monitor->recursion + 1;
        thread->wake_recursion = 0;

        monitor_exit_critical();
        thread_safepoint_check();
        return JERR_OK;
    }

    list_del_init(&thread->list);
    list_add_tail(&thread->list, &monitor->enter_set);

    monitor_exit_critical();
    thread_notify_wait();

    thread_safepoint_check();
    return JERR_OK;
}

Error_t monitor_exit(Monitor_t* monitor){
    Thread_t* thread = thread_self_get();

    monitor_enter_critical();
    if(monitor->owner != thread) return JERR_INVALIDMONITORSTATE;

    if(--monitor->recursion == 0){
        list_del_init(&monitor->list);
        monitor->owner_object = NULL;
        monitor->owner = NULL;


        Thread_t *wakeup = NULL, *tmp = NULL;
        list_for_each_entry_safe(wakeup, tmp, &monitor->enter_set, list){
            list_del_init(&wakeup->list);
            thread_notify_send(wakeup);
            break;
        }
    }


    monitor_exit_critical();
    return JERR_OK;
}

Error_t monitor_exit_force(Monitor_t* monitor){
    Thread_t* thread = thread_self_get();

    monitor_enter_critical();
    if(monitor->owner != thread) return JERR_INVALIDMONITORSTATE;

    list_del_init(&monitor->list);
    monitor->owner_object = NULL;
    monitor->owner = NULL;


    Thread_t *wakeup = NULL, *tmp = NULL;
    list_for_each_entry_safe(wakeup, tmp, &monitor->enter_set, list){
        list_del_init(&wakeup->list);
        thread_notify_send(wakeup);
    }


    monitor_exit_critical();
    return JERR_OK;    
}

Error_t monitor_wait(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;

    monitor_enter_critical();

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread){
        monitor_exit_critical();
        return JERR_INVALIDMONITORSTATE;
    }


    list_del_init(&thread->list);
    list_del_init(&monitor->list); //Remove from held

    thread->wake_recursion = monitor->recursion;
    monitor->recursion = 0;
    monitor->owner = NULL;

    list_add(&thread->list, &monitor->wait_set);

    monitor_exit_critical(); //Must release the lock, otherwise we are deeeply in trouble
    thread_notify_wait();


    return monitor_enter(object);
}

extern uint64_t thread_time_ns_get();
Error_t monitor_waitTimeout(Object_t* object, int64_t timeout){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;

    Monitor_t* monitor = object->monitor;

    monitor_enter_critical();
    if(monitor == NULL || monitor->owner != thread){
        monitor_exit_critical();
        return JERR_INVALIDMONITORSTATE;
    }


    list_del_init(&thread->list);
    list_del_init(&monitor->list); //Remove from held

    thread->wake_recursion = monitor->recursion;

    monitor->recursion = 0;
    monitor->owner = NULL;

    monitor_exit_critical(); //Must release the lock, otherwise we are deeeply in trouble

    thread_notify_timedwait(timeout);

    return monitor_enter(object);
}

Error_t monitor_notify(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;
    
    monitor_enter_critical();
    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread) return JERR_INVALIDMONITORSTATE;
    
    Thread_t *to_notify = NULL, *tmp = NULL;
    list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
        list_del_init(&to_notify->list);
        thread_notify_send(to_notify);
        break;
    }

    monitor_exit_critical();
    return JERR_OK;
}

Error_t monitor_notifyAll(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;

    monitor_enter_critical();

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread) return JERR_INVALIDMONITORSTATE;
    Thread_t *to_notify = NULL, *tmp = NULL;
    list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
        list_del_init(&to_notify->list);
        thread_notify_send(to_notify);
    }

    monitor_exit_critical();
    return JERR_OK;
}

void monitor_free(Object_t* object){
    monitor_enter_critical();

    Monitor_t* monitor = object->monitor;
    object->monitor = NULL;

    if(monitor){
        monitor->owner_object = NULL;
        list_del_init(&monitor->list);
        list_add(&monitor->list, &s_free_monitors);
    }

    monitor_exit_critical();
}