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
#include "bumper.h"
#include "config.h"
#include "heap.h"
#include "jerror.h"
#include "thread.h"
#include "memman.h"

#include <limits.h>
#include <assert.h>
#include <stdatomic.h>

#define MONITOR_CRITICAL_ENTER(spinlock) ({while(atomic_flag_test_and_set(&(spinlock))){}})
#define MONITOR_CRITICAL_EXIT(spinlock) atomic_flag_clear(&(spinlock))

static bump_allocator_t* s_arena = NULL;
static struct list_head s_free_monitors;
static atomic_flag s_free_monitors_guard = ATOMIC_FLAG_INIT;

//=============== INTERNAL HELPERS ===============
static void monitor_init(Monitor_t* monitor){
    monitor->spinlock = (atomic_flag)ATOMIC_FLAG_INIT;
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
    assert((s_arena = memman_get(VM_PERMA_ARENA_ID)));
    INIT_LIST_HEAD(&s_free_monitors);
}

static Monitor_t* monitor_alloc(){
    //monitor_enter_critical();
    MONITOR_CRITICAL_ENTER(s_free_monitors_guard);

    Monitor_t* unused = NULL;
    list_for_each_entry(unused, &s_free_monitors, list){
        list_del(&unused->list);
        monitor_init(unused);

        MONITOR_CRITICAL_EXIT(s_free_monitors_guard);
        //monitor_exit_critical();
        return unused;
    }

    if(unused == NULL){
        Monitor_t* new = bumper_calloc(s_arena, 1, sizeof(*unused));
        if(new){
            monitor_init(new);
            MONITOR_CRITICAL_EXIT(s_free_monitors_guard);
            //monitor_exit_critical();
            return new;
        }
    }

    MONITOR_CRITICAL_EXIT(s_free_monitors_guard);
    //monitor_exit_critical();
    return NULL;
}

Error_t monitor_enter(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;

    if(object->monitor == NULL){
        object->monitor = monitor_alloc();
        if(!object->monitor) return JERR_OOM;
    }

    //monitor_enter_critical();
    
    Monitor_t* monitor = object->monitor;
    MONITOR_CRITICAL_ENTER(monitor->spinlock);
    if(monitor->owner == NULL || monitor->owner == thread){

        if(monitor->recursion == 0){
            list_del_init(&monitor->list);
            list_add(&monitor->list, &thread->interpreter.frame->held_monitors);
        }

        monitor->owner_object = object;
        monitor->owner = thread;
        monitor->recursion = thread->wake_recursion > 0 ? thread->wake_recursion : monitor->recursion + 1;
        thread->wake_recursion = 0;

        //monitor_exit_critical();
        MONITOR_CRITICAL_EXIT(monitor->spinlock);
        thread_safepoint_check();
        return JERR_OK;
    }

    list_del_init(&thread->list);
    list_add_tail(&thread->list, &monitor->enter_set);

    MONITOR_CRITICAL_EXIT(monitor->spinlock);
    //monitor_exit_critical();
    thread_notify_wait();

    thread_safepoint_check();
    return JERR_OK;
}

Error_t monitor_exit(Monitor_t* monitor){
    Thread_t* thread = thread_self_get();

    MONITOR_CRITICAL_ENTER(monitor->spinlock);
    //monitor_enter_critical();
    if(monitor->owner != thread){
        MONITOR_CRITICAL_EXIT(monitor->spinlock);
        return JERR_INVALIDMONITORSTATE;
    }

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


    MONITOR_CRITICAL_EXIT(monitor->spinlock);
    //monitor_exit_critical();
    return JERR_OK;
}

Error_t monitor_exit_force(Monitor_t* monitor){
    Thread_t* thread = thread_self_get();

    MONITOR_CRITICAL_ENTER(monitor->spinlock);
    //monitor_enter_critical();
    if(monitor->owner != thread){
        MONITOR_CRITICAL_EXIT(monitor->spinlock);
        return JERR_INVALIDMONITORSTATE;
    }

    list_del_init(&monitor->list);
    monitor->owner_object = NULL;
    monitor->owner = NULL;


    Thread_t *wakeup = NULL, *tmp = NULL;
    list_for_each_entry_safe(wakeup, tmp, &monitor->enter_set, list){
        list_del_init(&wakeup->list);
        thread_notify_send(wakeup);
    }


    MONITOR_CRITICAL_EXIT(monitor->spinlock);
    //monitor_exit_critical();
    return JERR_OK;    
}

