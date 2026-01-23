#pragma once

#include "bumper.h"
#include "class.h"

#include "list.h"
#include "ljvm.h"
#include "object.h"

#include <setjmp.h>
#include <stdatomic.h>

typedef struct JHandle_frame_t JHandle_frame_t;
typedef struct JFrame_t JFrame_t;
typedef struct JHandle_frame_t{
    void** handles;
    unsigned handles_count;
    unsigned handles_index;

    JHandle_frame_t* prev;
}JHandle_frame_t;

typedef struct JFrame_t{
    JFrame_t* prev;
    JHandle_frame_t* handle_frame; //Can also be scanned by GC, used only by NATIVE code
    JMethod_t* method;
    size_t frame_size;

    uint16_t sp;
    uint32_t pc;
    uint32_t* stack; //I will use conservative GC scanning technique for stack and locals
    uint32_t* locals; //But Object scanning will be implemented precisly
}JFrame_t;

enum{
    EJTS_ACTIVE,
    EJTS_BLOCKED,
    EJTS_TERMINATING,
};

typedef struct JThread_state_t{
    _Atomic uint8_t state;
    _Atomic uint8_t code; //State code. Might be used for exit or EJTS_TERMINATING (future prone, will be removed if not needed)
}JThread_state_t;

#include "os_support.h"
typedef struct JThread_t JThread_t;
typedef struct JThread_t{
    struct list_head list;
    mutex_t thread_lock;

    JFrame_t* top_frame;
    JVM_t* jvm;

    JThread_state_t state;
    bool to_interrupt;
    //jmp_buf* unblocking_interrupt; //This will be used to manually unlock thread from wait in read() for example

    bump_allocator_t frame_arena;

    uint64_t native_thread;
    JObject_t* jobject_thread;
    void (*stop)(JThread_t* thread);
    void (*resume)(JThread_t* thread);
    void (*interrupt)(JThread_t* thread);
}JThread_t;

extern __thread JThread_t* current_JThread;
typedef JError_t (*JMethod_fn_t)(); //This is the main method type of JVM. If returned error is not EJERR_OK, then JVM will exit

JThread_t* thread_init(JVM_t* jvm);
JFrame_t* thread_method_frame_push(JMethod_t* method);
JError_t thread_method_return(void* value, JValue_type_t type);

void thread_frame_stack_push(void* value, JValue_type_t type); //same but automaticly assumes that we want to push in current thread
void thread_frame_stack_pop(void* value, JValue_type_t type);

void thread_frame_stack_push_raw(void* value, uint8_t size); //same but automaticly assumes that we want to push in current thread
void thread_frame_stack_pop_raw(void* value, uint8_t size);

JFrame_t* thread_frame_get();

JError_t frame_method_return(JFrame_t* frame, void* value, JValue_type_t type);

void frame_stack_push(JFrame_t* frame, void* value, JValue_type_t type);
void frame_stack_pop(JFrame_t* frame, void* value, JValue_type_t type);

void frame_stack_push_raw(JFrame_t* frame, void* value, uint8_t size);
void frame_stack_pop_raw(JFrame_t* frame, void* value, uint8_t size);