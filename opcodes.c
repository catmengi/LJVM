#include "opcodes.h"
#include "arena.h"
#include "class_linker.h"
#include "class_loader.h"

#include "jvm.h"
#include "jvm_internal.h"
#include "jvm_method.h"
#include "object.h"

#include <math.h>
#include <stdint.h>

jvm_error_t jvm_nop_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    return JVM_OK;
}

jvm_error_t jvm_ldc_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    uint16_t index = 0;

    switch(opcode){
        default: 
            index = *(uint16_t*)args[0];
            break;
        case OP_LDC:
            index = *(uint8_t*)args[0];
            break;
    }

    FAIL_SET_JUMP(index > 0, err, JVM_OPCODE_INVALID,exit);
    index--;

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_constant_t* constant = &class_info->constant_pool.constants[index];

    jvm_value_t new_value = {0};

    switch(constant->constant_type){
        case EJCT_double:
            new_value.type = EJVT_DOUBLE;
            *(double*)new_value.value = *(double*)constant->constant_value;
            break;
        case EJCT_float:
            new_value.type = EJVT_FLOAT;
            *(float*)new_value.value = *(float*)constant->constant_value;
            break;
        case EJCT_int:
            new_value.type = EJVT_INT;
            *(uint32_t*)new_value.value = *(uint32_t*)constant->constant_value;
            break;     
        case EJCT_long:
            new_value.type = EJVT_LONG;
            *(uint64_t*)new_value.value = *(uint64_t*)constant->constant_value;
            break;       
        case EJCT_string:
            new_value.type = EJVT_REFERENCE;
            *(void**)new_value.value = constant->constant_value;
            break;
        case EJCT_unitialised_string:{ //String fuckery because i cant initialise them in linker properly............ Even more string fuckery because of GC....
                jvm_lock(frame->jvm); //Cannot be done fully lockless
                    char* native_string = constant->constant_value;
                    constant->constant_type = EJCT_string;
                    constant->constant_value = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker,"java/lang/String"));
                jvm_unlock(frame->jvm);

                objectmanager_object_t* string = constant->constant_value;
                FAIL_SET_JUMP(string,err,JVM_OOM,exit);

                classlinker_method_t* init_method = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(string),"<init>","(*)V");
                FAIL_SET_JUMP(init_method,err,JVM_NOTFOUND,exit);

                jvm_value_t args[2] = {{EJVT_REFERENCE},{EJVT_NATIVEPTR}};
                *(void**)args[0].value = string;
                *(void**)args[1].value = native_string;

                jvm_error_t init_error = jvm_invoke(frame->jvm,frame,init_method,2,args);
                FAIL_SET_JUMP(init_error == JVM_OK,err,init_error,exit);

                constant->constant_type = EJCT_string;
                constant->constant_value = string;

                new_value.type = EJVT_REFERENCE;
                *(void**)new_value.value = string;
            }
            break;

        default:
            printf("%s: unknown constant type %d\n",__PRETTY_FUNCTION__,constant->constant_type);
            break;
    }
    frame->stack.stack[frame->stack.sp++] = new_value;

exit:
    return err;
}

jvm_error_t jvm_putstatic_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t* input = &frame->stack.stack[--frame->stack.sp];

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* field_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;


    classlinker_field_t* field = classlinker_find_staticfield(frame,field_ref->class, field_ref->nameandtype.name);
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);
    FAIL_SET_JUMP((field->flags & ACC_STATIC) == ACC_STATIC,err,JVM_OPCODE_INVALID,exit);

    field->value = *input;

exit:
    return err;
}

jvm_error_t jvm_getstatic_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;


    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* field_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;


    classlinker_field_t* field = classlinker_find_staticfield(frame,field_ref->class, field_ref->nameandtype.name);
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);
    FAIL_SET_JUMP((field->flags & ACC_STATIC) == ACC_STATIC,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = field->value;
exit:
    return err;
}

jvm_error_t jvm_add_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t new_value = {0};
    switch(opcode){
        case OP_IADD:{
                new_value.type = EJVT_INT;
                *(uint32_t*)new_value.value = (*(uint32_t*)value1.value) + (*(uint32_t*)value2.value);
            }
            break;

        case OP_LADD:{
                new_value.type = EJVT_LONG;
                *(uint64_t*)new_value.value = (*(uint64_t*)value1.value) + (*(uint64_t*)value2.value);
            }
            break;
        
        case OP_FADD:{
                new_value.type = EJVT_FLOAT;
                *(float*)new_value.value = (*(float*)value1.value) + (*(float*)value2.value);
            }
            break;

        case OP_DADD:{
                new_value.type = EJVT_DOUBLE;
                *(double*)new_value.value = (*(double*)value1.value) + (*(double*)value2.value);
            }
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = new_value;

    return err;
}

