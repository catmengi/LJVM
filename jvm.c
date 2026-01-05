#include "arena.h"
#include "builtin_classes.h"
#include "class_linker.h"
#include "class_loader.h"
#include "jvm_internal.h"
#include "jvm.h"
#include "jvm_method.h"
#include "lb_endian.h"
#include "list.h"
#include "object.h"
#include "opcodes.h"

#include <assert.h>
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
    [OP_GETFIELD] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_getfield_opcode},
    [OP_PUTFIELD] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_putfield_opcode},

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

    [OP_LSTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_lstore_opcodes},
    [OP_LSTORE0] = {0,NULL,jvm_lstore_opcodes},
    [OP_LSTORE1] = {0,NULL,jvm_lstore_opcodes},
    [OP_LSTORE2] = {0,NULL,jvm_lstore_opcodes},
    [OP_LSTORE3] = {0,NULL,jvm_lstore_opcodes},

    [OP_FSTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_fstore_opcodes},
    [OP_FSTORE0] = {0,NULL,jvm_fstore_opcodes},
    [OP_FSTORE1] = {0,NULL,jvm_fstore_opcodes},
    [OP_FSTORE2] = {0,NULL,jvm_fstore_opcodes},
    [OP_FSTORE3] = {0,NULL,jvm_fstore_opcodes},

    [OP_DSTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_dstore_opcodes},
    [OP_DSTORE0] = {0,NULL,jvm_dstore_opcodes},
    [OP_DSTORE1] = {0,NULL,jvm_dstore_opcodes},
    [OP_DSTORE2] = {0,NULL,jvm_dstore_opcodes},
    [OP_DSTORE3] = {0,NULL,jvm_dstore_opcodes},

    [OP_ASTORE] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_astore_opcodes},
    [OP_ASTORE0] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE1] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE2] = {0,NULL,jvm_astore_opcodes},
    [OP_ASTORE3] = {0,NULL,jvm_astore_opcodes},
    

    [OP_ILOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_iload_opcodes},
    [OP_ILOAD0] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD1] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD2] = {0,NULL,jvm_iload_opcodes},
    [OP_ILOAD3] = {0,NULL,jvm_iload_opcodes},

    [OP_FLOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_fload_opcodes},
    [OP_FLOAD0] = {0,NULL,jvm_fload_opcodes},
    [OP_FLOAD1] = {0,NULL,jvm_fload_opcodes},
    [OP_FLOAD2] = {0,NULL,jvm_fload_opcodes},
    [OP_FLOAD3] = {0,NULL,jvm_fload_opcodes},

    [OP_DLOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_dload_opcodes},
    [OP_DLOAD0] = {0,NULL,jvm_dload_opcodes},
    [OP_DLOAD1] = {0,NULL,jvm_dload_opcodes},
    [OP_DLOAD2] = {0,NULL,jvm_dload_opcodes},
    [OP_DLOAD3] = {0,NULL,jvm_dload_opcodes},

    [OP_LLOAD] = {1,(jvm_opcode_argtype_t[]){EJOT_U8},jvm_lload_opcodes},
    [OP_LLOAD0] = {0,NULL,jvm_lload_opcodes},
    [OP_LLOAD1] = {0,NULL,jvm_lload_opcodes},
    [OP_LLOAD2] = {0,NULL,jvm_lload_opcodes},
    [OP_LLOAD3] = {0,NULL,jvm_lload_opcodes},

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
    [OP_ANEWARRAY] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_anewarray_opcode},

    [OP_DUP] = {0,NULL,jvm_dup_opcodes},
    [OP_DUPx1] = {0,NULL,jvm_dup_opcodes},
    [OP_DUPx2] = {0,NULL,jvm_dup_opcodes},
    [OP_DUP2] = {0,NULL,jvm_dup_opcodes},
    [OP_DUP2x1] = {0,NULL,jvm_dup_opcodes},
    [OP_DUP2x2] = {0,NULL,jvm_dup_opcodes},

    [OP_I2B] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_I2C] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_I2D] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_I2F] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_I2L] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_I2S] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_L2I] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_L2F] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_L2D] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_D2I] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_D2F] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_D2L] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_F2I] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_F2D] = {0,NULL,jvm_ANY2ANY_opcodes},
    [OP_F2L] = {0,NULL,jvm_ANY2ANY_opcodes},

    [OP_ATHROW] = {0,NULL,jvm_athrow_opcode},

    [OP_ACONSTNULL] = {0,NULL,jvm_aconstnull},

    [OP_AALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_BALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_CALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_SALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_IALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_LALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_FALOAD] = {0,NULL,jvm_arrayload_opcode},
    [OP_DALOAD] = {0,NULL,jvm_arrayload_opcode},

    [OP_AASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_BASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_CASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_SASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_IASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_LASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_FASTORE] = {0,NULL,jvm_arraystore_opcode},
    [OP_DASTORE] = {0,NULL,jvm_arraystore_opcode},

    [OP_ISUB] = {0,NULL,jvm_sub_opcodes},
    [OP_LSUB] = {0,NULL,jvm_sub_opcodes},
    [OP_FSUB] = {0,NULL,jvm_sub_opcodes},
    [OP_DSUB] = {0,NULL,jvm_sub_opcodes},

    [OP_IDIV] = {0,NULL,jvm_div_opcodes},
    [OP_LDIV] = {0,NULL,jvm_div_opcodes},
    [OP_FDIV] = {0,NULL,jvm_div_opcodes},
    [OP_DDIV] = {0,NULL,jvm_div_opcodes},

    [OP_IAND] = {0,NULL,jvm_and_opcodes},
    [OP_LAND] = {0,NULL,jvm_and_opcodes},

    [OP_IOR] = {0,NULL,jvm_or_opcodes},
    [OP_LOR] = {0,NULL,jvm_or_opcodes},

    [OP_IXOR] = {0,NULL,jvm_xor_opcodes},
    [OP_LXOR] = {0,NULL,jvm_xor_opcodes},

    [OP_DCMPG] = {0,NULL,jvm_dcmp_opcodes},
    [OP_DCMPL] = {0,NULL,jvm_dcmp_opcodes},

    [OP_FCMPG] = {0,NULL,jvm_fcmp_opcodes},
    [OP_FCMPL] = {0,NULL,jvm_fcmp_opcodes},

    [OP_IFeq] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},
    [OP_IFne] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},
    [OP_IFlt] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},
    [OP_IFle] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},
    [OP_IFgt] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},
    [OP_IFge] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifi_opcodes},

    [OP_IFnull] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifnull_opcodes},
    [OP_IFnonnull] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifnull_opcodes},

    [OP_IFACMPq] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifacmp_opcodes},
    [OP_IFACMPe] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_ifacmp_opcodes},

    [OP_INEG] = {0,NULL,jvm_neg_opcodes},
    [OP_LNEG] = {0,NULL,jvm_neg_opcodes},
    [OP_FNEG] = {0,NULL,jvm_neg_opcodes},
    [OP_DNEG] = {0,NULL,jvm_neg_opcodes},

    [OP_INSTANCEOF] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_instanceof_opcode},
    [OP_CHECKCAST] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_checkcast_opcode},

    [OP_IREM] = {0,NULL,jvm_rem_opcodes},
    [OP_LREM] = {0,NULL,jvm_rem_opcodes},
    [OP_FREM] = {0,NULL,jvm_rem_opcodes},
    [OP_DREM] = {0,NULL,jvm_rem_opcodes},

    [OP_ISHL] = {0,NULL,jvm_shl_opcodes},
    [OP_LSHL] = {0,NULL,jvm_shl_opcodes},
    [OP_ISHR] = {0,NULL,jvm_shr_opcodes},
    [OP_LSHR] = {0,NULL,jvm_shr_opcodes},

    [OP_LCMP] = {0,NULL,jvm_lcmp_opcode},

    [OP_LCONST0] = {0,NULL,jvm_lconst_opcodes},
    [OP_LCONST1] = {0,NULL,jvm_lconst_opcodes},

    [OP_MONITORENTER] = {0,NULL,jvm_monitor_opcodes},
    [OP_MONITOREXIT] = {0,NULL,jvm_monitor_opcodes},

    [OP_SWAP] = {0,NULL,jvm_swap_opcode},
    [OP_ARRAYLENGTH] = {0,NULL,jvm_arraylength_opcode},

    [OP_INVOKESTATIC] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokestatic_opcode},
    [OP_INVOKESPECIAL] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokespecial_opcode},
    [OP_INVOKEVIRTUAL] = {1,(jvm_opcode_argtype_t[]){EJOT_U16},jvm_invokevirtual_opcode},
    [OP_INVOKEINTERFACE] = {3,(jvm_opcode_argtype_t[]){EJOT_U16,EJOT_U8,EJOT_U8},jvm_invokevirtual_opcode}, //Temporary solution, investigate this opcode later!

};

