#include "../jvm.h"
#include "../jvm_internal.h"
#include "../object.h"
#include "../class_linker.h"
#include <pthread.h>
#include <time.h>

extern classlinker_class_t java_lang_Object;


static jvm_error_t system_gc(jvm_frame_t* frame);
static jvm_error_t system_exit(jvm_frame_t* frame);
static jvm_error_t system_arraycopy(jvm_frame_t* frame);
static jvm_error_t system_clinit(jvm_frame_t* frame);
static jvm_error_t currentTimeMillis(jvm_frame_t* frame);
static jvm_error_t indentityHashCode(jvm_frame_t* frame);


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

    .methods_count = 6,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .fn = system_clinit,
            .flags = ACC_STATIC | ACC_NATIVE,

        },
        {
            .name = "exit",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .fn = system_exit,
            .flags = ACC_STATIC | ACC_NATIVE,

        },
        {
            .name = "gc",
            .raw_description = "()V",
            .fn = system_gc,
            .flags = ACC_STATIC | ACC_NATIVE,
        },
        {
            .name = "currentTimeMillis",
            .raw_description = "()J",
            .fn = currentTimeMillis,
            .flags = ACC_STATIC | ACC_NATIVE,
        },
        {
            .name = "indentityHashCode",
            .raw_description = "()I",
            .frame_descriptor.arguments_count = 1,
            .fn = indentityHashCode,
            .flags = ACC_STATIC | ACC_NATIVE,
        },
        {
            .name = "arraycopy",
            .raw_description = "(Ljava/lang/Object;ILjava/lang/Object;II)V",
            .frame_descriptor.arguments_count = 5,
            .fn = system_arraycopy,
            .flags = ACC_STATIC | ACC_NATIVE,
        }
    }
};

classlinker_class_t java_lang_System = {
    .this_name = "java/lang/System",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &java_lang_System_info,
};


//насильно вызывает сборщик мусора попытаться отчистить память(может не получиться) 
static jvm_error_t system_gc(jvm_frame_t* frame) {
    objectmanager_gc(frame->jvm, 0);
    return JVM_OK;
}

static jvm_error_t currentTimeMillis(jvm_frame_t* frame){
    struct timespec t;
    clock_gettime(CLOCK_REALTIME,&t);
    int64_t milliseconds = (int64_t)t.tv_sec * 1000 + (int64_t)t.tv_nsec / 1000000;

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(milliseconds));
    return JVM_OK;
}

static jvm_error_t indentityHashCode(jvm_frame_t* frame){
    objectmanager_object_t* to_hash = JVM_TO_C_VALUE(frame->locals[0],typeof(to_hash));
    uint32_t hash = objectmanager_hash(to_hash);

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(hash));
    return JVM_OK;
}

static jvm_error_t system_arraycopy(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    bool do_throw = false;
    char* throw_what = NULL;

    objectmanager_object_t* array_obj_src = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    uint32_t src_offset = JVM_TO_C_VALUE(frame->locals[1],uint32_t);

    objectmanager_object_t* array_obj_dst = JVM_TO_C_VALUE(frame->locals[2],objectmanager_object_t*);
    uint32_t dst_offset = JVM_TO_C_VALUE(frame->locals[3],uint32_t);
    uint32_t length = JVM_TO_C_VALUE(frame->locals[4],uint32_t);


    FAIL_SET_JUMP(array_obj_src && array_obj_dst, do_throw,({throw_what = "java/lang/NullPointerException";(true);}),exit);

    objectmanager_array_object_t* array_src = objectmanager_get_array_object_info(array_obj_src);
    objectmanager_array_object_t* array_dst = objectmanager_get_array_object_info(array_obj_dst);

    FAIL_SET_JUMP(array_src && array_dst, do_throw,({throw_what = "java/lang/ArrayStoreException";(true);}),exit);
    FAIL_SET_JUMP(array_src->count - src_offset > length,do_throw,({throw_what = "java/lang/IndexOutOfBoundsException";(true);}),exit);
    FAIL_SET_JUMP(array_dst->count - dst_offset > length,do_throw,({throw_what = "java/lang/IndexOutOfBoundsException";(true);}),exit);

    for(unsigned i = 0; i < length; i++){
        array_dst->elements[i + dst_offset] = array_src->elements[i + src_offset];
    }

exit:
    if(do_throw){
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, throw_what));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            err = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            err = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(err == JVM_OK){
                err = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
    }

    return err;
}

//просто ломаем программу ошибочным ретурном
static jvm_error_t system_exit(jvm_frame_t* frame) {
    return JVM_SYSTEM_EXIT;
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

    classlinker_method_t* console_init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(console_stream),"<init>", "(I)V");
    classlinker_method_t* outerr_init = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(out_stream),"<init>", "(Ljava/io/OutputStream;)V");
 
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