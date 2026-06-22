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

static void monitor_init(Monitor_t* monitor){
    monitor->owner = NULL;
    INIT_LIST_HEAD(&monitor->list);
    INIT_LIST_HEAD(&monitor->awaiters);
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
        monitor->recursion++;
        return JERR_OK;
    }

    list_del_init(&thread->list);
    list_add_tail(&thread->list, &monitor->awaiters);

    return JERR_SCHEDULE;
}


extern struct list_head* threads_get_schedule_list();
Error_t monitor_exit(Monitor_t* monitor, Thread_t* thread){
    Error_t err = JERR_OK;

    if(monitor->owner != thread){
        Object_t* exception = NULL;
        Class_t* exception_class = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/IllegalMonitorStateException"), &exception_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);

        return thread_throw_exception(thread, exception);
    }

    if(--monitor->recursion == 0){
        list_del_init(&monitor->list);
        monitor->owner_object = NULL;
        monitor->owner = NULL;


        Thread_t *wakeup = NULL, *tmp = NULL;
        list_for_each_entry_safe(wakeup, tmp, &monitor->awaiters, list){
            list_del_init(&wakeup->list);
            list_add_tail(&wakeup->list, threads_get_schedule_list());
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