#include "../jvm.h"
#include "../jvm_internal.h"
#include "../object.h"
#include "../class_linker.h"

static jvm_error_t string_clinit(jvm_frame_t* frame){
    return JVM_OK;
}

static jvm_error_t string_init(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    classlinker_field_t* self_internals = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "UTF8_string");

    C_TO_JVM_VALUE(self_internals->value, objectmanager_new_array_object(frame, EJVT_BYTE, 0));
    FAIL_SET_JUMP(JVM_TO_C_VALUE(self_internals->value, objectmanager_object_t*),err,JVM_OOM,exit);

exit:
    return err;
}

static jvm_error_t string_init_chars(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    classlinker_field_t* self_internals = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "UTF8_string");

    C_TO_JVM_VALUE(self_internals->value,objectmanager_object_clone(frame, JVM_TO_C_VALUE(frame->locals[1],objectmanager_object_t*)));
    FAIL_SET_JUMP(JVM_TO_C_VALUE(self_internals->value, objectmanager_object_t*),err,JVM_OOM,exit);
exit:
    return err;
}

static jvm_error_t string_init_chars_offlen(jvm_frame_t* frame) {
    jvm_error_t err = JVM_OK;
    bool do_throw = false;
    char* throw_what = NULL;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    objectmanager_object_t* char_array_obj = JVM_TO_C_VALUE(frame->locals[1], objectmanager_object_t*);
    int32_t offset = JVM_TO_C_VALUE(frame->locals[2], int32_t);
    int32_t length = JVM_TO_C_VALUE(frame->locals[3], int32_t);

    FAIL_SET_JUMP(char_array_obj, do_throw,
        ({ throw_what = "java/lang/NullPointerException"; (true); }), exit);

    objectmanager_array_object_t* char_array = objectmanager_get_array_object_info(char_array_obj);
    
    // Bounds checking
    FAIL_SET_JUMP(offset >= 0, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);
    
    FAIL_SET_JUMP(length >= 0, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);
    
    FAIL_SET_JUMP(offset + length <= char_array->count, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);

    // Create UTF-8 byte array
    jvm_lock(frame->jvm);

    objectmanager_object_t* utf8_array = objectmanager_new_array_object(frame, EJVT_BYTE, length);
    FAIL_SET_JUMP(utf8_array, err, JVM_OOM, exit);

    jvm_unlock(frame->jvm);

    objectmanager_array_object_t* utf8_array_info = objectmanager_get_array_object_info(utf8_array);
    
    // Copy characters (assuming ASCII)
    for (int32_t i = 0; i < length; i++) {
        uint16_t char_val = JVM_TO_C_VALUE(char_array->elements[offset + i], uint16_t);
        if (char_val > 255) {
            // Non-ASCII character - store as '?' (63)
            char_val = 63;
        }
        utf8_array_info->elements[i].type = EJVT_BYTE;
        uint8_t* byte_ptr = (uint8_t*)utf8_array_info->elements[i].value;
        *byte_ptr = (uint8_t)char_val;
    }

    // Store in string field
    classlinker_field_t* utf8_field = objectmanager_class_object_get_field(
        frame, objectmanager_get_class_object_info(self), "UTF8_string");
    FAIL_SET_JUMP(utf8_field, do_throw,
        ({ throw_what = "java/lang/InternalError"; (true); }), exit);

    C_TO_JVM_VALUE(utf8_field->value, utf8_array);

exit:
    if (do_throw) {
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(
            frame, classlinker_find_class(frame->jvm->linker, throw_what));
        if (!exception) {
            err = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            err = jvm_throw(frame, exception);
            jvm_unlock(frame->jvm);
        }
    }

    return err;
}

static jvm_error_t string_string_init(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    objectmanager_object_t* init_from = JVM_TO_C_VALUE(frame->locals[1],objectmanager_object_t*);

    classlinker_field_t* self_internals = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "UTF8_string");
    classlinker_field_t* init_from_internals = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(init_from), "UTF8_string");

    self_internals->value = init_from_internals->value; //Just copy pointers, and gc will handle this

    assert(self_internals->value.type == EJVT_REFERENCE);

exit:
    return err;
}

