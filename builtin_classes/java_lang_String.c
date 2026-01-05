#include "../jvm.h"
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

static jvm_error_t string_init_chars_offlen(jvm_frame_t* frame){
    return JVM_OK;
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

static jvm_error_t string_getBytes(jvm_frame_t* frame){
    jvm_native_return(frame,objectmanager_class_object_get_field(frame,
                    objectmanager_get_class_object_info(JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*)), "UTF8_string")->value);

    return JVM_OK;
}

classlinker_normalclass_t java_lang_String_info = {
    .methods_count = 7,
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
            .name = "getBytes",
            .raw_description = "()[B",
            .fn = string_getBytes,
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