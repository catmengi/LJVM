#include "arena.h"
#include "builtin_classes.h"
#include "class_linker.h"
#include "class_loader.h"
#include "jvm_internal.h"
#include "jvm.h"
#include "lb_endian.h"
#include "list.h"
#include "object.h"
#include "opcodes.h"

#include <complex.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

__thread jvm_thread_t* jvm_current_thread = NULL;

#define OPCODE_MAX_ARGUMENTS 8
static jvm_opcode_executor_t opcode_executors[211] = {
    [OP_NOP] = {0,NULL,jvm_nop_opcode},

    [OP_LDC] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_ldc_opcode},
    [OP_LDC2w] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ldc_opcode},
    [OP_LDCw] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ldc_opcode},

    [OP_PUTSTATIC] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_putstatic_opcode},
    [OP_GETSTATIC] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_getstatic_opcode},

    [OP_IADD] = {0,NULL,jvm_add_opcodes},
    [OP_LADD] = {0,NULL,jvm_add_opcodes},
    [OP_FADD] = {0,NULL,jvm_add_opcodes},
    [OP_DADD] = {0,NULL,jvm_add_opcodes},

    [OP_RETURN]  = {0,NULL,jvm_return_opcodes},
    [OP_IRETURN] = {0,NULL,jvm_return_opcodes},
    [OP_LRETURN] = {0,NULL,jvm_return_opcodes},
    [OP_FRETURN] = {0,NULL,jvm_return_opcodes},
    [OP_DRETURN] = {0,NULL,jvm_return_opcodes},
    [OP_ARETURN] = {0,NULL,jvm_return_opcodes},

    [OP_JSR] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_return_opcodes},
    [OP_JSRw] = {1,(jvm_opcode_argtype_t[]){EJOT_U32},jvm_return_opcodes},
    [OP_RET] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_ret_opcode},

    [OP_GOTO] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_goto_opcodes},
    [OP_GOTOw] = {1,(jvm_opcode_argtype_t[]){EJOT_U32},jvm_goto_opcodes},

    [OP_POP] = {0,NULL,jvm_pop_opcodes},
    [OP_POP2] = {0,NULL,jvm_pop_opcodes},

    [OP_ICONSTm1] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST0] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST1] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST2] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST3] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST4] = {0,NULL,jvm_iconst_opcodes},
    [OP_ICONST5] = {0,NULL,jvm_iconst_opcodes},

    [OP_ISTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_istore_opcodes},
    [OP_ISTORE0] = {0,NULL,jvm_istore_opcodes},
    [OP_ISTORE1] = {0,NULL,jvm_istore_opcodes},
    [OP_ISTORE2] = {0,NULL,jvm_istore_opcodes},
    [OP_ISTORE3] = {0,NULL,jvm_istore_opcodes},

    [OP_ASTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_astore_opcodes},
    [OP_ASTORE0] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE1] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE2] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE3] = {0,NULL,jvm_astore_opcodes},

    [OP_BASTORE] = {0,NULL,jvm_bastore_opcodes},
    

    [OP_ILOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_iload_opcodes},
    [OP_ILOAD0] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD1] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD2] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD3] = {0,NULL,jvm_iload_opcodes},

    [OP_ALOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_aload_opcodes},
    [OP_ALOAD0] = {0,NULL,jvm_aload_opcodes},
    [OP_ALOAD1] = {0,NULL,jvm_aload_opcodes},
    [OP_ALOAD2] = {0,NULL,jvm_aload_opcodes},
    [OP_ALOAD3] = {0,NULL,jvm_aload_opcodes},

    [OP_SIPUSH] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_sipush_opcode},
    [OP_BIPUSH] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_bipush_opcode},
    [OP_IINC] = {2,(jvm_opcode_argtype_t[]){EJOT_U8,EJOT_U8},jvm_iinc_opcode},

    [OP_IMUL] = {0,NULL,jvm_mul_opcodes},
    [OP_LMUL] = {0,NULL,jvm_mul_opcodes},
    [OP_FMUL] = {0,NULL,jvm_mul_opcodes},
    [OP_DMUL] = {0,NULL,jvm_mul_opcodes},

    [OP_IFICMPeq] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},
    [OP_IFICMPge] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},
    [OP_IFICMPgt] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},
    [OP_IFICMPlt] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},
    [OP_IFICMPle] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},
    [OP_IFICMPne] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ificmp_opcodes},

    [OP_NEW] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_new_opcode},
    [OP_NEWARRAY] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_newarray_opcode},

    [OP_DUP] = {0,NULL,jvm_dup_opcodes},

    [OP_I2B] = {0,NULL,jvm_i2ANY_opcodes},
    [OP_I2C] = {0,NULL,jvm_i2ANY_opcodes},
    [OP_I2D] = {0,NULL,jvm_i2ANY_opcodes},
    [OP_I2F] = {0,NULL,jvm_i2ANY_opcodes},
    [OP_I2L] = {0,NULL,jvm_i2ANY_opcodes},
    [OP_I2S] = {0,NULL,jvm_i2ANY_opcodes},

    [OP_INVOKESTATIC] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokestatic_opcode},
    [OP_INVOKESPECIAL] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokeVS_opcode},
    [OP_INVOKEVIRTUAL] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokeVS_opcode},
};