static jvm_error_t string_native_utf8_init(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = *(void**)frame->locals[0].value;
    char* native_utf8 = *(void**)frame->locals[1].value;

    jvm_value_t UTF8_string = {EJVT_REFERENCE};
    
    objectmanager_object_t* UTF8_array = objectmanager_new_array_object(frame, EJVT_BYTE, strlen(native_utf8));
    FAIL_SET_JUMP(UTF8_array,err,JVM_OOM,exit);

    objectmanager_array_object_t* array_itself = objectmanager_get_array_object_info(UTF8_array);
    for(unsigned i = 0; i < array_itself->count; i++){
        *(char*)array_itself->elements[i].value = native_utf8[i];
    }

    classlinker_field_t* field = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "UTF8_string");
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);

    field->value.type = EJVT_REFERENCE;
    *(void**)field->value.value = UTF8_array;

exit:
    return err;
}

static jvm_error_t string_valueOfB(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    bool value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char* cstr = value ? "true" : "false";

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE(cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfC(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    char value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char* cstr = ((char[2]){value,0});

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE(cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfD(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    double value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char cstr[64];
    sprintf(cstr,"%lf",value);

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE((char*)cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfF(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    float value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char cstr[64];
    sprintf(cstr,"%f",value);

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE((char*)cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfI(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    uint32_t value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char cstr[64];
    sprintf(cstr,"%zu",(size_t)value);

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE((char*)cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfL(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    uint64_t value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char cstr[64];
    sprintf(cstr,"%zu",(size_t)value);

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE((char*)cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfO(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "(*)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    objectmanager_object_t* value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));
    char cstr[64];
    snprintf(cstr,sizeof(cstr),"%s@%p",objectmanager_get_class_object_info(value)->class->this_name,value);

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE((char*)cstr)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfCA(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "([C)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    objectmanager_object_t* value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));

    jvm_value_t args[2] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE(value)};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_valueOfCAOFFLEN(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;    

    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker,"java/lang/String")));
    objectmanager_object_t* str = JVM_TO_C_VALUE(frame->locals[1],typeof(str));

    FAIL_SET_JUMP(str,err,JVM_OK,exit);

    classlinker_method_t* init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(str), "<init>", "([CII)V");
    FAIL_SET_JUMP(init,err,JVM_NOTFOUND,exit);

    objectmanager_object_t* value = JVM_TO_C_VALUE(frame->locals[0],typeof(value));

    jvm_value_t args[4] = {C_TO_NEW_JVM_VALUE(str),C_TO_NEW_JVM_VALUE(value),frame->locals[2],frame->locals[3]};
    err = jvm_invoke(frame->jvm,frame,init,2,args);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(str));
exit:
    return err;
}

static jvm_error_t string_length(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    objectmanager_object_t* utf8_string = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "UTF8_string")->value,typeof(utf8_string));

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(objectmanager_get_array_object_info(utf8_string)->count));
    return JVM_OK;
}

static jvm_error_t string_getBytes(jvm_frame_t* frame){
    jvm_native_return(frame,objectmanager_class_object_get_field(frame,
                    objectmanager_get_class_object_info(JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*)), "UTF8_string")->value);

    return JVM_OK;
}

static jvm_error_t string_getChars(jvm_frame_t* frame) {
    jvm_error_t err = JVM_OK;
    bool do_throw = false;
    char* throw_what = NULL;

    // Extract parameters: void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin)
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    int32_t srcBegin = JVM_TO_C_VALUE(frame->locals[1], int32_t);
    int32_t srcEnd = JVM_TO_C_VALUE(frame->locals[2], int32_t);
    objectmanager_object_t* dst_obj = JVM_TO_C_VALUE(frame->locals[3], objectmanager_object_t*);
    int32_t dstBegin = JVM_TO_C_VALUE(frame->locals[4], int32_t);

    // Check for null destination array
    FAIL_SET_JUMP(dst_obj, do_throw, 
        ({ throw_what = "java/lang/NullPointerException"; (true); }), exit);

    // Get the string's internal UTF-8 byte array
    classlinker_field_t* utf8_field = objectmanager_class_object_get_field(
        frame, objectmanager_get_class_object_info(self), "UTF8_string");
    FAIL_SET_JUMP(utf8_field, err, JVM_UNKNOWN, exit);

    objectmanager_object_t* utf8_array_obj = JVM_TO_C_VALUE(utf8_field->value, objectmanager_object_t*);
    if (!utf8_array_obj) {
        // Empty string - nothing to copy
        goto exit;
    }

    objectmanager_array_object_t* utf8_array = objectmanager_get_array_object_info(utf8_array_obj);
    objectmanager_array_object_t* dst_array = objectmanager_get_array_object_info(dst_obj);

    // Get string length from byte array
    int32_t string_length = utf8_array->count;

    // Bounds checking
    FAIL_SET_JUMP(srcBegin >= 0, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);
    
    FAIL_SET_JUMP(srcEnd <= string_length, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);
    
    FAIL_SET_JUMP(srcBegin <= srcEnd, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);

    FAIL_SET_JUMP(dstBegin >= 0, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);
    
    FAIL_SET_JUMP(dstBegin <= dst_array->count, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);

    int32_t copy_length = srcEnd - srcBegin;
    FAIL_SET_JUMP(dstBegin + copy_length <= dst_array->count, do_throw,
        ({ throw_what = "java/lang/IndexOutOfBoundsException"; (true); }), exit);

    // Copy bytes from UTF-8 array to char array
    for (int32_t i = 0; i < copy_length; i++) {
        // Get byte from UTF-8 array
        uint8_t byte_val = JVM_TO_C_VALUE(utf8_array->elements[srcBegin + i], uint8_t);
        
        // Store as char in destination array
        dst_array->elements[dstBegin + i].type = EJVT_CHAR;
        uint16_t* char_ptr = (uint16_t*)dst_array->elements[dstBegin + i].value;
        *char_ptr = (uint16_t)byte_val;
    }

exit:
    if (do_throw) {
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(
            frame, classlinker_find_class(frame->jvm->linker, throw_what));
        if (!exception) {
            err = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            err = jvm_throw(frame, exception);
            jvm_unlock(frame->jvm);
        }
    }

    return err;
}

classlinker_normalclass_t java_lang_String_info = {
    .methods_count = 18,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = string_clinit,
            .flags = ACC_STATIC | ACC_NATIVE,
        },
        {
            .name = "<init>",
            .raw_description = "(*)V",
            .fn = string_native_utf8_init,
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
        },
        {
            .name = "<init>",
            .raw_description = "()V",
            .fn = string_init,
            .flags = ACC_NATIVE,            
        },
        {
            .name = "<init>",
            .raw_description = "(Ljava/lang/String;)V",
            .fn = string_string_init,
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,                
        },
        {
            .name = "<init>",
            .raw_description = "([C)V",
            .fn = string_init_chars,
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
        },
        {
            .name = "<init>",
            .raw_description = "([CII)V",
            .fn = string_init_chars_offlen,
            .frame_descriptor.arguments_count = 3,
            .flags = ACC_NATIVE,
        },
        {
            .name = "valueOf",
            .raw_description = "(Z)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfB,
        },
        {
            .name = "valueOf",
            .raw_description = "(C)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfC,
        },
        {
            .name = "valueOf",
            .raw_description = "([C)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfCA,
        },
        {
            .name = "valueOf",
            .raw_description = "([CII)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 3,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfCAOFFLEN,
        },
        {
            .name = "valueOf",
            .raw_description = "(D)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfD,
        },
        {
            .name = "valueOf",
            .raw_description = "(F)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfF,
        },
        {
            .name = "valueOf",
            .raw_description = "(I)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfI,
        },
        {
            .name = "valueOf",
            .raw_description = "(J)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfL,
        },
        {
            .name = "valueOf",
            .raw_description = "(Ljava/lang/Object;)Ljava/lang/String;",
            .flags = ACC_STATIC | ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .fn = string_valueOfO,
        },
        {
            .name = "length",
            .raw_description = "()I",
            .flags = ACC_NATIVE,
            .fn = string_length,
        },
        {
            .name = "getBytes",
            .raw_description = "()[B",
            .fn = string_getBytes,
            .flags = ACC_NATIVE,
        },
        {
            .name = "getChars",
            .raw_description = "(II[CI)V",
            .fn = string_getChars,
            .frame_descriptor.arguments_count = 4,
            .frame_descriptor.locals_count = 5,
            .flags = ACC_NATIVE,
        },
    },
    .fields_count = 1,
    .fields = (classlinker_field_t[]){
        {
            .name = "UTF8_string",
            .flags = ACC_PRIVATE,
            .value.type = EJVT_REFERENCE,
            },
    }
};

extern classlinker_class_t java_lang_Object;
classlinker_class_t java_lang_String = {
    .this_name = "java/lang/String",
    .parent = &java_lang_Object,
    .info = &java_lang_String_info,
    .generation = 1,
};