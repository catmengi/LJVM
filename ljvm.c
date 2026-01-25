#include "ljvm.h"
#include "class.h"
#include "linker.h"
#include "object.h"
#include "thread.h"

JError_t jvm_invoke(JMethod_t* method){
    
}

JError_t jvm_init(JVM_t* jvm, linker_t* linker){
    INIT_LIST_HEAD(&jvm->thread_list);
    jvm->object_heap = object_heap_create();
    assert(jvm->object_heap); //we are DEEPLY fucked

    jvm->linker = linker;

    thread_init(jvm);

    JClass_t* cur_class = NULL;
    list_for_each_entry(cur_class,&linker->classes, list){
        JMethod_t* method = class_find_method(cur_class,"<clinit>@()V",1);
        if(method){
            thread_method_frame_push(method);
            method->method();
            thread_method_frame_pop();
        }
    }

    return EJERR_OK;
}