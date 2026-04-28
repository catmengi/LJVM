#include "esp_heap_caps.h"
#include "freertos/idf_additions.h"
#include "jeex.h"
#include "vm.h"
#include "jerror.h"
#include "list.h"
#include "lb_endian.h"
#include "opcodes.h"
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
        int32_t* stack_base = cstack->csp > 0 ? cstack->frames[cstack->csp - 1].stack : thread->stackbuf;
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

static VMFrame_t* thread_frame_pop(VMThread_t* thread){
    return thread->call_stack.csp > 0 ? &thread->call_stack.frames[--thread->call_stack.csp] : NULL;
}

static VMFrame_t* thread_frame_get(VMThread_t* thread){
    return thread->call_stack.csp > 0 ? &thread->call_stack.frames[thread->call_stack.csp - 1] : NULL;
}

static VMError_t interpret_bytecode(VMThread_t* thread){
    VMError_t err = EVMERR_OK;
    register VMFrame_t* frame = thread_frame_get(thread);

    void* opcode_labels[256] = {
        [EJOPCODE_ALOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_ALOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_ALOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_ALOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_ALOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_ILOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_ILOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_ILOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_ILOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_ILOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_FLOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_FLOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_FLOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_FLOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_FLOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_ICONST_0] = &&EJOPCODE_ICONST_0,
        [EJOPCODE_ICONST_1] = &&EJOPCODE_ICONST_1,
        [EJOPCODE_ICONST_2] = &&EJOPCODE_ICONST_2,
        [EJOPCODE_ICONST_3] = &&EJOPCODE_ICONST_3,
        [EJOPCODE_ICONST_4] = &&EJOPCODE_ICONST_4,
        [EJOPCODE_ICONST_5] = &&EJOPCODE_ICONST_5,
        [EJOPCODE_ICONST_M1] = &&EJOPCODE_ICONST_M1,

        [EJOPCODE_PUTSTATIC] = &&EJOPCODE_PUTSTATIC,
        [EJOPCODE_GETSTATIC] = &&EJOPCODE_GETSTATIC,

        [EJOPCODE_BIPUSH] = &&EJOPCODE_BIPUSH,
        [EJOPCODE_SIPUSH] = &&EJOPCODE_SIPUSH,

        [EJOPCODE_RETURN] = &&EJOPCODE_RETURN,
        [EJOPCODE_IRETURN] = &&EJOPCODE_IRETURN,
        [EJOPCODE_FRETURN] = &&EJOPCODE_FRETURN,
        [EJOPCODE_LRETURN] = &&EJOPCODE_LRETURN,
        [EJOPCODE_DRETURN] = &&EJOPCODE_DRETURN,
        [EJOPCODE_ARETURN] = &&EJOPCODE_ARETURN,

        [EJOPCODE_IF_ICMPEQ] = &&EJOPCODE_IF_ICMPEQ,
        [EJOPCODE_IF_ICMPNE] = &&EJOPCODE_IF_ICMPNE,
        [EJOPCODE_IF_ICMPGE] = &&EJOPCODE_IF_ICMPGE,
        [EJOPCODE_IF_ICMPGT] = &&EJOPCODE_IF_ICMPGT,
        [EJOPCODE_IF_ICMPLE] = &&EJOPCODE_IF_ICMPLE,
        [EJOPCODE_IF_ICMPLT] = &&EJOPCODE_IF_ICMPLT,
    };

    #define NEXT() goto *opcode_labels[*(frame->pc += (1 + JOpcode_args_sizes[*frame->pc]))]
    goto *opcode_labels[*frame->pc];

    
    EJOPCODE_LOAD32:
        *(frame->stack++) = frame->locals[*(frame->pc + 1)];
        NEXT();

    
    EJOPCODE_LOAD32_0:
        *(frame->stack++) = frame->locals[0];
        NEXT();
    EJOPCODE_LOAD32_1:
        *(frame->stack++) = frame->locals[1];
        NEXT();
    EJOPCODE_LOAD32_2:
        *(frame->stack++) = frame->locals[2];
        NEXT();
    EJOPCODE_LOAD32_3:
        *(frame->stack++) = frame->locals[3];
        NEXT();



    EJOPCODE_ICONST_0:
        *(frame->stack++) = 0;
        NEXT();
    EJOPCODE_ICONST_1:
        *(frame->stack++) = 1;
        NEXT();
    EJOPCODE_ICONST_2:
        *(frame->stack++) = 2;
        NEXT();
    EJOPCODE_ICONST_3:
        *(frame->stack++) = 3;
        NEXT();
    EJOPCODE_ICONST_4:
        *(frame->stack++) = 4;
        NEXT();
    EJOPCODE_ICONST_5:
        *(frame->stack++) = 5;
        NEXT();

    EJOPCODE_ICONST_M1:
        *(frame->stack++) = -1;
        NEXT();



    EJOPCODE_BIPUSH:
        *(frame->stack++) = *(int8_t*)(frame->pc + 1);
        NEXT();
    
    EJOPCODE_SIPUSH:
        *(frame->stack++) = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        NEXT();
    

    EJOPCODE_RETURN:
        frame = thread_frame_pop(thread);
        if(frame == NULL) //Root method exit
            goto exit;
        printf("return.\n");
        NEXT();

    
    EJOPCODE_IRETURN:
    EJOPCODE_FRETURN:
    EJOPCODE_ARETURN:{
        uint32_t retval = 0;
        int32_t* dest = --frame->stack;
        memcpy(&retval, dest, sizeof(uint32_t));

        frame = thread_frame_pop(thread);
        int32_t* target = frame->stack;

        memcpy(target, &retval, sizeof(uint32_t));
        frame->stack++;

        NEXT();
    }

    EJOPCODE_LRETURN:
    EJOPCODE_DRETURN:{
        uint64_t retval = 0;
        int64_t* dest = (int64_t*)(frame->stack -= 2);
        memcpy(&retval, dest, sizeof(uint64_t));

        frame = thread_frame_pop(thread);
        int64_t* target = (int64_t*)frame->stack;

        memcpy(target, &retval, sizeof(uint64_t));
        frame->stack += 2;

        NEXT();
    }



    EJOPCODE_PUTSTATIC:{
        JEEXSymbol_t* sym = &frame->class->symtab[be16_to_cpu(*((uint16_t*)(frame->pc + 1)))];
        assert(sym->type == EJEEXST_FIELD);
        JEEXField_t* field = sym->value;

        unsigned sz = field->type == EJEEXVT_LONG || field->type == EJEEXVT_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
        void* mem = (thread->vm->static_fields + field->offset);

        frame->stack -= (sz >> 2);
        memcpy(mem, frame->stack, sz);

        NEXT();
    }

    EJOPCODE_GETSTATIC:{
        JEEXSymbol_t* sym = &frame->class->symtab[be16_to_cpu(*((uint16_t*)(frame->pc + 1)))];
        assert(sym->type == EJEEXST_FIELD);
        JEEXField_t* field = sym->value;

        unsigned sz = field->type == EJEEXVT_LONG || field->type == EJEEXVT_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
        void* mem = (thread->vm->static_fields + field->offset);
        
        if(field->initialiser){
            memcpy(mem, field->initialiser,sz);
            field->initialiser = NULL;
        }
        memcpy(frame->stack, mem, sz);
        frame->stack += (sz >> 2);

        NEXT();
    }

    EJOPCODE_IF_ICMPEQ:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 == value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPNE:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 != value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPLT:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 < value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPLE:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 <= value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPGT:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 > value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPGE:{
        int16_t offset = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 >= value2){
            printf("Success\n");
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

exit:
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
        thread->vm = vm;
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

    for(unsigned i = 0; i < vm->jeex_image->id_table_length; i++){
        if(vm->jeex_image->id_table[i].type == EJEEXID_CLASS){
            JEEXClass_t* class = vm->jeex_image->id_table[i].element;
            JEEXMethod_t* clinit = JEEXMethod_get(class, "<clinit>@()V");
            if(clinit){
                assert(thread_frame_push(main_thread, clinit));

                FAIL_SET_JUMP((err = interpret_bytecode(main_thread)) == EVMERR_OK,err,err,exit);

                thread_frame_pop(main_thread);
            }
        }
    }

    assert(thread_frame_push(main_thread, main_method));
    err = interpret_bytecode(main_thread);

exit:
    return err;
}