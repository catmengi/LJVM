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
#include "class.h"
#include "config.h"
#include "heap.h"
#include "jerror.h"
#include "thread.h"
#include "stringpool.h"

#include <assert.h>

static Monitor_t s_monitors[MAX_MONITORS] = {0};
struct list_head s_free_monitors;

extern struct list_head* threads_get_sleep_list();
extern struct list_head* threads_get_schedule_list();

static void monitor_init(Monitor_t* monitor){
    monitor->owner = NULL;
    INIT_LIST_HEAD(&monitor->list);
    INIT_LIST_HEAD(&monitor->enter_set);
    INIT_LIST_HEAD(&monitor->wait_set);
}

void monitors_init(){
    INIT_LIST_HEAD(&s_free_monitors);

    for(unsigned i = 0; i < MAX_MONITORS; i++){
        Monitor_t* monitor = &s_monitors[i];
        monitor_init(monitor);

        list_add(&monitor->list, &s_free_monitors);
    }
}

static Monitor_t* monitor_alloc(){
    Monitor_t* unused = NULL;
    list_for_each_entry(unused, &s_free_monitors, list){
        list_del(&unused->list);
        monitor_init(unused);
        return unused;
    }

    return NULL;
}

Error_t monitor_enter(Object_t* object, Thread_t* thread){
    if(!object){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL; 
        assert(class_load_bynameid(stringpool_add("java/lang/IncompatibleClassChangeError"), &exception_class) == JERR_OK);
        assert(heap_class_object_alloc(exception_class, &exception) == JERR_OK);
        assert(java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL) == JERR_OK); 

        return thread_throw_exception(thread, exception);            
    }

    if(object->monitor == NULL){
        object->monitor = monitor_alloc();
        if(!object->monitor) return JERR_OOM;
    }

    Monitor_t* monitor = object->monitor;
    if(monitor->owner == NULL || monitor->owner == thread){
        list_del_init(&monitor->list);
        list_add(&monitor->list, &thread->top_frame->held_monitors);

        monitor->owner_object = object;
        monitor->owner = thread;
        monitor->recursion = thread->wake_recursion > 0 ? thread->wake_recursion : monitor->recursion + 1;
        thread->wake_recursion = 0;
        return JERR_OK;
    }

    list_del_init(&thread->list);
    list_add_tail(&thread->list, &monitor->enter_set);

    return JERR_SCHEDULE;
}

Error_t monitor_wait(Object_t* object, Thread_t* thread){
    Error_t err = JERR_OK;

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;
    
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }

    list_del_init(&thread->list);
    list_del_init(&thread->sleep_list); //If it was set somewhere - WTF????

    thread->wake_recursion = monitor->recursion;
    monitor->recursion = 0;
    monitor->owner = NULL;
    list_add(&thread->list, &monitor->wait_set);

    return JERR_SCHEDULE;

exit:
    return err; //Bad return only 
}

extern uint64_t thread_time_ns_get();
Error_t monitor_waitTimeout(Object_t* object, Thread_t* thread, int64_t timeout){
    Error_t err = JERR_OK;

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;
    
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }

    if(timeout < 0 || timeout > 999999){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;
    
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalArgumentException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }

    list_del_init(&thread->list);
    list_del_init(&thread->sleep_list); //If it was set somewhere - WTF????

    thread->wake_recursion = monitor->recursion;

    monitor->recursion = 0;
    monitor->owner = NULL;

    if(timeout > 0){
        thread->wakeup_on = timeout + thread_time_ns_get();
        list_add(&thread->sleep_list, threads_get_sleep_list());
    }

    list_add(&thread->list, &monitor->wait_set);

    return JERR_SCHEDULE;
exit:
    return err;
}

Error_t monitor_notify(Object_t* object, Thread_t* thread){
    Error_t err = JERR_OK;

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;
    
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }
    
    Thread_t *to_notify = NULL, *tmp = NULL;
    list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
        list_del_init(&to_notify->sleep_list);
        list_del_init(&to_notify->list);

        list_add_tail(&to_notify->list, &monitor->enter_set);
        break;
    }

exit:
    return err;
}

Error_t monitor_notifyAll(Object_t* object, Thread_t* thread){
    Error_t err = JERR_OK;

    Monitor_t* monitor = object->monitor;
    if(monitor == NULL || monitor->owner != thread){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;
    
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }
    
    Thread_t *to_notify = NULL, *tmp = NULL;
    list_for_each_entry_safe(to_notify, tmp, &monitor->wait_set, list){
        list_del_init(&to_notify->sleep_list);
        list_del_init(&to_notify->list);

        list_add_tail(&to_notify->list, &monitor->enter_set);
    }

exit:
    return err;
}

Error_t monitor_exit(Monitor_t* monitor, Thread_t* thread){
    Error_t err = JERR_OK;

    if(monitor->owner != thread){
        Object_t* exception = NULL;
        Class_t* exception_class = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = java_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL)) == JERR_OK, err, err, exit); 

        return thread_throw_exception(thread, exception);
    }

    if(--monitor->recursion == 0){
        list_del_init(&monitor->list);
        monitor->owner_object = NULL;
        monitor->owner = NULL;


        Thread_t *wakeup = NULL, *tmp = NULL;
        list_for_each_entry_safe(wakeup, tmp, &monitor->enter_set, list){
            list_del_init(&wakeup->list);
            list_add_tail(&wakeup->list, threads_get_schedule_list());
            break;
        }
    }

exit:
    return err;
}

void monitor_free(Object_t* object){
    Monitor_t* monitor = object->monitor;
    object->monitor = NULL;

    if(monitor){
        object->monitor = NULL;
        list_del_init(&monitor->list);
        list_add(&monitor->list, &s_free_monitors);
    }
}