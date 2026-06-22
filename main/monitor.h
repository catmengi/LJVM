#pragma once

#include <stdint.h>
#include "list.h"
#include "jerror.h"

typedef struct Thread_t Thread_t;
typedef struct Object_t Object_t;
typedef struct Monitor_t{
    struct list_head list;
    struct list_head awaiters; //list of threads that awaiting the objects unlocking
    
    Object_t* owner_object;
    Thread_t* owner;
    uint32_t recursion;
}Monitor_t;

void monitors_init();

Error_t monitor_enter(Object_t* object, Thread_t* thread);
Error_t monitor_exit(Monitor_t* monitor, Thread_t* thread);
void monitor_free(Object_t* object);