uint32_t nextby(uint32_t value, uint32_t by){
    if (value % by == 0) {
        return value; // Already divisible by 16
    }
    return (value / by + 1) * by; // Calculate the next multiple of 16
}

#define JVM_EXECUTOR_RESERVED_MEMORY 64 * 1024

void jvm_thread_lock(){ //GC will lock threads mutexes in order to stop them from further execution
    if(jvm_current_thread){
        pthread_mutex_lock(&jvm_current_thread->mutex);
    }
}

void jvm_thread_unlock(){
    if(jvm_current_thread){
        pthread_mutex_unlock(&jvm_current_thread->mutex);
    }
}

jvm_instance_t* jvm_new(classlinker_instance_t* linker, uint32_t heap_size){
    TODO("GC based memory manager");

    Arena* arena = arena_new_dynamic(sizeof(jvm_instance_t*) + heap_size + JVM_EXECUTOR_RESERVED_MEMORY);
    jvm_instance_t* instance = arena_alloc(arena,sizeof(*instance));
    assert(instance);

    instance->arena = arena;
    instance->linker = linker;

    jvm_frame_t param_frame = {.jvm = instance};
    assert(objectmanager_init_heap(&param_frame,heap_size) == JVM_OK);

    classlinker_class_t* class = NULL;
    list_for_each_entry(class,&linker->loaded_classes,list){
        {
            jvm_invokestatic(instance,NULL,classlinker_find_method(class, "<clinit>", NULL),0,NULL);
        }
    }

    return instance;
}

jvm_error_t jvm_bytecode_executor(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    classlinker_bytecode_t* bytecode = frame->method->userctx;

    jvm_thread_lock();
    void** arguments = arena_alloc(frame->jvm->arena,OPCODE_MAX_ARGUMENTS * sizeof(void*));
    FAIL_SET_JUMP(arguments,err,JVM_OOM,exit);

    for(unsigned i = 0; i < OPCODE_MAX_ARGUMENTS; i++){
        arguments[i] = arena_alloc(frame->jvm->arena,sizeof(uint64_t));
        FAIL_SET_JUMP(arguments[i],err,JVM_OOM,exit);
    }

    for(;frame->pc < bytecode->code_length; frame->pc++){
        {
            jvm_opcode_t opcode = bytecode->code[frame->pc];
            jvm_opcode_executor_t executor = opcode_executors[opcode];

            
            for(unsigned arg = 0; arg < executor.nargs; arg++){
                frame->pc++;
                switch(executor.arg_types[arg]){
                    case EJOT_U8:
                        *(uint8_t*)arguments[arg] = bytecode->code[frame->pc];
                        break;
                    case EJOT_U16:
                        *(uint16_t*)arguments[arg] = be16_to_cpu(*(uint16_t*)&bytecode->code[frame->pc]);
                        frame->pc += (sizeof(uint16_t) - 1);
                        break;
                    
                    case EJOT_U32:
                        *(uint32_t*)arguments[arg] = be32_to_cpu(*(uint32_t*)&bytecode->code[frame->pc]);
                        frame->pc += (sizeof(uint32_t) - 1);
                        break;
                    default:
                        TODO("Opcode argument bigger than EJOT_U32 is not implemented now");
                        break;
                }
            }

            if(executor.opcode_fn){
                jvm_error_t executor_ret = executor.opcode_fn(opcode,frame,frame->method->class,executor.nargs,arguments);
                FAIL_SET_JUMP(executor_ret == JVM_OK || executor_ret == JVM_METHOD_RETURN,err,executor_ret,exit);

                if(executor_ret == JVM_METHOD_RETURN){
                    err = JVM_OK;
                    goto exit;
                }

            } else printf("%s : Unknown opcode %d\n",__PRETTY_FUNCTION__,opcode);

        }
    }

exit:
    for(unsigned i = 0; i < OPCODE_MAX_ARGUMENTS; i++){
        arena_free_block(arguments[i]);
    }
    arena_free_block(arguments);
    
    jvm_thread_unlock();
    return err;
}

