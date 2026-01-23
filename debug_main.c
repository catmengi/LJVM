#include "class.h"
#include "class_loader.h"
#include "linker.h"
#include "thread.h"


int main(){
    classloader_instance_t* instance = classloader_new();
    assert(instance);

    assert(classloader_load_folder(instance, "./debug_app") == CLASSLOADER_OK);

    linker_t linker;
    linker_init(&linker,instance);
    linker_link(&linker);

    JClass_t* some_random_shit = linker_find(&linker,"shitx2");
    assert(some_random_shit);

    uint32_t* a = linker_get_staticfield(linker_find_field(some_random_shit,"a@I",1));
    float* c = linker_get_staticfield(linker_find_field(some_random_shit,"c@F",1));
    double* garbage = linker_get_staticfield(linker_find_field(some_random_shit,"garbage@D",1));

    printf("%p %p %p\n",a,c,garbage);

    JMethod_t* clinit = linker_find_method(some_random_shit,"<clinit>@()V",1);
    
    JVM_t jvm = {0};
    jvm_init(&jvm);

    thread_init(&jvm);
    thread_method_frame_push(clinit);

    clinit->method();
    printf("%d %f %lf\n",*a,*c,*garbage);
}