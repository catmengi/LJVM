#include "thread.h"
#include "class.h"
#include "ljvm.h"
#include <stdint.h>

__thread JThread_t* current_JThread = NULL;

#define THREAD_STACK_SIZE 48 * 1024 //i think 48KB should be enough per thread
JThread_t* thread_init(JVM_t* jvm){
    JThread_t* new_thread = malloc(sizeof(*new_thread));
    if(!new_thread) goto exit;

    memset(new_thread,0,sizeof(*new_thread));
    new_thread->jvm = jvm;

    if(bumper_create(&new_thread->frame_arena,THREAD_STACK_SIZE))
        goto exit;

    TODO("Init other fields");

    INIT_LIST_HEAD(&new_thread->list);
    list_add(&new_thread->list,&jvm->thread_list);

    mutex_init(&new_thread->thread_lock);

    current_JThread = new_thread;
    return new_thread;
exit:
    free(new_thread);
    return NULL;
}

JFrame_t* thread_method_frame_push(JMethod_t* method){
    size_t frame_size = sizeof(JFrame_t) + (method->frame_info.stack_size * sizeof(uint32_t)) + (method->frame_info.locals_count * sizeof(uint32_t));
    JFrame_t* new_frame = bumper_calloc(&current_JThread->frame_arena,1,frame_size);
    if(!new_frame) return NULL;

    mutex_lock(&current_JThread->thread_lock);

    new_frame->frame_size = frame_size;
    new_frame->method = method;
    new_frame->stack = (void*)((uint8_t*)new_frame + sizeof(JFrame_t));
    new_frame->locals = (void*)((uint8_t*)new_frame + sizeof(JFrame_t) + (method->frame_info.stack_size * sizeof(uint32_t)));

    new_frame->prev = current_JThread->top_frame;
    current_JThread->top_frame = new_frame;

    mutex_unlock(&current_JThread->thread_lock);

    return new_frame;
}

static JFrame_t* thread_pop_frame(){
    mutex_lock(&current_JThread->thread_lock);
    JFrame_t* to_pop = current_JThread->top_frame;
    if(to_pop){
        current_JThread->top_frame = to_pop->prev;
        bumper_unwind(&current_JThread->frame_arena,to_pop->frame_size);
    }
    mutex_unlock(&current_JThread->thread_lock);

    return current_JThread->top_frame;
}

JError_t thread_method_return(void* value, JValue_type_t type){
    JError_t err = EJERR_OK;

    mutex_lock(&current_JThread->thread_lock);

    if(type != EJVT_VOID){
        JFrame_t* return_to = current_JThread->top_frame->prev;
        FAIL_SET_JUMP(return_to,err,EJERR_INVALID_FRAME_STATE,exit);

        frame_stack_push(return_to,value,type);
    }
    thread_pop_frame();

exit:
    mutex_unlock(&current_JThread->thread_lock);
    return err;
}

JFrame_t* thread_frame_get(){
    return current_JThread->top_frame;
}

JError_t frame_method_return(JFrame_t* frame, void* value, JValue_type_t type){
    JError_t err = EJERR_OK;

    mutex_lock(&current_JThread->thread_lock);
    JFrame_t* return_to = frame->prev;
    FAIL_SET_JUMP(return_to,err,EJERR_INVALID_FRAME_STATE,exit);

    frame_stack_push(return_to,value,type);
    thread_pop_frame();

exit:
    mutex_unlock(&current_JThread->thread_lock);
    return err;
}

void frame_stack_push_raw(JFrame_t* frame, void* value, uint8_t size){
    memcpy(&frame->stack[frame->sp],value,size);
    frame->sp += size;
}

void frame_stack_pop_raw(JFrame_t* frame, void* value, uint8_t size){
    frame->sp -= size;
    memcpy(value,&frame->stack[frame->sp],size);
}

void frame_stack_push(JFrame_t* frame, void* value, JValue_type_t type){
    frame_stack_push_raw(frame,value,JValue_sizeof(type));
}

void frame_stack_pop(JFrame_t* frame, void* value, JValue_type_t type){
    frame_stack_pop_raw(frame,value,JValue_sizeof(type));
}

void thread_frame_stack_push(void* value, JValue_type_t type){
    frame_stack_push(current_JThread->top_frame,value,type);
}
void thread_frame_stack_pop(void* value, JValue_type_t type){
    frame_stack_pop(current_JThread->top_frame,value,type);
}

void thread_frame_stack_push_raw(void* value, uint8_t size){
    frame_stack_push_raw(current_JThread->top_frame,value,size);
}
void thread_frame_stack_pop_raw(void* value, uint8_t size){
    frame_stack_pop_raw(current_JThread->top_frame,value,size);
}