jvm_error_t jvm_invoke(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]){
    jvm_error_t err = JVM_OK;
    jvm_thread_lock();

    FAIL_SET_JUMP(instance,err,JVM_UNKNOWN,exit);
    FAIL_SET_JUMP(callable_method,err,JVM_UNKNOWN,exit);
    FAIL_SET_JUMP(callable_method->class,err,JVM_NOTFOUND,exit);

    jvm_frame_t frame = {
        .jvm = instance,
        .method = callable_method,
        .previous_frame = previous_frame,
        .locals = alloca(callable_method->frame_descriptor.locals_count * sizeof(*frame.locals)),
        .stack.stack = alloca(callable_method->frame_descriptor.stack_size * sizeof(*frame.stack.stack)),
    };
    if(jvm_current_thread){
        jvm_current_thread->topmost_frame = &frame;
    }

    for(unsigned i = 0; i < nargs; i++){
        frame.locals[i] = args[i];
    }

    if((callable_method->flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED)
        TODO("Synchronized methods support");

    jvm_error_t method_err = callable_method->fn(&frame);
    FAIL_SET_JUMP(method_err == JVM_OK,err,method_err,exit);

    if(jvm_current_thread){
        jvm_current_thread->topmost_frame = previous_frame;
    }

exit:
    jvm_thread_unlock();
    return err;
}

jvm_error_t jvm_invokestatic(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP((callable_method->flags & ACC_STATIC) == ACC_STATIC,err,JVM_OPCODE_INVALID,exit);
    err = jvm_invoke(instance,previous_frame,callable_method,nargs,args);

exit:
    return err;
}

jvm_error_t jvm_launch_class(jvm_instance_t* instance, char* class, int nargs, char** args){
    jvm_error_t err = JVM_OK;

    classlinker_class_t* found_class = classlinker_find_class(instance->linker, class);
    FAIL_SET_JUMP(found_class,err,JVM_NOTFOUND,exit);

    classlinker_method_t* method = classlinker_find_method(found_class,"main","([Ljava/lang/String;)V");
    FAIL_SET_JUMP(method,err,JVM_NOTFOUND,exit);

    jvm_frame_t frame = {
        .jvm = instance,
    };

    objectmanager_object_t* args_object = objectmanager_new_array_object(&(jvm_frame_t){instance}, EJVT_REFERENCE, nargs);
    FAIL_SET_JUMP(args_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* args_array = objectmanager_get_array_object_info(args_object);
    for(unsigned i = 0; i < args_array->count; i++){
        objectmanager_object_t* string_arg = objectmanager_new_class_object(&frame,classlinker_find_class(instance->linker,"java/lang/String"));
        FAIL_SET_JUMP(string_arg,err,JVM_OOM,exit);

        classlinker_method_t* init = objectmanager_class_object_get_method(objectmanager_get_class_object_info(string_arg),"<init>", "(*)V");
        FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

        jvm_value_t init_args[] = {{EJVT_REFERENCE},{EJVT_REFERENCE}};
        *(void**)init_args[0].value = string_arg;
        *(void**)init_args[1].value = args[i];

        jvm_error_t init_err = jvm_invoke(instance,NULL,init,2,init_args);
        FAIL_SET_JUMP(init_err == JVM_OK,err,init_err,exit);

        *(void**)args_array->elements[i].value = string_arg;
    }

    jvm_thread_t* new_thread = arena_alloc(instance->arena,sizeof(*new_thread));
    INIT_LIST_HEAD(&new_thread->list);
    list_add(&instance->threads,&new_thread->list);

    new_thread->topmost_frame = &frame;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&new_thread->mutex,&attr);

    jvm_current_thread = new_thread;

    jvm_value_t invoke_args[] = {{EJVT_REFERENCE}};
    *(void**)invoke_args[0].value = args_object;

    jvm_error_t main_err = jvm_invoke(instance,NULL,method,1,invoke_args);
    FAIL_SET_JUMP(main_err == JVM_OK,err,main_err,exit);

exit:
    pthread_mutex_destroy(&new_thread->mutex);
    jvm_thread_t* tmp_thread = jvm_current_thread;
    jvm_current_thread = NULL;

    if(tmp_thread){
        list_del(&tmp_thread->list);
        arena_free_block(tmp_thread);
    }
    return err;
}

jvm_error_t debug_segfault(jvm_frame_t* frame){
    objectmanager_object_t* string_array = *(void**)frame->locals[0].value;
    objectmanager_array_object_t* array_itself = objectmanager_get_array_object_info(string_array);

    *(int*)1 = 0;

    return JVM_OK;
}

int main(){
    file_reader_t reader = {0};
    file_open_class(&reader,"./test_app.class");

    classloader_instance_t* loader = classloader_new();
    classloader_load_class(loader,&reader);

    classlinker_instance_t* linker = classlinker_new();

    classlinker_method_t debug_segfault_m = {
        .name = "debug_segfault",
        .fn = debug_segfault,
        .raw_description = "([Ljava/lang/String;)V",
        .flags = ACC_STATIC,
        .frame_descriptor.arguments_count = 1,
    };

    classlinker_jni_t jnis[] = {{.class_name = "test_app", .method = &debug_segfault_m}};
    classlinker_jni_list_t jni_list = {.fn_count = 1, .fns = jnis};

    linker->jni_list = &jni_list;

    classlinker_link(linker,loader);

    jvm_instance_t* jvm = jvm_new(linker, 2 * 1024 * 1024);

    jvm_launch_class(jvm,"test_app",1,(char*[]){"Hello world!\n"});
}
