#include "jvm.h"
#include "class_loader.h"
#include "class_linker.h"

static jvm_error_t debug_segfault(jvm_frame_t* frame){
    *(int*)1 = 0;
}

int main(){

    classloader_instance_t* loader = classloader_new();
    classloader_load_folder(loader,"./class_preload");
    classloader_load_folder(loader,"./debug_app/");


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
    classloader_destroy(loader);

    jvm_instance_t* jvm = jvm_new(linker, 2 * 1024 * 1024);

    printf("jvm exit code: %d\n",jvm_launch_class(jvm,"test_app",1,(char*[]){"Hello world!\n"}));
    return 0;
}