jvm_error_t jvm_return_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    switch(opcode){
        case OP_RETURN:
            err = JVM_METHOD_RETURN;
            goto exit;

        default:
            err = JVM_METHOD_RETURN;
            FAIL_SET_JUMP(frame->previous_frame,err,JVM_OPCODE_INVALID,exit);

            frame->previous_frame->stack.stack[frame->previous_frame->stack.sp++] = frame->stack.stack[--frame->stack.sp];
            break;
    }

exit:
    return err;
}

jvm_error_t jvm_jsr_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = 0;
    switch(opcode){
        default:
            FAIL_SET_JUMP(0,err,JVM_OPCODE_INVALID,exit);
        case OP_JSR:
            offset = *(int16_t*)args[0];
            offset -= sizeof(int16_t) - 1;
            break;
        case OP_JSRw:
            offset = *(int32_t*)args[0];
            offset -= sizeof(int32_t) - 1;
            break;
    }

    jvm_value_t addr = {EJVT_CODEADDR};
    *(int32_t*)addr.value = frame->pc + (offset - 2);

    frame->stack.stack[frame->stack.sp++] = addr;

exit:
    return err;
}

jvm_error_t jvm_ret_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP(*(uint8_t*)args[0] < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);
    jvm_value_t addr = frame->locals[*(uint8_t*)args[0]];

    FAIL_SET_JUMP(addr.type == EJVT_CODEADDR,err,JVM_OPCODE_INVALID,exit);
    frame->pc = *(uint32_t*)addr.value - 1;

exit:
    return err;
}

jvm_error_t jvm_goto_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = 0;
    switch(opcode){
        case OP_GOTO:
            offset = *(int16_t*)args[0];
            offset -= sizeof(uint16_t) - 1; //fucking fuckery to fuck frame->pc in the right way
            break;
        case OP_GOTOw:
            offset = *(int32_t*)args[0];
            offset -= sizeof(uint32_t) - 1;
            break;

        default: break;
    }

    frame->pc += (offset - 2);

    return err;
}

jvm_error_t jvm_ificmp_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = -3 + *(uint16_t*)args[0]; //Magic value to skip frame->pc properly. Same as goto

    int32_t value2 = *(int32_t*)frame->stack.stack[--frame->stack.sp].value;
    int32_t value1 = *(int32_t*)frame->stack.stack[--frame->stack.sp].value;

    bool do_branch = false;
    switch(opcode){
        case OP_IFICMPeq:
            if(value1 == value2)
                do_branch = true;
            break;
        
        case OP_IFICMPne:
            if(value1 != value2)
                do_branch = true;
            break;
        
        case OP_IFICMPlt:
            if(value1 < value2)
                do_branch = true;
            break;
        
        case OP_IFICMPle:
            if(value1 <= value2)
                do_branch = true;
            break;
        
        case OP_IFICMPgt:
            if(value1 > value2)
                do_branch = true;
            break;
        
        case OP_IFICMPge:
            if(value1 >= value2)
                do_branch = true;
            break;

        default: break;
    }

    if(do_branch)
        frame->pc += offset;
    

    return err;
}

jvm_error_t jvm_ifnull_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = -3 + *(uint16_t*)args[0]; //Magic value to skip frame->pc properly. Same as goto

    void* value = *(void**)frame->stack.stack[--frame->stack.sp].value;

    bool do_branch = false;
    switch(opcode){
        case OP_IFnull:
            if(value == NULL)
                do_branch = true;
            break;
        
        case OP_IFnonnull:
            if(value)
                do_branch = true;
            break;

        default: break;
    }

    if(do_branch)
        frame->pc += offset;
    

    return err;
}

jvm_error_t jvm_ifacmp_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = -3 + *(uint16_t*)args[0]; //Magic value to skip frame->pc properly. Same as goto

    void* value2 = *(void**)frame->stack.stack[--frame->stack.sp].value;
    void* value1 = *(void**)frame->stack.stack[--frame->stack.sp].value;

    bool do_branch = false;
    switch(opcode){
        case OP_IFACMPq:
            if(value1 == value2)
                do_branch = true;
            break;
        
        case OP_IFACMPe:
            if(value1 != value2)
                do_branch = true;
            break;

        default: break;
    }

    if(do_branch)
        frame->pc += offset;
    

    return err;
}

