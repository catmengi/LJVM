#include "opcodes.h"
#include "arena.h"
#include "class_linker.h"
#include "class_loader.h"

#include "jvm.h"
#include "jvm_internal.h"
#include "jvm_method.h"
#include "object.h"

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
        case EJCT_unitialised_string:{ //String fuckery because i cant initialise them in linker properly............
                objectmanager_object_t* string = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker,"java/lang/String"));
                FAIL_SET_JUMP(string,err,JVM_OOM,exit);

                classlinker_method_t* init_method = objectmanager_object_get_method(string,"<init>","(*)V");
                FAIL_SET_JUMP(init_method,err,JVM_NOTFOUND,exit);

                jvm_value_t args[2] = {{EJVT_REFERENCE},{EJVT_NATIVEPTR}};
                *(void**)args[0].value = string;
                *(void**)args[1].value = constant->constant_value;

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


    classlinker_field_t* field = classlinker_find_staticfield(field_ref->class, field_ref->nameandtype.name);
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


    classlinker_field_t* field = classlinker_find_staticfield(field_ref->class, field_ref->nameandtype.name);
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

exit:
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
            offset -= sizeof(uint16_t) - 1;
            offset += *(int16_t*)args[0];
            break;
        case OP_JSRw:
            offset -= sizeof(uint32_t) - 1;
            offset += *(int32_t*)args[0];
            break;
    }

    jvm_value_t addr = {EJVT_CODEADDR};
    *(uint32_t*)addr.value = offset - 2;

    frame->stack.stack[frame->stack.sp++] = addr;

exit:
    return err;
}

jvm_error_t jvm_ret_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    FAIL_SET_JUMP(*(uint8_t*)args[0] < frame->method->frame_descriptor.locals_count,err,JVM_OPCODE_INVALID,exit);
    jvm_value_t addr = frame->locals[*(uint8_t*)args[0]];

    FAIL_SET_JUMP(addr.type == EJVT_CODEADDR,err,JVM_OPCODE_INVALID,exit);
    frame->pc = (*(uint32_t*)addr.value) - 1;

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

exit:
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
    

exit:
    return err;
}

jvm_error_t jvm_invokestatic_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* class_info = cur_class->info;
    classlinker_fmimref_t* method_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;

    classlinker_method_t* method = classlinker_find_method(method_ref->class,method_ref->nameandtype.name,method_ref->nameandtype.descriptor);

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

exit:
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

exit:
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

jvm_error_t jvm_bastore_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = frame->stack.stack[--frame->stack.sp];
    uint32_t index = *(uint32_t*)frame->stack.stack[--frame->stack.sp].value;

    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    FAIL_SET_JUMP(object,err,JVM_UNKNOWN,exit);

    objectmanager_array_object_t* array = objectmanager_get_array_object_info(object);
    FAIL_SET_JUMP(array,err,JVM_UNKNOWN,exit);

    FAIL_SET_JUMP(index < array->count,err,JVM_ARRAY_OUTOFBOUNDS,exit);
    array->elements[index] = value;

exit:
    return err;
}

jvm_error_t jvm_i2ANY_opcodes(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    switch(opcode){
        case OP_I2B:
            frame->stack.stack[frame->stack.sp].type = EJVT_INT;
            break;
        case OP_I2S:
            frame->stack.stack[frame->stack.sp].type = EJVT_SHORT;
            break;
        case OP_I2C:
            frame->stack.stack[frame->stack.sp].type = EJVT_CHAR;
            break;
        case OP_I2L:
            frame->stack.stack[frame->stack.sp].type = EJVT_LONG;
            break;
        case OP_I2F:
            frame->stack.stack[frame->stack.sp].type = EJVT_FLOAT;
            break;
        case OP_I2D:
            frame->stack.stack[frame->stack.sp].type = EJVT_DOUBLE;
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
exit:
    return err;
}

jvm_error_t jvm_bipush_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    jvm_value_t value = {EJVT_INT};
    *(uint32_t*)value.value = *(uint8_t*)args[0];

    frame->stack.stack[frame->stack.sp++] = value;
exit:
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

exit:
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
    TODO("Other variations of OP_DUP need to be implemented!");
    uint16_t prev_sp = frame->stack.sp - 1;
    frame->stack.stack[frame->stack.sp++] = frame->stack.stack[prev_sp];
    return JVM_OK;
}

jvm_error_t jvm_invokevirtual_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    jvm_error_t err = JVM_OK;

    classlinker_normalclass_t* class_info = cur_class->info;

    classlinker_fmimref_t* method_ref = class_info->constant_pool.constants[(*(uint16_t*)args[0]) - 1].constant_value;
    classlinker_method_t* class_method = classlinker_find_method(method_ref->class,method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
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

    classlinker_method_t* lookedup_method = objectmanager_object_get_method(object_itself, method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
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
    classlinker_method_t* class_method = classlinker_find_method(method_ref->class,method_ref->nameandtype.name, method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(class_method,err,JVM_NOTFOUND,exit);

    jvm_value_t* method_args = alloca((class_method->frame_descriptor.arguments_count + 1) * sizeof(*method_args));

    for(unsigned i = class_method->frame_descriptor.arguments_count + 1; i-- > 1;){
        method_args[i] = frame->stack.stack[--frame->stack.sp];
    }
    jvm_value_t object = frame->stack.stack[--frame->stack.sp];
    method_args[0] = object;

    classlinker_method_t* lookedup_method = classlinker_find_method(method_ref->class,method_ref->nameandtype.name,method_ref->nameandtype.descriptor);
    FAIL_SET_JUMP(lookedup_method,err,JVM_NOTFOUND,exit);

    FAIL_SET_JUMP(strcmp(lookedup_method->name,"<init>") == 0 || strcmp(lookedup_method->name,"<clinit>") == 0, err,JVM_OPPARAM_INVALID,exit);

    jvm_error_t invoke_err = jvm_invoke(frame->jvm,frame,lookedup_method,class_method->frame_descriptor.arguments_count + 1,method_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

exit:
    return err;
}

jvm_error_t jvm_athrow_opcode(jvm_opcode_t opcode, jvm_frame_t* frame, classlinker_class_t* cur_class, unsigned nargs, void* args[]){
    objectmanager_object_t* object = *(void**)frame->stack.stack[--frame->stack.sp].value;
    return jvm_throw(frame,object);
}