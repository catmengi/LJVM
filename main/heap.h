#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "class.h"
#include "thread.h"

#define GC_MARK_SENTINEL (uint32_t)0xFFFFFFFF

typedef struct Object_t Object_t;
typedef struct Object_t{
    struct list_head list; //this list gonna be used for GCing, i.e. for scanning objects without big recursion going
    uint32_t forward;
    Class_t* class;
}Object_t;

typedef struct{
    size_t count;
    JavaValueType_t type;

    void* array;
}ArrayObject_t;

//Object_t header
//==================
//(int32_t fields[class->fields_count[1]] || ArrayObject_t header)
//==========================================
//int(32/64)_t element[array->count];

void heap_init();

void heap_gc_thread_register(Thread_t* thread);
void heap_gc_thread_unregister(Thread_t* thread);

void heap_gc_start();

Error_t heap_class_object_alloc(Class_t* class, int32_t* output);
Error_t heap_class_object_get_fields(Object_t* object, int32_t** output);