jvm_error_t jvm_ifi_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    int32_t offset = -3 + *(uint16_t*)args[0]; //Magic value to skip frame->pc properly. Same as goto

    int32_t value = *(int32_t*)frame->stack.stack[--frame->stack.sp].value;

    bool do_branch = false;
    switch(opcode){
        case OP_IFeq:
            if(value == 0)
                do_branch = true;
        break;

        case OP_IFne:
            if(value != 0)
                do_branch = true;
        break;

        case OP_IFlt:
            if(value < 0)
                do_branch = true;
        break;

        case OP_IFle:
            if(value <= 0)
                do_branch = true;
        break;

        case OP_IFgt:
            if(value > 0)
                do_branch = true;
        break;

        case OP_IFge:
            if(value >= 0)
                do_branch = true;
        break;

        default: break;
    }

    if(do_branch)
        frame->pc += offset;
    

    return err;
}

jvm_error_t jvm_invokestatic_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* method_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    classlinker_method_t* method = classlinker_find_method(frame,method_ref->class,method_ref->nameandtype.name,method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(method,err,JVM_NOTFOUND,exit);

    jvm_value_t* method_args = alloca(method->frame_descriptor.arguments_count * sizeof(*method_args));
    for(unsigned i = method->frame_descriptor.arguments_count; i-- > 0;){
        method_args[i] = frame->stack.stack[--frame->stack.sp];
    }

    jvm_error_t invoke_err = jvm_invokestatic(frame->jvm,frame,method,method->frame_descriptor.arguments_count,method_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

exit:
    return err;
}

jvm_error_t jvm_pop_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    switch(opcode){
        case OP_POP:
            frame->stack.stack[--frame->stack.sp] = (jvm_value_t){0};
            break;
        case OP_POP2:
            frame->stack.stack[--frame->stack.sp] = (jvm_value_t){0};
            frame->stack.stack[--frame->stack.sp] = (jvm_value_t){0};
            break;

        default: break;
    }

    return err;
}

jvm_error_t jvm_iconst_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = {.type = EJVT_INT};
    switch(opcode){
        case OP_ICONSTm1:
            *(int32_t*)value.value = -1;
            break;
        case OP_ICONST0:
            *(int32_t*)value.value = 0;
            break;
        case OP_ICONST1:
            *(int32_t*)value.value = 1;
            break;
        case OP_ICONST2:
            *(int32_t*)value.value = 2;
            break;
        case OP_ICONST3:
            *(int32_t*)value.value = 3;
            break;
        case OP_ICONST4:
            *(int32_t*)value.value = 4;
            break;
        case OP_ICONST5:
            *(int32_t*)value.value = 5;
            break;

        default: break;
    }

    frame->stack.stack[frame->stack.sp++] = value;

    return err;
}

jvm_error_t jvm_istore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_ISTORE:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_ISTORE1:
            local_index = 1;
            break;
        case OP_ISTORE2:
            local_index = 2;
            break;
        case OP_ISTORE3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->locals[local_index] = frame->stack.stack[--frame->stack.sp];
exit:
    return err;
}

jvm_error_t jvm_lstore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_LSTORE:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_LSTORE1:
            local_index = 1;
            break;
        case OP_LSTORE2:
            local_index = 2;
            break;
        case OP_LSTORE3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->locals[local_index] = frame->stack.stack[--frame->stack.sp];
exit:
    return err;
}

jvm_error_t jvm_fstore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_FSTORE:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_FSTORE1:
            local_index = 1;
            break;
        case OP_FSTORE2:
            local_index = 2;
            break;
        case OP_FSTORE3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->locals[local_index] = frame->stack.stack[--frame->stack.sp];
exit:
    return err;
}

jvm_error_t jvm_dstore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_DSTORE:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_DSTORE1:
            local_index = 1;
            break;
        case OP_DSTORE2:
            local_index = 2;
            break;
        case OP_DSTORE3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->locals[local_index] = frame->stack.stack[--frame->stack.sp];
exit:
    return err;
}

jvm_error_t jvm_astore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_ASTORE:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_ASTORE1:
            local_index = 1;
            break;
        case OP_ASTORE2:
            local_index = 2;
            break;
        case OP_ASTORE3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->locals[local_index] = frame->stack.stack[--frame->stack.sp];
exit:
    return err;
}

