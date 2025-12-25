#include "builtin_classes.h"
#include "class_linker.h"
#include "class_loader.h"
#include "jvm.h"
#include "jvm_internal.h"
#include "list.h"
#include "object.h"

#include <unistd.h>
#include <fcntl.h>

static jvm_error_t string_clinit(jvm_frame_t* frame);
static jvm_error_t object_clinit(jvm_frame_t* frame);
static jvm_error_t system_clinit(jvm_frame_t* frame);
static jvm_error_t printstream_clinit(jvm_frame_t* frame);
static jvm_error_t outputstream_clinit(jvm_frame_t* frame);

static jvm_error_t string_native_utf8_init(jvm_frame_t* frame);
static jvm_error_t outputstream_init(jvm_frame_t* frame);
static jvm_error_t outputstream_wfd_init(jvm_frame_t* frame);
static jvm_error_t printstream_init(jvm_frame_t* frame);

static jvm_error_t outputstream_close(jvm_frame_t* frame);
static jvm_error_t outputstream_flush(jvm_frame_t* frame);
static jvm_error_t outputstream_writebytes(jvm_frame_t* frame);

classlinker_normalclass_t java_lang_Object_info = {
    .methods_count = 1,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = object_clinit,
            .flags = ACC_STATIC,
        },
    },
};

classlinker_class_t java_lang_Object = {
    .this_name = "java/lang/Object",
    .info = &java_lang_Object_info,
};

classlinker_normalclass_t java_lang_String_info = {
    .methods_count = 2,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = string_clinit,
            .flags = ACC_STATIC,
        },
        {
            .name = "<init>",
            .raw_description = "(*)V",
            .fn = string_native_utf8_init,
            .frame_descriptor.arguments_count = 2,
            .flags = ACC_STATIC,
        }
    },
    .fields_count = 1,
    .fields = (classlinker_field_t[]){
        {
            .name = "UTF8_string",
            },
    }
};

classlinker_class_t java_lang_String = {
    .this_name = "java/lang/String",
    .parent = &java_lang_Object,
    .info = &java_lang_String_info,
    .generation = 1,
};

classlinker_class_t java_io_IOException = {
    .this_name = "java/io/IOException",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &(classlinker_normalclass_t){}
};

classlinker_normalclass_t java_io_OutputStream_info = {
    .methods_count = 6,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = outputstream_clinit,
            .flags = ACC_STATIC,
        },
        {
            .name = "<init>",
            .raw_description = "()V",
            .frame_descriptor.arguments_count = 1,
            .fn = outputstream_init,
            .flags = ACC_STATIC,
        },
        {
            .name = "<init>",
            .raw_description = "(I)V", //Secret custom FD output init :)
            .frame_descriptor.arguments_count = 2,
            .fn = outputstream_wfd_init,
            .flags = ACC_STATIC,
        },
        {
            .name = "close",
            .raw_description = "()V", //Secret custom FD function!
            .fn = outputstream_close,
        },
        {
            .name = "flush",
            .raw_description = "()V", //Secret custom FD function!
            .fn = outputstream_close,
        },
        {
            .name = "write",
            .raw_description = "([B)V", //Secret custom FD function!
            .frame_descriptor.arguments_count = 1,
            .fn = outputstream_writebytes,
        },
    },
    .fields_count = 1,
    .fields = (classlinker_field_t[]){
        {
            .name = "fd",
        },
    },
};

classlinker_class_t java_io_OutputStream = {
    .this_name = "java/io/OutputStream",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &java_io_OutputStream_info,
};

classlinker_normalclass_t java_io_PrintStream_info = {
    .fields_count = 1,
    .fields = (classlinker_field_t[]){
        {
            .name = "output_stream",
        },
    },
    .methods_count = 2,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .flags = ACC_STATIC,
            .fn = printstream_clinit,
        },
        {
            .name = "<init>",
            .raw_description = "(Ljava/io/OutputStream;)V",
            .frame_descriptor.arguments_count = 2,
            .fn = printstream_init,
            .flags = ACC_STATIC,
        },
    }
};
classlinker_class_t java_io_PrintStream = {
    .this_name = "java/io/PrintStream",
    .generation = 2,
    .parent = &java_io_OutputStream,
    .info = &java_io_PrintStream_info,
};


