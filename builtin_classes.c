#include "builtin_classes.h"
#include "class_linker.h"
#include "class_loader.h"
#include "jvm.h"
#include "jvm_internal.h"
#include "jvm_method.h"
#include "list.h"
#include "object.h"

#include <unistd.h>
#include <fcntl.h>

static jvm_error_t string_clinit(jvm_frame_t* frame);
static jvm_error_t object_clinit(jvm_frame_t* frame);
static jvm_error_t system_clinit(jvm_frame_t* frame);
static jvm_error_t ioexception_clinit(jvm_frame_t* frame);
static jvm_error_t printstream_clinit(jvm_frame_t* frame);
static jvm_error_t outputstream_clinit(jvm_frame_t* frame);

static jvm_error_t string_native_utf8_init(jvm_frame_t* frame);
static jvm_error_t outputstream_init(jvm_frame_t* frame);
static jvm_error_t outputstream_wfd_init(jvm_frame_t* frame);
static jvm_error_t printstream_init(jvm_frame_t* frame);
static jvm_error_t object_init(jvm_frame_t* frame);

static jvm_error_t ioexception_init(jvm_frame_t* frame);

static jvm_error_t outputstream_close(jvm_frame_t* frame);
static jvm_error_t outputstream_flush(jvm_frame_t* frame);
static jvm_error_t outputstream_writebytes(jvm_frame_t* frame);

static jvm_error_t printstream_printbool(jvm_frame_t* frame);
static jvm_error_t printstream_printchar(jvm_frame_t* frame);
static jvm_error_t printstream_printchararray(jvm_frame_t* frame);
static jvm_error_t printstream_printdouble(jvm_frame_t* frame);
static jvm_error_t printstream_printfloat(jvm_frame_t* frame);
static jvm_error_t printstream_printint(jvm_frame_t* frame);
static jvm_error_t printstream_printlong(jvm_frame_t* frame);
static jvm_error_t printstream_printobject(jvm_frame_t* frame);
static jvm_error_t printstream_printstring(jvm_frame_t* frame);
static jvm_error_t printstream_println(jvm_frame_t* frame);
static jvm_error_t printstream_printlnvoid(jvm_frame_t* frame);

classlinker_normalclass_t java_lang_Object_info = {
    .methods_count = 2,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = object_clinit,
            .flags = ACC_STATIC,
        },
        {
            .name = "<init>",
            .raw_description = "()V",
            .fn = object_init,
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
            .frame_descriptor.arguments_count = 1,
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

classlinker_normalclass_t java_io_IOException_info = {
    .methods_count = 2,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = ioexception_clinit,
            .flags = ACC_STATIC,
        },
        {
            .name = "<init>",
            .raw_description = "()V",
            .fn = ioexception_init,
        },
    }
};
classlinker_class_t java_io_IOException = {
    .this_name = "java/io/IOException",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &java_io_IOException_info,
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
            .fn = outputstream_init,
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
    .methods_count = 21,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = printstream_clinit,
            .flags = ACC_STATIC,

        },
        {
            .name = "<init>",
            .raw_description = "(Ljava/io/OutputStream;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_init,
        },
        {
            .name = "print",
            .raw_description = "(Z)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printbool,
        },
            {
            .name = "print",
            .raw_description = "(C)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printchar,
        },
        {
            .name = "print",
            .raw_description = "([C)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printchararray,
        },
        {
            .name = "print",
            .raw_description = "(D)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printdouble,
        },
        {
            .name = "print",
            .raw_description = "(F)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printfloat,
        },
        {
            .name = "print",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printint,
        },
        {
            .name = "print",
            .raw_description = "(J)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printlong,
        },
        {
            .name = "print",
            .raw_description = "(Ljava/lang/Object;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printobject,
        },
        {
            .name = "print",
            .raw_description = "(Ljava/lang/String;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printstring,
        },


        {
            .name = "println",
            .raw_description = "()V",
            .frame_descriptor.locals_count = 1,
            .fn = printstream_printlnvoid,
        },
        {
            .name = "println",
            .raw_description = "(Z)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
            {
            .name = "println",
            .raw_description = "(C)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "([C)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(D)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(F)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(J)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(Ljava/lang/Object;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
        },
        {
            .name = "println",
            .raw_description = "(Ljava/lang/String;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
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

static jvm_error_t ioexception_clinit(jvm_frame_t* frame){
    return JVM_OK;
}
static jvm_error_t ioexception_init(jvm_frame_t* frame){
    return JVM_OK;
}


static jvm_error_t string_clinit(jvm_frame_t* frame){
    return JVM_OK;
}

static jvm_error_t object_clinit(jvm_frame_t* frame){
    return JVM_OK;
}
static jvm_error_t object_init(jvm_frame_t* frame){
    return JVM_OK;
}

static jvm_error_t system_clinit(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    classlinker_field_t* field_out = classlinker_find_staticfield(frame,frame->method->class,"out");
    classlinker_field_t* field_err = classlinker_find_staticfield(frame,frame->method->class,"err");

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

    classlinker_method_t* console_init = objectmanager_object_get_method(frame,console_stream,"<init>", "(I)V");
    classlinker_method_t* outerr_init = objectmanager_object_get_method(frame,out_stream,"<init>", "(Ljava/io/OutputStream;)V");
 
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

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "fd");
    FAIL_SET_JUMP(field_fd,err,JVM_NOTFOUND,exit);

    field_fd->value.type = EJVT_INT;
    *(int32_t*)field_fd->value.value = fd; 

exit:
    return err;
}
static jvm_error_t outputstream_close(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = *(void**)frame->locals[0].value;

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "fd");
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

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "fd");
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

    classlinker_field_t* field_fd = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "fd");
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

    classlinker_field_t* output_stream = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "output_stream");
    *(void**)output_stream->value.value = stream;
    output_stream->value.type = EJVT_REFERENCE;

    return JVM_OK;
}

