#pragma once
#include "class_linker.h"
#include "list.h"
#include "os_support.h"

typedef enum{
    EJVT_BYTE = 'B',
    EJVT_CHAR = 'C',
    EJVT_DOUBLE = 'D',
    EJVT_FLOAT = 'F',
    EJVT_INT = 'I',
    EJVT_LONG = 'J',
    EJVT_REFERENCE = 'L',
    EJVT_SHORT = 'S',
    EJVT_BOOL = 'Z',
    EJVT_ARRAYDIM = '[',
    EJVT_VOID = 'V',
    EJVT_CODEADDR = 'A',
    EJVT_NATIVEPTR = '*',
}jvm_value_type_t;

typedef struct{
    jvm_value_type_t type;
    uint8_t value[sizeof(uint64_t)];
}jvm_value_t;

typedef struct{
    uint16_t sp;
    jvm_value_t* stack;
}jvm_stack_t;

typedef struct jvm_instance_t jvm_instance_t;
typedef struct jvm_frame_t jvm_frame_t;
typedef struct jvm_frame_t{
    jvm_instance_t* jvm;

    classlinker_method_t* method;

    int64_t pc; //Negative because of some for loop issues with gotos
    jvm_stack_t stack;
    jvm_value_t* locals;

    jvm_frame_t* previous_frame;
}jvm_frame_t;

typedef struct{
    struct list_head list;

    pthread_t thread;
    pthread_mutex_t mutex;
    
    jvm_frame_t* topmost_frame;
}jvm_thread_t;

typedef struct{ //I hate C include system sometimes
    struct list_head object_list;
    void* gc_heap;
    uint32_t gc_heapsize;
}objectmanager_heap_t;

typedef struct jvm_instance_t{
    Arena* arena;
    classlinker_instance_t* linker;

    objectmanager_heap_t heap;
    struct list_head threads;
}jvm_instance_t;