classlinker_normalclass_t java_lang_System_info = {
    .static_fields_count = 2,
    .static_fields = (classlinker_field_t[]){
        {
            .name = "out",
            .flags = ACC_STATIC,
        },
        {
            .name = "err",
            .flags = ACC_STATIC,
        }
    },

    .methods_count = 1,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = system_clinit,
            .flags = ACC_STATIC,
        },
    }
};

classlinker_class_t java_lang_System = {
    .this_name = "java/lang/System",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &java_lang_System_info,
};




classlinker_class_t* builtin_classes[] = {
    &java_lang_Object,
    &java_lang_String,
    &java_io_IOException,
    &java_io_OutputStream,
    &java_io_PrintStream,
    &java_lang_System,
};

void builtin_classes_init(classlinker_instance_t* linker){
    for(unsigned i = 0; i < sizeof(builtin_classes) / sizeof(builtin_classes[0]); i++){
        classlinker_class_t* class = builtin_classes[i];

        classlinker_class_t* copy = arena_calloc(linker->arena,1,sizeof(*copy));
        assert(copy);

        *copy = *class;
        INIT_LIST_HEAD(&copy->list);
        list_add_tail(&copy->list,&linker->loaded_classes);
    }
}



static jvm_error_t string_clinit(jvm_frame_t* frame){
    return JVM_OK;
}

static jvm_error_t object_clinit(jvm_frame_t* frame){
    return JVM_OK;
}

static jvm_error_t system_clinit(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    classlinker_field_t* field_out = classlinker_find_staticfield(frame->method->class,"out");
    classlinker_field_t* field_err = classlinker_find_staticfield(frame->method->class,"err");

    FAIL_SET_JUMP(field_out,err,JVM_NOTFOUND,exit);
    FAIL_SET_JUMP(field_err,err,JVM_NOTFOUND,exit);

    field_out->value.type = EJVT_REFERENCE;
    field_err->value.type = EJVT_REFERENCE;

    objectmanager_object_t* out_stream = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker,"java/io/PrintStream"));
    objectmanager_object_t* err_stream = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker,"java/io/PrintStream"));

    objectmanager_object_t* console_stream = objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker, "java/io/OutputStream"));


    FAIL_SET_JUMP(out_stream,err,JVM_OOM,exit);
    FAIL_SET_JUMP(err_stream,err,JVM_OOM,exit);
    FAIL_SET_JUMP(console_stream,err,JVM_OOM,exit);

    *(void**)field_out->value.value = out_stream;
    *(void**)field_err->value.value = out_stream;

    classlinker_method_t* console_init = objectmanager_class_object_get_method(objectmanager_get_class_object_info(console_stream),"<init>", "(I)V");
    classlinker_method_t* outerr_init = objectmanager_class_object_get_method(objectmanager_get_class_object_info(out_stream),"<init>", "(Ljava/io/OutputStream;)V");
 
    FAIL_SET_JUMP(console_init,err,JVM_NOTFOUND,exit);
    FAIL_SET_JUMP(outerr_init,err,JVM_NOTFOUND,exit);

    jvm_value_t invoke_args[] = {{EJVT_REFERENCE},{EJVT_REFERENCE}};

    *(void**)invoke_args[0].value = console_stream;
    *(int32_t*)invoke_args[1].value = fileno(stdout);

    jvm_error_t invoke_err = JVM_OK;

    invoke_err = jvm_invoke(frame->jvm,frame,console_init,2,invoke_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

    *(void**)invoke_args[0].value = out_stream;
    *(void**)invoke_args[1].value = console_stream; 

    invoke_err = jvm_invoke(frame->jvm,frame,outerr_init,2,invoke_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

    *(void**)invoke_args[0].value = err_stream;

    invoke_err = jvm_invoke(frame->jvm,frame,outerr_init,2,invoke_args);
    FAIL_SET_JUMP(invoke_err == JVM_OK,err,invoke_err,exit);

exit:
    return err;
}

static jvm_error_t outputstream_clinit(jvm_frame_t* frame){
    return JVM_OK;
}
static jvm_error_t printstream_clinit(jvm_frame_t* frame){
    return JVM_OK;
}





static jvm_error_t outputstream_init(jvm_frame_t* frame){
    return JVM_OK;
}
static jvm_error_t outputstream_wfd_init(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = *(void**)frame->locals[0].value;
    int32_t fd = *(int32_t*)frame->locals[1].value;

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "fd");
    FAIL_SET_JUMP(field_fd,err,JVM_NOTFOUND,exit);

    field_fd->value.type = EJVT_INT;
    *(int32_t*)field_fd->value.value = fd; 

exit:
    return err;
}
static jvm_error_t outputstream_close(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = *(void**)frame->locals[0].value;

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "fd");
    FAIL_SET_JUMP(field_fd,err,JVM_NOTFOUND,exit);

    int32_t fd = *(int32_t*)field_fd->value.value;
    FAIL_SET_JUMP(fd >= 0,err,JVM_UNKNOWN,exit);

    close(fd);

