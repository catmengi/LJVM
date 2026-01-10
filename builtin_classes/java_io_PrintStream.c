#include "../jvm.h"
#include "../jvm_internal.h"
#include "../object.h"
#include "../class_linker.h"


extern classlinker_class_t java_io_OutputStream;

static jvm_error_t printstream_clinit(jvm_frame_t* frame){
    return JVM_OK;
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

    classlinker_method_t* write_method = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(output_stream_object),"write", "([B)V");
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

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,1);
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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

    return err;   
}

static jvm_error_t printstream_printdouble(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    double value = *(typeof(value)*)frame->locals[1].value;

    char Coutput_str[33] = {0};
    snprintf(Coutput_str,sizeof(Coutput_str) - 1,"%lf",value);

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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

    frame->locals[2].type = EJVT_REFERENCE; //To tell GC we are using it
    *(void**)frame->locals[2].value = objectmanager_new_array_object(frame, EJVT_BYTE,strlen(Coutput_str));
    objectmanager_object_t* byte_array_object = *(void**)frame->locals[2].value;
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
    printf("%p",*(void**)frame->locals[1].value);
    return JVM_OK;
}
static jvm_error_t printstream_printlnobject(jvm_frame_t* frame){
    printf("%p\n",*(void**)frame->locals[1].value);
    return JVM_OK;
}

static jvm_error_t printstream_printstring(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* string = *(void**)frame->locals[1].value;

    classlinker_method_t* getBytes = objectmanager_class_object_get_method(frame, objectmanager_get_class_object_info(string), "getBytes", "()[B");
    FAIL_SET_JUMP(getBytes,err,JVM_NOTFOUND,exit);

    jvm_value_t args[1];
    C_TO_JVM_VALUE(args[0],string);
    jvm_invoke(frame->jvm,frame,getBytes,1,args);

    objectmanager_object_t* byte_array_object = JVM_TO_C_VALUE(jvm_native_get_return(frame),objectmanager_object_t*);
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

    C_TO_JVM_VALUE(frame->locals[1],(char)'\n');
    err = printstream_printchar(frame);
exit:
    return err;
}

static jvm_error_t printstream_printlnvoid(jvm_frame_t* frame){
    *(uint32_t*)frame->locals[1].value = '\n';
    return printstream_printchar(frame);
}


classlinker_normalclass_t java_io_PrintStream_info = {
    .fields_count = 1,
    .fields = (classlinker_field_t[]){
        {
            .name = "output_stream",
            .value.type = EJVT_REFERENCE,
        },
    },
    .methods_count = 21,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = printstream_clinit,
            .flags = ACC_STATIC | ACC_NATIVE,

        },
        {
            .name = "<init>",
            .raw_description = "(Ljava/io/OutputStream;)V",
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_init,
            .flags = ACC_NATIVE,
        },
        {
            .name = "print",
            .raw_description = "(Z)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printbool,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
            {
            .name = "print",
            .raw_description = "(C)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printchar,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "([C)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printchararray,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(D)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printdouble,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(F)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printfloat,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(I)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printint,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(J)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printlong,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(Ljava/lang/Object;)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printobject,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "print",
            .raw_description = "(Ljava/lang/String;)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.stack_size = 1,
            .fn = printstream_printstring,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },


        {
            .name = "println",
            .raw_description = "()V",
            .frame_descriptor.locals_count = 2,
            .fn = printstream_printlnvoid,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(Z)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
            {
            .name = "println",
            .raw_description = "(C)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "([C)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(D)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(F)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED ,
        },
        {
            .name = "println",
            .raw_description = "(I)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(J)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(Ljava/lang/Object;)V",
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.arguments_count = 1,
            .fn = printstream_printlnobject,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
        {
            .name = "println",
            .raw_description = "(Ljava/lang/String;)V",
            .frame_descriptor.arguments_count = 1,
            .frame_descriptor.locals_count = 1,
            .frame_descriptor.stack_size = 1,
            .fn = printstream_println,
            .flags = ACC_NATIVE | ACC_SYNCHRONIZED,
        },
    }
};
classlinker_class_t java_io_PrintStream = {
    .this_name = "java/io/PrintStream",
    .generation = 2,
    .parent = &java_io_OutputStream,
    .info = &java_io_PrintStream_info,
};