jvm_error_t jvm_ANY2ANY_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    switch(opcode){
        case OP_I2B:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_INT;
            break;
        case OP_I2S:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_SHORT;
            break;
        case OP_I2C:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_CHAR;
            break;
        case OP_I2L:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_LONG;
            break;
        case OP_I2F:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_FLOAT;
            *(float*)frame->stack.stack[frame->stack.sp - 1].value = *(int32_t*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_I2D:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_DOUBLE;
            *(double*)frame->stack.stack[frame->stack.sp - 1].value = *(int32_t*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_L2D:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_DOUBLE;
            *(double*)frame->stack.stack[frame->stack.sp - 1].value = *(int64_t*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_L2F:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_FLOAT;
            *(float*)frame->stack.stack[frame->stack.sp - 1].value = *(int64_t*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_L2I:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_INT;
            break;
        case OP_F2D:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_DOUBLE;
            *(double*)frame->stack.stack[frame->stack.sp - 1].value = *(float*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_F2L:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_LONG;
            *(int64_t*)frame->stack.stack[frame->stack.sp - 1].value = *(float*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_F2I:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_INT;
            *(int32_t*)frame->stack.stack[frame->stack.sp - 1].value = *(float*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_D2F:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_FLOAT;
            *(float*)frame->stack.stack[frame->stack.sp - 1].value = *(double*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_D2I:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_INT;
            *(int32_t*)frame->stack.stack[frame->stack.sp - 1].value = *(double*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;
        case OP_D2L:
            frame->stack.stack[frame->stack.sp - 1].type = EJVT_LONG;
            *(int64_t*)frame->stack.stack[frame->stack.sp - 1].value = *(double*)frame->stack.stack[frame->stack.sp - 1].value; 
            break;

        default: break;
    }

    return JVM_OK;
}

jvm_error_t jvm_iload_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_ILOAD:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_ILOAD1:
            local_index = 1;
            break;
        case OP_ILOAD2:
            local_index = 2;
            break;
        case OP_ILOAD3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = frame->locals[local_index];

exit:
    return err;
}

jvm_error_t jvm_lload_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_LLOAD:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_LLOAD1:
            local_index = 1;
            break;
        case OP_LLOAD2:
            local_index = 2;
            break;
        case OP_LLOAD3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = frame->locals[local_index];

exit:
    return err;
}

jvm_error_t jvm_fload_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_FLOAD:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_FLOAD1:
            local_index = 1;
            break;
        case OP_FLOAD2:
            local_index = 2;
            break;
        case OP_FLOAD3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = frame->locals[local_index];

exit:
    return err;
}

jvm_error_t jvm_dload_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_DLOAD:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_DLOAD1:
            local_index = 1;
            break;
        case OP_DLOAD2:
            local_index = 2;
            break;
        case OP_DLOAD3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = frame->locals[local_index];

exit:
    return err;
}

jvm_error_t jvm_aload_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    unsigned local_index = 0;

    switch(opcode){
        case OP_ALOAD:
            local_index = *(uint8_t*)args[0];
            break;
        case OP_ALOAD1:
            local_index = 1;
            break;
        case OP_ALOAD2:
            local_index = 2;
            break;
        case OP_ALOAD3:
            local_index = 3;
            break;

        default: break;
    }
    FAIL_SET_JUMP(local_index < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);

    frame->stack.stack[frame->stack.sp++] = frame->locals[local_index];

exit:
    return err;
}

jvm_error_t jvm_sipush_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = {EJVT_INT};
    *(uint32_t*)value.value = *(uint16_t*)args[0];

    frame->stack.stack[frame->stack.sp++] = value;

    return err;
}

jvm_error_t jvm_bipush_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = {EJVT_INT};
    *(uint32_t*)value.value = *(uint8_t*)args[0];

    frame->stack.stack[frame->stack.sp++] = value;

    return err;
}

jvm_error_t jvm_iinc_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP(*(uint8_t*)args[0] < frame->method->frame_descriptor.locals_count, err, JVM_OPCODE_INVALID,exit);
    FAIL_SET_JUMP(frame->locals[*(uint8_t*)args[0]].type == EJVT_INT,err,JVM_OPCODE_INVALID,exit);

    *(int32_t*)frame->locals[*(uint8_t*)args[0]].value += *(int8_t*)args[1];
exit:
    return err;
}

jvm_error_t jvm_mul_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_IMUL:
            *(int32_t*)output.value = *(int32_t*)value1.value * *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LMUL:
            *(int64_t*)output.value = *(int64_t*)value1.value * *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;
        case OP_FMUL:
            *(float*)output.value = *(float*)value1.value * *(float*)value2.value;
            output.type = EJVT_FLOAT;
            break;
        case OP_DMUL:
            *(double*)output.value = *(double*)value1.value * *(double*)value2.value;
            output.type = EJVT_DOUBLE;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}

jvm_error_t jvm_sub_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_ISUB:
            *(int32_t*)output.value = *(int32_t*)value1.value - *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LSUB:
            *(int64_t*)output.value = *(int64_t*)value1.value - *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;
        case OP_FSUB:
            *(float*)output.value = *(float*)value1.value - *(float*)value2.value;
            output.type = EJVT_FLOAT;
            break;
        case OP_DSUB:
            *(double*)output.value = *(double*)value1.value - *(double*)value2.value;
            output.type = EJVT_DOUBLE;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}

