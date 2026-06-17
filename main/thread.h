#pragma once

#include "bumper.h"
#include "class.h"
#include "config.h"
#include "object.h"

#include <stdint.h>

#define SHADOW_CLEAR_REF(bitmap, idx)  ((bitmap)[(idx) >> 5] &= ~(1U << ((idx) & 31)))
#define SHADOW_SET_REF(bitmap, idx)    ((bitmap)[(idx) >> 5] |= (1U << ((idx) & 31)))
#define SHADOW_GET_REF(bitmap, idx)    (((bitmap)[(idx) >> 5] & (1U << ((idx) & 31))) ? 1 : 0)

typedef struct CallFrame_t CallFrame_t;
typedef struct CallFrame_t{
    size_t frame_size; //Used because of arena

    uint8_t* pc;

    uint32_t sp;
    int32_t* stack;
    uint32_t* shadow_stack;

    int32_t* locals;
    uint32_t* shadow_locals;

    Method_t* method;
    CallFrame_t* prev;
}CallFrame_t;

typedef enum{
    THREAD_ACTIVE,
    THREAD_BLOCKED_SLEEP,
}ThreadState_t;

typedef struct{
    struct list_head list;
    struct list_head joiners; //List of threads that want to join us

    ThreadState_t state;
    int opcode_quota;

    bump_allocator_t frame_allocator; //Initialised on VM startup
    CallFrame_t* top_frame;

    int64_t wakeup_on; //Time when thread should wakeup if sleeping
    char stackbuf[THREAD_STACK_SIZE];   
}Thread_t;

void threads_init();
Error_t thread_schedule();

Thread_t* thread_alloc();
void thread_start(Thread_t* thread, Object_t* this, Method_t* method);
void thread_kill(Thread_t* thread);

Error_t java_method_invoke(Method_t* method, int32_t* arguments, void* return_value);