exit:
    return err;
}
static jvm_error_t outputstream_flush(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = *(void**)frame->locals[0].value;

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "fd");
    FAIL_SET_JUMP(field_fd,err,JVM_NOTFOUND,exit);

    int32_t fd = *(int32_t*)field_fd->value.value;
    FAIL_SET_JUMP(fd >= 0,err,JVM_UNKNOWN,exit);

    fsync(fd);

exit:
    return err;
}

static jvm_error_t outputstream_writebytes(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = *(void**)frame->locals[0].value;
    objectmanager_object_t* bytes = *(void**)frame->locals[1].value;

    objectmanager_array_object_t* bytes_array = objectmanager_get_array_object_info(bytes);
    uint8_t Cbytes[bytes_array->count];

    FAIL_SET_JUMP(bytes_array,err,JVM_UNKNOWN,exit);

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "fd");
    FAIL_SET_JUMP(field_fd,err,JVM_NOTFOUND,exit);

    int32_t fd = *(int32_t*)field_fd->value.value;
    FAIL_SET_JUMP(fd >= 0,err,JVM_UNKNOWN,exit);

    for(unsigned i = 0; i < bytes_array->count; i++){
        Cbytes[i] = *(uint8_t*)bytes_array->elements[i].value;
    }
    write(fd,Cbytes,bytes_array->count);

exit:
    return err;
}


static jvm_error_t printstream_init(jvm_frame_t* frame){

    objectmanager_object_t* self = *(void**)frame->locals[0].value;
    objectmanager_object_t* stream = *(void**)frame->locals[1].value;

    classlinker_field_t* output_stream = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "output_stream");
    *(void**)output_stream->value.value = stream;
    output_stream->value.type = EJVT_REFERENCE;

    return JVM_OK;
}

static jvm_error_t string_native_utf8_init(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = *(void**)frame->locals[0].value;
    char* native_utf8 = *(void**)frame->locals[1].value;

    jvm_value_t UTF8_string = {EJVT_REFERENCE};
    
    objectmanager_object_t* UTF8_array = objectmanager_new_array_object(frame, EJVT_CHAR, strlen(native_utf8) - 1);
    FAIL_SET_JUMP(UTF8_array,err,JVM_OOM,exit);

    objectmanager_array_object_t* array_itself = objectmanager_get_array_object_info(UTF8_array);
    for(unsigned i = 0; i < array_itself->count; i++){
        *(char*)array_itself->elements[i].value = native_utf8[i];
    }

    classlinker_field_t* field = objectmanager_class_object_get_field(objectmanager_get_class_object_info(self), "UTF8_string");
    FAIL_SET_JUMP(field,err,JVM_NOTFOUND,exit);

    field->value.type = EJVT_REFERENCE;
    *(void**)field->value.value = UTF8_array;

exit:
    return err;
}