jvm_error_t jvm_and_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_IADD:
            *(int32_t*)output.value = *(int32_t*)value1.value & *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LAND:
            *(int64_t*)output.value = *(int64_t*)value1.value & *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}

jvm_error_t jvm_or_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_IOR:
            *(int32_t*)output.value = *(int32_t*)value1.value | *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LOR:
            *(int64_t*)output.value = *(int64_t*)value1.value | *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}

jvm_error_t jvm_xor_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_IXOR:
            *(int32_t*)output.value = *(int32_t*)value1.value ^ *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LXOR:
            *(int64_t*)output.value = *(int64_t*)value1.value ^ *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}


jvm_error_t jvm_div_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t output = {0};
    switch(opcode){
        case OP_IDIV:
            *(int32_t*)output.value = *(int32_t*)value1.value / *(int32_t*)value2.value;
            output.type = EJVT_INT;
            break;
        case OP_LDIV:
            *(int64_t*)output.value = *(int64_t*)value1.value / *(int64_t*)value2.value;
            output.type = EJVT_LONG;
            break;
        case OP_FDIV:
            *(float*)output.value = *(float*)value1.value / *(float*)value2.value;
            output.type = EJVT_FLOAT;
            break;
        case OP_DDIV:
            *(double*)output.value = *(double*)value1.value / *(double*)value2.value;
            output.type = EJVT_DOUBLE;
            break;

        default: break;
    }
    frame->stack.stack[frame->stack.sp++] = output;

    return err;
}

jvm_error_t jvm_new_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* cur_class_info = cur_class->info;
    classlinker_class_t* new_class = cur_class_info->constant_pool.constants[*(uint16_t*)args[0] - 1].constant_value;
    
    FAIL_SET_JUMP(new_class,err,JVM_NOTFOUND,exit);
    FAIL_SET_JUMP(new_class->type == EClass, err, JVM_OPCODE_INVALID,exit);

    jvm_value_t new_value = {EJVT_REFERENCE};

    objectmanager_object_t* object = objectmanager_new_class_object(frame,new_class);
    FAIL_SET_JUMP(object,err,JVM_OOM,exit);

    objectmanager_class_object_t* cobject = object->data;

    *(void**)new_value.value = object;
    frame->stack.stack[frame->stack.sp++] = new_value;

exit:
    return err;
}

jvm_error_t jvm_newarray_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    uint8_t jvm_array_type = *(uint8_t*)args[0];
    jvm_value_type_t array_type = EJVT_VOID;

    jvm_value_t count = frame->stack.stack[--frame->stack.sp];

    switch(jvm_array_type){
        default: break;

        case 4:
            array_type = EJVT_BOOL;
            break;
        case 5:
            array_type = EJVT_CHAR;
            break;
        case 6:
            array_type = EJVT_FLOAT;
            break;
        case 7:
            array_type = EJVT_DOUBLE;
            break;
        case 8:
            array_type = EJVT_BYTE;
            break;
        case 9:
            array_type = EJVT_SHORT;
            break;
        case 10:
            array_type = EJVT_INT;
            break;
        case 11:
            array_type = EJVT_LONG;
            break;
    }

    FAIL_SET_JUMP(*(int32_t*)count.value >= 0, err, JVM_OPCODE_INVALID,exit);
    FAIL_SET_JUMP(array_type != EJVT_VOID,err,JVM_OPCODE_INVALID,exit);

    jvm_value_t array = {EJVT_REFERENCE};

    objectmanager_object_t* object = objectmanager_new_array_object(frame,array_type, *(int32_t*)count.value);
    FAIL_SET_JUMP(object,err,JVM_OOM,exit);

    *(void**)array.value = object;

    frame->stack.stack[frame->stack.sp++] = array;

exit:
    return err;
}

jvm_error_t jvm_dup_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    assert(opcode == OP_DUP && "TODO: other OP_DUPs");

    uint16_t prev_sp = frame->stack.sp - 1;
    frame->stack.stack[frame->stack.sp++] = frame->stack.stack[prev_sp];
    return JVM_OK;
}

