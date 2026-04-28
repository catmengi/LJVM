#include "esp_heap_caps.h"
#include "freertos/idf_additions.h"
#include "jeex.h"
#include "vm.h"
#include "jerror.h"
#include "list.h"
#include "lb_endian.h"
#include "portmacro.h"

#include <string.h>

static VMThread_t* alloc_thread(VM_t* vm){
    xSemaphoreTake(vm->thread_lock, portMAX_DELAY);

    VMThread_t* free_thread = NULL;
    list_for_each_entry(free_thread,&vm->free_threads, thread_list){break;}

    if(free_thread){
        list_del_init(&free_thread->thread_list);
        list_add(&free_thread->thread_list, &vm->used_threads);

        free_thread->call_stack.csp = 0;
        free_thread->task_handle = xTaskGetCurrentTaskHandle();
        INIT_LIST_HEAD(&free_thread->wait_list);
    }

    xSemaphoreGive(vm->thread_lock);
    return free_thread;
}

static VMFrame_t* thread_frame_push(VMThread_t* thread, JEEXMethod_t* method){
    VMCallStack_t* cstack = &thread->call_stack;
    if(cstack->csp < VM_MAX_METHOD_CALL_DEPTH){
        uint32_t* stack_base = cstack->csp > 0 ? cstack->frames[cstack->csp - 1].stack : thread->stackbuf;
        VMFrame_t* frame = &cstack->frames[cstack->csp++];

        frame->class = method->owner;
        frame->method = method;
        frame->locals = stack_base;
        frame->stack = (stack_base + method->code.bytecode->locals_count);
        frame->pc = method->code.bytecode->code;

        return frame;
    }
    return NULL;
}

static void thread_frame_pop(VMThread_t* thread){
    thread->call_stack.csp = thread->call_stack.csp > 0 ? thread->call_stack.csp - 1 : 0;
}

static VMFrame_t* thread_frame_get(VMThread_t* thread){
    return thread->call_stack.csp > 0 ? &thread->call_stack.frames[thread->call_stack.csp - 1] : NULL;
}

static VMError_t interpret_bytecode(VMThread_t* thread){
    VMError_t err = EVMERR_OK;
    register VMFrame_t* frame = thread_frame_get(thread);

    void* opcode_labels[] = {};

    return err;
}

VMError_t VM_init(VM_t* vm, JEEXHeader_t* jeex_image){
    VMError_t err = EVMERR_OK;

    INIT_LIST_HEAD(&vm->used_threads);
    INIT_LIST_HEAD(&vm->free_threads);

    vm->thread_lock = xSemaphoreCreateMutex();
    FAIL_SET_JUMP(vm->thread_lock,err,({printf("Couldnt allocate thread_lock mutex!\n");(EVMERR_OOM);}),exit);

    vm->jeex_image = jeex_image;    
    
    vm->gc_data.alloc_bitmap_size = VM_MAX_OBJECTS / (sizeof(uint32_t) * 8);
    vm->gc_data.handles_count = VM_MAX_OBJECTS;
    vm->gc_data.objects_count = 0;
    
    vm->gc_data.sort_table = heap_caps_calloc(vm->gc_data.handles_count,sizeof(*vm->gc_data.sort_table),MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    FAIL_SET_JUMP(vm->gc_data.sort_table, err, ({printf("Unable to allocate GC sort table\n"); (EVMERR_OOM);}),exit);

    vm->gc_data.handles = heap_caps_calloc(vm->gc_data.handles_count,sizeof(*vm->gc_data.handles),MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    FAIL_SET_JUMP(vm->gc_data.handles, err, ({printf("Unable to allocate GC handle table\n"); (EVMERR_OOM);}),exit);

    vm->gc_data.alloc_bitmap = heap_caps_calloc(vm->gc_data.alloc_bitmap_size,sizeof(*vm->gc_data.alloc_bitmap),MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    FAIL_SET_JUMP(vm->gc_data.alloc_bitmap, err, ({printf("Unable to allocate GC handle alloc_bitmap\n"); (EVMERR_OOM);}),exit);

    vm->static_fields = heap_caps_calloc(1, jeex_image->static_fields_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    FAIL_SET_JUMP(vm->static_fields, err, ({printf("Unable to allocate static fields!\n"); (EVMERR_OOM);}),exit);

    for(unsigned i = 0; i < sizeof(vm->threads) / sizeof(vm->threads[0]); i++){
        VMThread_t* thread = &vm->threads[i];
        
        INIT_LIST_HEAD(&thread->thread_list);
        INIT_LIST_HEAD(&thread->wait_list);

        thread->call_stack.csp = 0;
        memset(thread->call_stack.frames, 0, sizeof(thread->call_stack.frames));
        memset(thread->stackbuf, 0, sizeof(thread->stackbuf));

        list_add(&thread->thread_list,&vm->free_threads);
    }

    return err;

exit:
    vSemaphoreDelete(vm->thread_lock);
    free(vm->gc_data.alloc_bitmap);
    free(vm->gc_data.handles);
    free(vm->gc_data.sort_table);
    free(vm->static_fields);
    return err;
}

//Starts a main from a class
VMError_t VM_start(VM_t* vm, char* class_name){
    VMError_t err = EVMERR_OK;
    
    JEEXClass_t* entry_class = JEEXClass_get(vm->jeex_image, class_name);
    FAIL_SET_JUMP(entry_class,err,EVMERR_NOTFOUND,exit);

    JEEXMethod_t* main_method = JEEXMethod_get(entry_class, "main@([Ljava/lang/String;)V");
    FAIL_SET_JUMP(main_method,err,EVMERR_NOTFOUND,exit);

    VMThread_t* main_thread = alloc_thread(vm);
    assert(main_thread);
    assert(thread_frame_push(main_thread, main_method));

exit:
    return err;
}