Error_t monitor_wait(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;
    Monitor_t* monitor = object->monitor;
    if(monitor){
        MONITOR_CRITICAL_ENTER(monitor->spinlock);
        if(monitor->owner == thread){
            list_del_init(&thread->list);
            list_del_init(&monitor->list); //Remove from held

            thread->wake_recursion = monitor->recursion;
            monitor->recursion = 0;
            monitor->owner = NULL;

            list_add(&thread->list, &monitor->wait_set);

            MONITOR_CRITICAL_EXIT(monitor->spinlock); //Must release the lock, otherwise we are deeeply in trouble
            thread_notify_wait();


            return monitor_enter(object);
        } else {MONITOR_CRITICAL_EXIT(monitor->spinlock); return JERR_INVALIDMONITORSTATE;}
    } else return JERR_INVALIDMONITORSTATE;
}

extern uint64_t thread_time_ns_get();
Error_t monitor_waitTimeout(Object_t* object, int64_t timeout){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;
    Monitor_t* monitor = object->monitor;
    if(monitor){
        MONITOR_CRITICAL_ENTER(monitor->spinlock);
        if(monitor->owner == thread){
            list_del_init(&thread->list);
            list_del_init(&monitor->list); //Remove from held

            thread->wake_recursion = monitor->recursion;
            monitor->recursion = 0;
            monitor->owner = NULL;

            list_add(&thread->list, &monitor->wait_set);

            MONITOR_CRITICAL_EXIT(monitor->spinlock); //Must release the lock, otherwise we are deeeply in trouble
            thread_notify_timedwait(timeout);


            return monitor_enter(object);
        } else {MONITOR_CRITICAL_EXIT(monitor->spinlock); return JERR_INVALIDMONITORSTATE;}
    } else return JERR_INVALIDMONITORSTATE;
}

Error_t monitor_notify(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;
    Monitor_t* monitor = object->monitor;

    if(monitor){
        MONITOR_CRITICAL_ENTER(monitor->spinlock);
        if(monitor->owner == thread){
            Thread_t *to_notify = NULL, *tmp = NULL;
            list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
                list_del_init(&to_notify->list);
                thread_notify_send(to_notify);
                break;
            }

            MONITOR_CRITICAL_EXIT(monitor->spinlock);
            return JERR_OK;
        } else {MONITOR_CRITICAL_EXIT(monitor->spinlock); return JERR_INVALIDMONITORSTATE;}
    } else return JERR_INVALIDMONITORSTATE;
}

Error_t monitor_notifyAll(Object_t* object){
    Thread_t* thread = thread_self_get();

    if(!object) return JERR_NULLPOINTER;
    Monitor_t* monitor = object->monitor;

    if(monitor){
        MONITOR_CRITICAL_ENTER(monitor->spinlock);
        if(monitor->owner == thread){
            Thread_t *to_notify = NULL, *tmp = NULL;
            list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
                list_del_init(&to_notify->list);
                thread_notify_send(to_notify);
            }

            MONITOR_CRITICAL_EXIT(monitor->spinlock);
            return JERR_OK;
        } else {MONITOR_CRITICAL_EXIT(monitor->spinlock); return JERR_INVALIDMONITORSTATE;}
    } else return JERR_INVALIDMONITORSTATE;
}

void monitor_free(Object_t* object){
    //monitor_enter_critical();
    MONITOR_CRITICAL_ENTER(s_free_monitors_guard);

    Monitor_t* monitor = object->monitor;
    object->monitor = NULL;

    if(monitor){
        monitor->owner_object = NULL;
        list_del_init(&monitor->list);
        list_add(&monitor->list, &s_free_monitors);
    }

    MONITOR_CRITICAL_EXIT(s_free_monitors_guard);
    //monitor_exit_critical();
}