#define JVM_EXECUTOR_RESERVED_MEMORY 512 * 1024

jvm_instance_t* jvm_new(classlinker_instance_t* linker, uint32_t heap_size){
    Arena* arena = arena_new_dynamic(sizeof(jvm_instance_t*) + JVM_EXECUTOR_RESERVED_MEMORY);
    jvm_instance_t* instance = arena_alloc(arena,sizeof(*instance));
    assert(instance);

    instance->arena = arena;
    instance->linker = linker;

    assert(objectmanager_init_heap(instance,heap_size) == JVM_OK);

    jvm_thread_t* main_thread = arena_alloc(instance->arena,sizeof(*main_thread));
    assert(main_thread);

    INIT_LIST_HEAD(&instance->threads);
    INIT_LIST_HEAD(&main_thread->list);
    main_thread->JThread = NULL; //main isnt java thread fully;
    main_thread->topmost_frame = NULL; //currently no frame

    jvm_current_thread = main_thread;

    list_add(&main_thread->list,&instance->threads);

    classlinker_class_t* class = NULL;
    list_for_each_entry(class,&linker->loaded_classes,list){
        {
            jvm_invokestatic(instance,NULL,classlinker_find_method(NULL,class, "<clinit>", NULL),0,NULL);
        }
    }

    return instance;
}