static jvm_error_t printstream_common(jvm_frame_t* frame, objectmanager_object_t* byte_array_object){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self_object = *(void**)frame->locals[0].value;

    classlinker_field_t* output_stream = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self_object), "output_stream");
    FAIL_SET_JUMP(output_stream,err,JVM_NOTFOUND,exit);

    objectmanager_object_t* output_stream_object = *(void**)output_stream->value.value;
    FAIL_SET_JUMP(output_stream,err,JVM_UNKNOWN,exit);

    classlinker_method_t* write_method = objectmanager_object_get_method(frame,output_stream_object,"write", "([B)V");
    FAIL_SET_JUMP(write_method,err,JVM_NOTFOUND,exit);

    jvm_value_t args[2] = {{EJVT_REFERENCE},{EJVT_REFERENCE}};
    *(void**)args[0].value = output_stream_object;
    *(void**)args[1].value = byte_array_object;

    err = jvm_invoke(frame->jvm,frame,write_method,sizeof(args) / sizeof(args[0]),args);

exit:
    return err;
}

static jvm_error_t printstream_printbool(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    uint32_t boolean = *(uint32_t*)frame->locals[1].value;

    char* Coutput_str = boolean != 0 ? "true" : "false";

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    for(unsigned i = 0; i < byte_array->count; i++){
        *(uint8_t*)byte_array->elements[i].value = Coutput_str[i];
    }

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printchar(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    uint32_t character = *(uint32_t*)frame->locals[1].value;

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,1);
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    *(uint32_t*)byte_array->elements[0].value = character;

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printchararray(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* byte_array_object = *(void**)frame->locals[1].value;

    err = printstream_common(frame,byte_array_object);

exit:
    return err;   
}

static jvm_error_t printstream_printdouble(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    double value = *(typeof(value)*)frame->locals[1].value;

    char Coutput_str[33] = {0};
    snprintf(Coutput_str,sizeof(Coutput_str) - 1,"%lf",value);

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    for(unsigned i = 0; i < byte_array->count; i++){
        *(uint8_t*)byte_array->elements[i].value = Coutput_str[i];
    }

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printfloat(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    float value = *(typeof(value)*)frame->locals[1].value;

    char Coutput_str[33] = {0};
    snprintf(Coutput_str,sizeof(Coutput_str) - 1,"%f",value);

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    for(unsigned i = 0; i < byte_array->count; i++){
        *(uint8_t*)byte_array->elements[i].value = Coutput_str[i];
    }

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printint(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    size_t value = *(uint32_t*)frame->locals[1].value;

    char Coutput_str[33] = {0};
    snprintf(Coutput_str,sizeof(Coutput_str) - 1,"%zu",value);

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    for(unsigned i = 0; i < byte_array->count; i++){
        *(uint8_t*)byte_array->elements[i].value = Coutput_str[i];
    }

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printlong(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    uint64_t value = *(uint64_t*)frame->locals[1].value;

    char Coutput_str[33] = {0};
    snprintf(Coutput_str,sizeof(Coutput_str) - 1,"%ld",value);

    objectmanager_object_t* byte_array_object = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    FAIL_SET_JUMP(byte_array_object,err,JVM_OOM,exit);

    objectmanager_array_object_t* byte_array = objectmanager_get_array_object_info(byte_array_object);
    for(unsigned i = 0; i < byte_array->count; i++){
        *(uint8_t*)byte_array->elements[i].value = Coutput_str[i];
    }

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

static jvm_error_t printstream_printobject(jvm_frame_t* frame){
    TODO("print object!");
    *(int*)1 = 0;
}

static jvm_error_t printstream_printstring(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* string = *(void**)frame->locals[1].value;

    objectmanager_object_t* byte_array_object = *(void**)objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(string), "UTF8_string")->value.value;
    FAIL_SET_JUMP(byte_array_object,err,JVM_OPCODE_INVALID,exit);

    err = printstream_common(frame,byte_array_object);

exit:
    return err;
}

struct{
    char* description;
    jvm_method_t method;
}printstream_println_dispach_table[] = {
    {"(Z)V",printstream_printbool},
    {"(C)V",printstream_printchar,},
    {"([C)V",printstream_printchararray},
    {"(D)V",printstream_printdouble,},
    {"(F)V",printstream_printfloat,},
    {"(I)V",printstream_printint},
    {"(J)V",printstream_printlong},
    {"(Ljava/lang/Object;)V",printstream_printobject},
    {"(Ljava/lang/String;)V",printstream_printstring},
};
static jvm_error_t printstream_println(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    char* description = frame->method->raw_description; //I was just tooooooooo lazy to copy past this shit with println
    jvm_method_t to_call = NULL;

    for(unsigned i = 0; i < sizeof(printstream_println_dispach_table) / sizeof(printstream_println_dispach_table[0]); i++){
        if(strcmp(printstream_println_dispach_table[i].description,description) == 0){
            to_call = printstream_println_dispach_table[i].method;
            break;
        }
    }
    FAIL_SET_JUMP(to_call,err,JVM_UNKNOWN,exit);
    FAIL_SET_JUMP(to_call(frame) == JVM_OK,err,JVM_UNKNOWN,exit);

    *(uint32_t*)frame->locals[1].value = '\n';
    err = printstream_printchar(frame);

exit:
    return err;
}

static jvm_error_t printstream_printlnvoid(jvm_frame_t* frame){
    *(uint32_t*)frame->locals[1].value = '\n';
    return printstream_printchar(frame);
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