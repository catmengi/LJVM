#pragma once

#include "bumper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "list.h"
#include "vm.h"
#include "frame.h"

#include "freertos/freeRTOS.h"
#include "freertos/task.h"

#define THREAD_STACK_SIZE 4 * 1024
typedef struct{
    struct list_head list; //This list is used to store thread in JVM
    struct list_head waiting_on; //This list is used to store thread inside monitor while wait
    VM_t* vm;
    enum{
        EJTS_RUNNING = 0,
        EJTS_NATIVE = 1, //Can directly change them with JFrame_t->is_native
        EJTS_STOPPED,
    }state;

    TaskHandle_t self;
    //TODO: JObject_t* jself; //Java object associated with thread

    StaticSemaphore_t lockbuf;
    SemaphoreHandle_t state_lock; //This lock disallows GC to read thread while state is changing, or block thread from doing so
    bump_allocator_t stack_arena;
    JFrame_t* topmost_frame;
}JThread_t;

extern __thread JThread_t* g_JCurrentThread;
JThread_t* JThread_init(VM_t* vm, SemaphoreHandle_t notify_when_done);
JFrame_t* JThread_push_iframe(JMethod_t* method);
JFrame_t* JThread_push_nframe(JMethod_t* method);
JError_t JThread_pop_frame();