void jvm_lock(jvm_instance_t* jvm){

}
void jvm_unlock(jvm_instance_t* jvm){

}

jvm_error_t jvm_bytecode_executor(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    classlinker_bytecode_t* bytecode = frame->method->userctx;

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
    
    return err;
}

jvm_error_t jvm_invoke(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP(instance,err,JVM_UNKNOWN,exit);
    FAIL_SET_JUMP(callable_method,err,JVM_UNKNOWN,exit);
    FAIL_SET_JUMP(callable_method->class,err,JVM_NOTFOUND,exit);

    jvm_frame_t frame = {
        .jvm = instance,
        .method = callable_method,
        .previous_frame = previous_frame,
        .native_exceptions = LIST_HEAD_INIT(frame.native_exceptions),
        .locals = alloca(callable_method->frame_descriptor.locals_count * sizeof(*frame.locals)),
        .stack.stack = alloca(callable_method->frame_descriptor.stack_size * sizeof(*frame.stack.stack)),
    };
    memset(frame.locals,0,callable_method->frame_descriptor.locals_count * sizeof(*frame.locals));
    memset(frame.stack.stack,0,callable_method->frame_descriptor.stack_size * sizeof(*frame.stack.stack));

    jvm_current_thread->topmost_frame = &frame;

    FAIL_SET_JUMP(nargs <= frame.method->frame_descriptor.locals_count || nargs == 0,err,JVM_OPCODE_INVALID,exit);
    FAIL_SET_JUMP(callable_method->fn,err,JVM_NOTFOUND,exit);

    for(unsigned i = 0; i < nargs; i++){
        frame.locals[i] = args[i];
    }

    //Object/class lock section
    if((callable_method->flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED){
        (callable_method->flags & ACC_STATIC) == ACC_STATIC ? 
                    classlinker_class_unlock(callable_method->class) : objectmanager_object_unlock(*(void**)frame.locals[0].value);
    }

    jvm_error_t method_err = callable_method->fn(&frame);

    if((callable_method->flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED){
        (callable_method->flags & ACC_STATIC) == ACC_STATIC ? 
                    classlinker_class_unlock(callable_method->class) : objectmanager_object_unlock(*(void**)frame.locals[0].value);
    }

    FAIL_SET_JUMP(method_err == JVM_OK,err,method_err,exit);
    //===============

    jvm_current_thread->topmost_frame =jvm_current_thread->topmost_frame->previous_frame;

exit:
    return err;
}

jvm_error_t jvm_invokestatic(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP((callable_method->flags & ACC_STATIC) == ACC_STATIC,err,JVM_OPCODE_INVALID,exit);
    err = jvm_invoke(instance,previous_frame,callable_method,nargs,args);

exit:
    return err;
}

jvm_error_t jvm_throw(jvm_frame_t* frame, objectmanager_object_t* exception_object){
    jvm_error_t err = JVM_UNKNOWN;
    jvm_lock(frame->jvm);

    FAIL_SET_JUMP(exception_object,err,JVM_OPPARAM_INVALID,exit);

    classlinker_method_t* exception_handler = NULL;
    objectmanager_class_object_t* exception_cobject = objectmanager_get_class_object_info(exception_object);

    FAIL_SET_JUMP(exception_cobject,err,JVM_OPPARAM_INVALID,exit);
    for(jvm_frame_t* cur = frame; cur; cur = cur->previous_frame){
        classlinker_method_t* cur_method = cur->method;

        if((cur_method->flags & ACC_NATIVE) != ACC_NATIVE){
            classlinker_bytecode_t* bytecode = cur_method->userctx;
            for(unsigned i = 0; i < bytecode->exceptiontable_size; i++){
                classlinker_exceptiontable_t* exception = &bytecode->exception_table[i];

                if(exception->start_pc <= cur->pc && exception->end_pc >= cur->pc){
                    cur->pc = exception->handler_pc - 1;

                    uint16_t sp = cur->stack.sp++;
                    cur->stack.stack[sp].type = EJVT_REFERENCE;
                    *(void**)cur->stack.stack[sp].value = exception_object;

                    err = JVM_OK;
                    goto exit;
                }
            }
        } else if(cur != frame){ //Without this exception will not be able to make it through this frame
            jvm_native_exception_t* native_exception = arena_alloc(frame->jvm->arena,sizeof(*native_exception));
            FAIL_SET_JUMP(native_exception,err,JVM_OOM,exit);

            INIT_LIST_HEAD(&native_exception->list);
            native_exception->exception_object = exception_object;

            list_add(&native_exception->list,&cur->native_exceptions);
            goto exit;
        }
        err = JVM_METHOD_RETURN; //Set err to JVM_METHOD_RETURN if there is no handler for this expection in frame
    }

exit:
    if(err == JVM_UNKNOWN){
        printf("unhandled exception! %s\n",exception_cobject->class->this_name);
    }
    jvm_unlock(frame->jvm);
    return err;
}

objectmanager_object_t* jvm_native_catch_exception(jvm_frame_t* frame){ //Native only
    jvm_native_exception_t* exception = NULL;
    jvm_native_exception_t* tmp = NULL;
    list_for_each_entry_safe(exception,tmp,&frame->native_exceptions,list){
        objectmanager_object_t* ret = exception->exception_object;
        list_del(&exception->list);
        arena_free_block(exception);

        return ret;
    }

    return NULL;
}

void jvm_native_return(jvm_frame_t* frame, jvm_value_t value){
    assert(frame->previous_frame);
    frame->previous_frame->stack.stack[frame->previous_frame->stack.sp++] = value;
}

jvm_value_t jvm_native_get_return(jvm_frame_t* frame){
    return frame->stack.stack[--frame->stack.sp];
}

jvm_error_t jvm_launch_class(jvm_instance_t* instance, char* class, int nargs, char** args){
    jvm_error_t err = JVM_OK;

    classlinker_class_t* found_class = classlinker_find_class(instance->linker, class);
    FAIL_SET_JUMP(found_class,err,JVM_NOTFOUND,exit);

    jvm_frame_t frame = {
        .jvm = instance,
    };

    classlinker_method_t* method = classlinker_find_method(&frame,found_class,"main","([Ljava/lang/String;)V");
    FAIL_SET_JUMP(method,err,JVM_NOTFOUND,exit);

    objectmanager_object_t* args_object = objectmanager_new_array_object(&(jvm_frame_t){instance}, EJVT_REFERENCE, nargs);
    FAIL_SET_JUMP(args_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* args_array = objectmanager_get_array_object_info(args_object);
    for(unsigned i = 0; i < args_array->count; i++){
        objectmanager_object_t* string_arg = objectmanager_new_class_object(&frame,classlinker_find_class(instance->linker,"java/lang/String"));
        FAIL_SET_JUMP(string_arg,err,JVM_OOM,exit);

        classlinker_method_t* init = objectmanager_class_object_get_method(&frame,objectmanager_get_class_object_info(string_arg),"<init>", "(*)V");
        FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

        jvm_value_t init_args[] = {{EJVT_REFERENCE},{EJVT_REFERENCE}};
        *(void**)init_args[0].value = string_arg;
        *(void**)init_args[1].value = args[i];

        jvm_error_t init_err = jvm_invoke(instance,NULL,init,2,init_args);
        FAIL_SET_JUMP(init_err == JVM_OK,err,init_err,exit);

        *(void**)args_array->elements[i].value = string_arg;
    }

    jvm_value_t invoke_args[] = {{EJVT_REFERENCE}};
    *(void**)invoke_args[0].value = args_object;

    jvm_error_t main_err = jvm_invoke(instance,NULL,method,1,invoke_args);
    FAIL_SET_JUMP(main_err == JVM_OK,err,main_err,exit);

exit:
    return err;
}
