#include "thread.h"
#include "object.h"
#include "class.h"
#include "ljvm.h"
#include <stdint.h>

__thread JThread_t* current_JThread = NULL;

#define THREAD_STACK_SIZE 8 * 1024 //i think this should be enough per thread
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

void frame_stack_push_u32(JFrame_t* frame, void* value){
    frame->stack[frame->sp++] = *(uint32_t*)value;
}

void frame_stack_pop_u32(JFrame_t* frame, void* output){
    *(uint32_t*)output = frame->stack[--frame->sp];
}

void frame_stack_push_u64(JFrame_t* frame, void* value){
    *(uint64_t*)&frame->stack[frame->sp] = *(uint64_t*)value;
    frame->sp += 2;
}

void frame_stack_pop_u64(JFrame_t* frame, void* output){
    frame->sp -= 2;
    *(uint64_t*)output = *(uint64_t*)&frame->stack[frame->sp];
}

void frame_stack_push_reference(JFrame_t* frame, void* value){
    uint32_t value_to_push;
#ifdef JHEAP_DWORD_PTR
    value_to_push = ptr_compress(current_JThread->jvm->object_heap->heap_start, value);
#else
    value_to_push = (uint32_t)(uintptr_t)value;
#endif
    frame->stack[frame->sp++] = value_to_push;
}

void frame_stack_pop_reference(JFrame_t* frame, void* output){
    uint32_t reference = frame->stack[--frame->sp];
#ifdef JHEAP_DWORD_PTR
    *(void**)output = ptr_decompress(current_JThread->jvm->object_heap->heap_start, reference);
#else
    *(void**)output = (void*)(uintptr_t)reference;
#endif
}

void thread_frame_stack_push_u32(void* value){
    frame_stack_push_u32(current_JThread->top_frame, value);
}

void thread_frame_stack_pop_u32(void* output){
    frame_stack_pop_u32(current_JThread->top_frame, output);
}

void thread_frame_stack_push_u64(void* value){
    frame_stack_push_u64(current_JThread->top_frame, value);
}

void thread_frame_stack_pop_u64(void* output){
    frame_stack_pop_u64(current_JThread->top_frame, output);
}

void thread_frame_stack_push_reference(void* value){
    frame_stack_push_reference(current_JThread->top_frame, value);
}

void thread_frame_stack_pop_reference(void* output){
    frame_stack_pop_reference(current_JThread->top_frame, output);
}

void frame_local_get_u32(JFrame_t* frame, unsigned index, void* output){
    *(uint32_t*)output = frame->locals[index];
}

void frame_local_get_u64(JFrame_t* frame, unsigned index, void* output){
    *(uint64_t*)output = *(uint64_t*)&frame->locals[index];
}

void frame_local_set_u32(JFrame_t* frame, unsigned index, void* value){
    frame->locals[index] = *(uint32_t*)value;
}

void frame_local_set_u64(JFrame_t* frame, unsigned index, void* value){
    *(uint64_t*)&frame->locals[index] = *(uint64_t*)value;
}

void thread_frame_local_set_u32(unsigned index, void* value){
    frame_local_set_u32(current_JThread->top_frame, index, value);
}

void thread_frame_local_set_u64(unsigned index, void* value){
    frame_local_set_u64(current_JThread->top_frame, index, value);
}

void thread_frame_local_get_u32(unsigned index, void* output){
    frame_local_get_u32(current_JThread->top_frame, index, output);
}

void thread_frame_local_get_u64(unsigned index, void* output){
    frame_local_get_u64(current_JThread->top_frame, index, output);
}

void frame_local_set_reference(JFrame_t* frame, unsigned index, void* value){
    uint32_t value_to_set;
#ifdef JHEAP_DWORD_PTR
    value_to_set = ptr_compress(current_JThread->jvm->object_heap->heap_start, value);
#else
    value_to_set = (uint32_t)(uintptr_t)value;
#endif
    frame_local_set_u32(frame, index, &value_to_set);
}

void frame_local_get_reference(JFrame_t* frame, unsigned index, void* output){
    uint32_t reference = 0;
    frame_local_get_u32(frame, index, &reference);
#ifdef JHEAP_DWORD_PTR
    *(void**)output = ptr_decompress(current_JThread->jvm->object_heap->heap_start, reference);
#else
    *(void**)output = (void*)(uintptr_t)reference;
#endif
}


void thread_frame_local_set_reference(unsigned index, void* value){
    frame_local_set_reference(current_JThread->top_frame, index, value);
}

void thread_frame_local_get_reference(unsigned index, void* output){
    frame_local_get_reference(current_JThread->top_frame,index,output);
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

JFrame_t* thread_method_frame_pop(){
    mutex_lock(&current_JThread->thread_lock);
    JFrame_t* to_pop = current_JThread->top_frame;
    if(to_pop){
        current_JThread->top_frame = to_pop->prev;
        bumper_unwind(&current_JThread->frame_arena,to_pop->frame_size);
    }
    mutex_unlock(&current_JThread->thread_lock);

    return current_JThread->top_frame;
}

JFrame_t* thread_frame_get(){
    return current_JThread->top_frame;
}

JError_t frame_method_return(JFrame_t* frame, void* value){
    JError_t err = EJERR_OK;

    mutex_lock(&current_JThread->thread_lock);
    JFrame_t* return_to = frame->prev;
    FAIL_SET_JUMP(return_to || frame->method->return_type == EJVT_VOID,err,EJERR_INVALID_FRAME_STATE,exit);

    JValue_type_t type = frame->method->return_type;

    switch(type){
        case EJVT_VOID:
            break; //Nothing to return

        case EJVT_DOUBLE:
        case EJVT_LONG:
            frame_stack_push_u64(return_to,value);
            break;

        case EJVT_REFERENCE:
        case EJVT_NATIVE:
            frame_stack_push_reference(return_to, value);
            break;

        default:
            frame_stack_push_u32(return_to,value);
    }

    thread_method_frame_pop();

exit:
    mutex_unlock(&current_JThread->thread_lock);
    return err;
}

JError_t thread_method_return(void* value){
    return frame_method_return(current_JThread->top_frame, value);
}