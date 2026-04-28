#pragma once

#include "freertos/idf_additions.h"
#include "jeex.h"
#include "list.h"
#include "freertos/freeRTOS.h"
#include "freertos/task.h"

#define VM_MAX_THREADS 16
#define VM_THREAD_STACKBUF_SIZE 768 //In uint32_ts
#define VM_MAX_METHOD_CALL_DEPTH 128
#define VM_MAX_OBJECTS 8192

typedef struct VM_t VM_t;

typedef enum{
    EVMERR_OK,
    EVMERR_BADPARAM,
    EVMERR_UNKNOWN,
    EVMERR_OOM,
    EVMERR_NOTFOUND,
}VMError_t;

typedef struct{
    JEEXMethod_t* method;
    JEEXClass_t* class;

    uint8_t* pc; //Point to current opcode inside method
    int32_t* stack;
    int32_t* locals;
}VMFrame_t;

typedef struct{
    uint16_t csp;
    VMFrame_t frames[VM_MAX_METHOD_CALL_DEPTH];
}VMCallStack_t;

typedef struct{
    VM_t* vm;
    struct list_head thread_list;
    struct list_head wait_list;

    VMCallStack_t call_stack;
    int32_t stackbuf[VM_THREAD_STACKBUF_SIZE]; //buffer for operand stack and local variables
 
    TaskHandle_t task_handle;
}VMThread_t;

typedef struct{
    unsigned handles_count;
    unsigned alloc_bitmap_size;

    void** handles; //MUST be in SRAM!
    uint32_t* alloc_bitmap; //Must be sized as VM_MAX_OBJECTS / (sizeof(uint32_t) * 8). Preferable in SRAM
    uint16_t* sort_table; //Sort table that used to properly compact objects. Should be in PSRAM!
    unsigned objects_count;
}VMGC_t;

typedef struct VM_t{
    JEEXHeader_t* jeex_image;

    struct list_head used_threads;
    struct list_head free_threads;
    SemaphoreHandle_t thread_lock; //Used to lock thread lists
    
    VMGC_t gc_data;
    uint8_t* static_fields;
    VMThread_t threads[VM_MAX_THREADS];
}VM_t;

VMError_t VM_init(VM_t* vm, JEEXHeader_t* jeex_image);
VMError_t VM_start(VM_t* vm, char* class_name);