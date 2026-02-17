#pragma once

#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"

#include "bumper.h"
#include "linker.h"
#include "list.h"
#include <pthread.h>

#define VM_INSTANCE_MEMORY 5 * 1024 * 1024
typedef struct{
    bool request_safepoint:1;
    SemaphoreHandle_t gc_stop_notify; //DO NOT TOUCH WHILE request_safepoint == 0 (will be allocated on stack of GC!)
}JVMControls_t;

typedef struct{
    unsigned threads_count; //All threads count (any type of methods)
    unsigned ithreads_count; //Interpreted threads count (currently in interpreter method)
    unsigned nthreads_count;  //Native threads count (currently in native method)
}JVMStats_t;

typedef struct{
    struct list_head threads; //List of threads....
    SemaphoreHandle_t thread_lock; //Used to synchronize thread modifications
    JVMControls_t controls;
    //TODO: global device description? HT to allow java classes load device's screen, sound, etc handles? 

    pthread_rwlock_t handle_rwlock; //Rw lock for handles in native code(to sync with gc properly)
    bump_allocator_t arena; //Global data arena: loader,linker.heap. Only threads have their own malloced one(for stacks)
    JLoader_t loader;
    JLinker_t linker;
}VM_t;

#include "thread.h"
int VM_init(VM_t* vm);
JError_t VM_startup(VM_t* vm, char* class);