jvm_error_t jvm_invokevirtual_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* class_info = cur_class->info;

    classlinker_fmimref_t* method_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;
    classlinker_method_t* class_method = classlinker_find_method(frame,method_ref->class,method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(class_method,err,JVM_NOTFOUND,exit);

    jvm_value_t* method_args = alloca((class_method->frame_descriptor.arguments_count + 1) * sizeof(*method_args));

    for(unsigned i = class_method->frame_descriptor.arguments_count + 1; i-- > 1;){
        method_args[i] = frame->stack.stack[--frame->stack.sp];
    }
    jvm_value_t object = frame->stack.stack[--frame->stack.sp];
    method_args[0] = object;

    objectmanager_object_t* object_itself = *(void**)object.value;
    objectmanager_class_object_t* class_object = objectmanager_get_class_object_info(object_itself);
    FAIL_SET_JUMP(class_object,err,JVM_OPPARAM_INVALID,exit);

    FAIL_SET_JUMP(objectmanager_class_object_is_compatible_to(class_object,method_ref->class),err,JVM_OPPARAM_INVALID,exit);

    classlinker_method_t* lookedup_method = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(object_itself),
                                             method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(lookedup_method,err,JVM_NOTFOUND,exit);

    FAIL_SET_JUMP(strcmp(lookedup_method->name,"<init>") != 0 && strcmp(lookedup_method->name,"<clinit>") != 0, err,JVM_OPPARAM_INVALID,exit);

    jvm_error_t invoke_err = jvm_invoke(frame->jvm,frame,lookedup_method,class_method->frame_descriptor.arguments_count + 1,method_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

exit:
    return err;
}

jvm_error_t jvm_invokespecial_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* class_info = cur_class->info;

    classlinker_fmimref_t* method_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;
    classlinker_method_t* class_method = classlinker_find_method(frame,method_ref->class,method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(class_method,err,JVM_NOTFOUND,exit);

    jvm_value_t* method_args = alloca((class_method->frame_descriptor.arguments_count + 1) * sizeof(*method_args));

    for(unsigned i = class_method->frame_descriptor.arguments_count + 1; i-- > 1;){
        method_args[i] = frame->stack.stack[--frame->stack.sp];
    }
    jvm_value_t object = frame->stack.stack[--frame->stack.sp];
    method_args[0] = object;

    classlinker_method_t* lookedup_method = classlinker_find_method(frame,method_ref->class,method_ref->nameandtype.name,method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(lookedup_method,err,JVM_NOTFOUND,exit);

    jvm_error_t invoke_err = jvm_invoke(frame->jvm,frame,lookedup_method,class_method->frame_descriptor.arguments_count + 1,method_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

exit:
    return err;
}

jvm_error_t jvm_getfield_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(object);

    FAIL_SET_JUMP(cobject,err,JVM_OPPARAM_INVALID,exit);

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* field_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    FAIL_SET_JUMP(field_ref,err,JVM_NOTFOUND,exit);

    classlinker_field_t* field = objectmanager_class_object_get_field(frame,cobject, field_ref->nameandtype.name);
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);

    frame->stack.stack[frame->stack.sp++] = field->value;

exit:
    return err;
}
jvm_error_t jvm_putfield_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = frame->stack.stack[--frame->stack.sp];
    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(object);

    FAIL_SET_JUMP(cobject,err,JVM_OPPARAM_INVALID,exit);

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* field_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    FAIL_SET_JUMP(field_ref,err,JVM_NOTFOUND,exit);

    classlinker_field_t* field = objectmanager_class_object_get_field(frame,cobject, field_ref->nameandtype.name);
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);

    field->value = value;

exit:
    return err;
}

jvm_error_t jvm_athrow_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    return jvm_throw(frame,object);
}

jvm_error_t jvm_anewarray_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    uint8_t jvm_array_type = *(uint8_t*)args[0];

    jvm_value_t count = frame->stack.stack[--frame->stack.sp];
    FAIL_SET_JUMP(*(int32_t*)count.value >= 0, err,({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/NegativeArraySizeException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0],exception);

            errs = jvm_invoke(frame->jvm,frame,objectmanager_class_object_get_method(frame,ecobject,"<init>", "()V"),1,init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame,exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }),exit);

    jvm_value_t array = {EJVT_REFERENCE};

    objectmanager_object_t* object = objectmanager_new_array_object(frame,EJVT_REFERENCE, *(int32_t*)count.value);
    FAIL_SET_JUMP(object,err,JVM_OOM,exit);

    *(void**)array.value = object;

    frame->stack.stack[frame->stack.sp++] = array;

exit:
    return err;
}

jvm_error_t jvm_aconstnull(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    uint16_t sp = frame->stack.sp++;
    frame->stack.stack[sp].type = EJVT_REFERENCE;
    *(void**)frame->stack.stack[sp].value = NULL;

    return JVM_OK;
}

