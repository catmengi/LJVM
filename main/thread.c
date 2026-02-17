#include "thread.h"

#include "bumper.h"
#include "class.h"
#include "frame.h"
#include "freertos/semphr.h"
#include "jerror.h"
#include "loader.h"
#include "portmacro.h"
#include "vm.h"
__thread JThread_t* g_JCurrentThread = NULL;

//Create thread object, init it, add to thread list, and notify someone who waits(if exists)
//In theory i can use notify_when_done for thread creation from java
//USE IT ONLY IF DIDNT INITIALISE(called JThread_init()) THIS FREERTOS TASK BEFORE
JThread_t* JThread_init(JVM_t* vm, SemaphoreHandle_t notify_when_done){
    JThread_t* new_thread = malloc(sizeof(*new_thread));
    if(!new_thread) goto cleanup;    

    INIT_LIST_HEAD(&new_thread->list);
    INIT_LIST_HEAD(&new_thread->waiting_on);

    new_thread->vm = vm;

    new_thread->self = xTaskGetCurrentTaskHandle();
    //TODO: new_thread->jself = ????;
    new_thread->topmost_frame = NULL;

    if(bumper_create(&new_thread->stack_arena,THREAD_STACK_SIZE) != 0) goto cleanup;

    xSemaphoreTakeRecursive(vm->thread_lock,portMAX_DELAY);
    
    list_add(&new_thread->list,&vm->threads);
    new_thread->state = EJTS_RUNNING;

    if(notify_when_done)
        xSemaphoreGive(notify_when_done);
    
    new_thread->state_lock = xSemaphoreCreateMutexStatic(&new_thread->lockbuf);
    xSemaphoreGiveRecursive(vm->thread_lock);

    g_JCurrentThread = new_thread;

    return new_thread;
cleanup:
    if(new_thread){
        bumper_destroy(&new_thread->stack_arena);
    }
    free(new_thread);

    return NULL;
}

//Push interpreter frame
JFrame_t* JThread_push_iframe(JMethod_t* method){
    JThread_t* thread = g_JCurrentThread;
    JFrame_t* frame = NULL;

    JCodeAttribute_t* code = method->method_info;
    unsigned alloc_size = sizeof(JFrame_t) + sizeof(JInterpreterFrame_t) + (code->max_locals * sizeof(uint32_t))
                          + (code->max_stack * sizeof(uint32_t));
    void* allocated = bumper_calloc(&thread->stack_arena,1,alloc_size);
    if(!allocated) goto exit;;

    void* frame_info = allocated + sizeof(JFrame_t*);
    void* stack = frame_info + sizeof(JInterpreterFrame_t);
    void* locals = stack + (code->max_stack * sizeof(uint32_t));

    frame->size = alloc_size;
    frame->is_native = 0;
    frame->actual_frame = frame_info;
    frame->prev = thread->topmost_frame;

    JInterpreterFrame_t* iframe = frame->actual_frame;
    iframe->stack.size = code->max_stack;
    iframe->stack.sp = 0;
    iframe->stack.stack = stack;

    iframe->locals.size = code->max_locals;
    iframe->locals.locals = locals;

    //TODO: stuff for state_lock
    thread->topmost_frame = frame;

exit:
    return frame;
}

//Push native frame
JFrame_t* JThread_push_nframe(JMethod_t* method){
    return NULL;
}

JError_t JThread_pop_frame(){
    return JERR_UNKNOWN;
}