#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "class.h"
#include "thread.h"

#define GC_MARK_SENTINEL (void*)0xFFFFFFFF

typedef struct Object_t Object_t;
typedef struct Object_t{
    struct list_head list; //this list gonna be used for GCing, i.e. for scanning objects without big recursion going
    Object_t* forward;
    Monitor_t* monitor;
    Class_t* class;
}Object_t;

//Object_t header
//==================
//(int32_t fields[class->fields_count[1]] || int32_t array_length)
//==========================================
//int(32/64)_t element[array->count];

void heap_init();

void heap_gc_thread_register(Thread_t* thread);
void heap_gc_thread_unregister(Thread_t* thread);

void heap_gc_start();

Error_t heap_class_object_alloc(Class_t* class, Object_t** output);
Error_t heap_class_object_get_fields(Object_t* object, int32_t** output);

Error_t heap_array_object_alloc(Class_t* class, int32_t length, Object_t** output);
Error_t heap_array_object_get_length(Object_t* object, int32_t* output);
Error_t heap_array_object_get_elements(Object_t* object, void** output);