jvm_error_t jvm_arrayload_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    uint32_t index = *(uint32_t*)frame->stack.stack[--frame->stack.sp].value;
    objectmanager_object_t* array_obj = *(void**)frame->stack.stack[--frame->stack.sp].value;

    FAIL_SET_JUMP(array_obj,err,jvm_throw(frame,objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/NullPointerException"))),exit);
    FAIL_SET_JUMP(array_obj->type == EJOMOT_ARRAY,err,JVM_OPPARAM_INVALID,exit);

    objectmanager_array_object_t* array = objectmanager_get_array_object_info(array_obj);
    FAIL_SET_JUMP(index < array->count, err,({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/ArrayIndexOutOfBoundsException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0],exception);

            errs = jvm_invoke(frame->jvm,frame,objectmanager_class_object_get_method(frame,ecobject,"<init>", "()V"),1,init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame,exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }),exit);

    frame->stack.stack[frame->stack.sp++] = array->elements[index];

exit:
    return err;
}

jvm_error_t jvm_arraystore_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t to_store = frame->stack.stack[--frame->stack.sp];
    uint32_t index = *(uint32_t*)frame->stack.stack[--frame->stack.sp].value;
    objectmanager_object_t* array_obj = *(void**)frame->stack.stack[--frame->stack.sp].value;

    FAIL_SET_JUMP(array_obj,err,jvm_throw(frame,objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/NullPointerException"))),exit);
    FAIL_SET_JUMP(array_obj->type == EJOMOT_ARRAY,err,JVM_OPPARAM_INVALID,exit);

    objectmanager_array_object_t* array = objectmanager_get_array_object_info(array_obj);
    FAIL_SET_JUMP(index < array->count, err,({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/ArrayIndexOutOfBoundsException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0],exception);

            errs = jvm_invoke(frame->jvm,frame,objectmanager_class_object_get_method(frame,ecobject,"<init>", "()V"),1,init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame,exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }),exit);

    array->elements[index] = to_store;

exit:
    return err;
}

jvm_error_t jvm_dcmp_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    double Cvalue2 = *(double*)value2.value;
    double Cvalue1 = *(double*)value1.value;

    int Cresult = 1;
    switch(opcode){
        default: break;

        case OP_DCMPG:
            if(Cvalue1 > Cvalue2) Cresult = 1;
            if(Cvalue1 == Cvalue2) Cresult = 0;
            if(Cvalue1 < Cvalue2) Cresult = -1;
            if(Cvalue1 == NAN || Cvalue2 == NAN) Cresult = 1;
        break;

        case OP_DCMPL:
            if(Cvalue1 > Cvalue2) Cresult = 1;
            if(Cvalue1 == Cvalue2) Cresult = 0;
            if(Cvalue1 < Cvalue2) Cresult = -1;
            if(Cvalue1 == NAN || Cvalue2 == NAN) Cresult = -1;
        break;
    }

    jvm_value_t result = {EJVT_INT};
    *(int32_t*)result.value = Cresult;

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_fcmp_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    float Cvalue2 = *(float*)value2.value;
    float Cvalue1 = *(float*)value1.value;

    int Cresult = 1;
    switch(opcode){
        default: break;

        case OP_FCMPG:
            if(Cvalue1 > Cvalue2) Cresult = 1;
            if(Cvalue1 == Cvalue2) Cresult = 0;
            if(Cvalue1 < Cvalue2) Cresult = -1;
            if(Cvalue1 == NAN || Cvalue2 == NAN) Cresult = 1;
        break;

        case OP_FCMPL:
            if(Cvalue1 > Cvalue2) Cresult = 1;
            if(Cvalue1 == Cvalue2) Cresult = 0;
            if(Cvalue1 < Cvalue2) Cresult = -1;
            if(Cvalue1 == NAN || Cvalue2 == NAN) Cresult = -1;
        break;
    }

    jvm_value_t result = {EJVT_INT};
    *(int32_t*)result.value = Cresult;

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_neg_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_value_t* value = &frame->stack.stack[frame->stack.sp - 1];

    switch(opcode){
        default: break;
        case OP_INEG:
            *(int32_t*)value->value = -*(int32_t*)value->value;
        break;

        case OP_LNEG:
            *(int64_t*)value->value = -*(int64_t*)value->value;
        break;

        case OP_FNEG:
            *(float*)value->value = -*(float*)value->value;
        break;

        case OP_DNEG:
            *(double*)value->value = -*(double*)value->value;
        break;
    }

    return JVM_OK;
}

jvm_error_t jvm_instanceof_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    classlinker_class_t* class = ((classlinker_normalclass_t*)frame->method->class->info)->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    jvm_value_t result = {EJVT_INT};

    if(object){
        if(object->type == EJOMOT_ARRAY && class->type == Earray){
            TODO("type checking for arrays");
            *(int32_t*)result.value = 1;
        } else *(int32_t*)result.value = objectmanager_class_object_is_compatible_to(objectmanager_get_class_object_info(object),class);
    }

    frame->stack.stack[frame->stack.sp++] = result;
    return JVM_OK;
}

