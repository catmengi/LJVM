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
    //JHandle_frame_t* handle_frame; //Can also be scanned by GC, used only by NATIVE code
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
    JError_t local_errno;
    
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
JFrame_t* thread_method_frame_pop();

JFrame_t* thread_frame_get();

//u32 is for uint32_t based values (everything except double and long), u64 is double and long basically
void thread_frame_stack_push_u32(void* value);
void thread_frame_stack_pop_u32(void* output);
void thread_frame_stack_push_u64(void* value);
void thread_frame_stack_pop_u64(void* output);

void thread_frame_local_set_u32(unsigned index, void* value);
void thread_frame_local_set_u64(unsigned index, void* value);
void thread_frame_local_get_u32(unsigned index, void* output);
void thread_frame_local_get_u64(unsigned index, void* output);

//USE ONLY THOOSE FOR POINTERS! THEY WILL AUTOMATICLY PERFORM POINTER COMPRESSION/DECOMPRESSION BASED ON ARCH
void thread_frame_stack_push_reference(void* value);
void thread_frame_stack_pop_reference(void* output);
void thread_frame_local_get_reference(unsigned index, void* output);
void thread_frame_local_set_reference(unsigned index, void* value);
//============================================================================================================

JError_t frame_method_return(JFrame_t* frame, void* value);
JError_t thread_method_return(void* value); //Return type will be automaticly fetched from method info