jvm_error_t jvm_checkcast_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* object = *(void**)frame->stack.stack[frame->stack.sp - 1].value;
    classlinker_class_t* class = ((classlinker_normalclass_t*)frame->method->class->info)->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    FAIL_SET_JUMP(object && objectmanager_class_object_is_compatible_to(objectmanager_get_class_object_info(object),class), err,({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/lang/ClassCastException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0],exception);

            errs = jvm_invoke(frame->jvm,frame,objectmanager_class_object_get_method(frame,ecobject,"<init>", "()V"),1,init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame,exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }),exit);

exit:
    return err;
}

jvm_error_t jvm_rem_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t result = {0};
    switch(opcode){
        default: break;

        case OP_FREM:{
            result.type = EJVT_FLOAT;
            *(float*)result.value = remainderf(*(float*)value1.value,*(float*)value2.value);
        }
        break;

        case OP_DREM:{
            result.type = EJVT_DOUBLE;
            *(double*)result.value = remainder(*(double*)value1.value,*(double*)value2.value);
        }
        break;

        case OP_IREM:{
            result.type = EJVT_INT;
            *(int32_t*)result.value = *(int32_t*)value1.value % *(int32_t*)value2.value;
        }
        break;

        case OP_LREM:{
            result.type = EJVT_LONG;
            *(int64_t*)result.value = *(int64_t*)value1.value % *(int64_t*)value2.value;
        }
        break;        
    }

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_shl_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t result = {0};
    switch(opcode){
        default: break;
        case OP_ISHL:{
            result.type = EJVT_INT;
            *(int32_t*)result.value = *(int32_t*)value1.value << *(int32_t*)value2.value;
        }
        break;

        case OP_LSHL:{
            result.type = EJVT_LONG;
            *(int64_t*)result.value = *(int64_t*)value1.value << *(int64_t*)value2.value;
        }
        break;        
    }

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_shr_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t result = {0};
    switch(opcode){
        default: break;
        case OP_ISHR:{
            result.type = EJVT_INT;
            *(int32_t*)result.value = *(int32_t*)value1.value >> *(int32_t*)value2.value;
        }
        break;

        case OP_LSHR:{
            result.type = EJVT_LONG;
            *(int64_t*)result.value = *(int64_t*)value1.value >> *(int64_t*)value2.value;
        }
        break;        
    }

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_lcmp_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];

    jvm_value_t result = {EJVT_INT};

    if(*(int64_t*)value1.value > *(int64_t*)value2.value) *(int32_t*)result.value = 1;
    if(*(int64_t*)value1.value == *(int64_t*)value2.value) *(int32_t*)result.value = 0;
    if(*(int64_t*)value1.value < *(int64_t*)value2.value) *(int32_t*)result.value = -1;

    frame->stack.stack[frame->stack.sp++] = result;

    return err;
}

jvm_error_t jvm_lconst_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_value_t result = {EJVT_LONG};

    switch(opcode){
        default: break;
        case OP_LCONST1:
            *(int64_t*)result.value = 1;
            break;
    }

    frame->stack.stack[frame->stack.sp++] = result;
    return JVM_OK;
}

jvm_error_t jvm_monitor_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;
    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    FAIL_SET_JUMP(object,err,JVM_OPPARAM_INVALID,exit);

    switch(opcode){
        default: break;
        case OP_MONITORENTER:
            objectmanager_object_lock(object);
            break;

        case OP_MONITOREXIT:
            objectmanager_object_unlock(object);
            break;

    }
exit:
    return err;
}

jvm_error_t jvm_swap_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_value_t value1 = frame->stack.stack[--frame->stack.sp];
    jvm_value_t value2 = frame->stack.stack[--frame->stack.sp];

    frame->stack.stack[frame->stack.sp++] = value1;
    frame->stack.stack[frame->stack.sp++] = value2;

    return JVM_OK;
}

jvm_error_t jvm_arraylength_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* obj = *(void**)frame->stack.stack[--frame->stack.sp].value;
    FAIL_SET_JUMP(obj,err,JVM_OPPARAM_INVALID,exit);

    objectmanager_array_object_t* array = objectmanager_get_array_object_info(obj);
    FAIL_SET_JUMP(array,err,JVM_OPPARAM_INVALID,exit);

    uint16_t sp = frame->stack.sp++;

    frame->stack.stack[sp].type = EJVT_INT;
    *(uint32_t*)frame->stack.stack[sp].value = array->count;

exit:
    return err;
}