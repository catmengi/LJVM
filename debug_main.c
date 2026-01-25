#include "class.h"
#include "class_loader.h"
#include "linker.h"
#include "object.h"
#include "thread.h"
#include <assert.h>


int main(){
    classloader_instance_t* instance = classloader_new();
    assert(instance);

    assert(classloader_load_folder(instance, "./debug_app") == CLASSLOADER_OK);

    linker_t linker;
    linker_init(&linker,instance);
    linker_link(&linker);

    JClass_t* some_random_shit = class_find(&linker,"shitx2");
    assert(some_random_shit);

    uint32_t* a = class_get_staticfield(class_find_field(some_random_shit,"a@I",1));
    float* c = class_get_staticfield(class_find_field(some_random_shit,"c@F",1));
    double* garbage = class_get_staticfield(class_find_field(some_random_shit,"garbage@D",1));

    printf("%p %p %p\n",a,c,garbage);
    
    JVM_t jvm = {0};
    jvm_init(&jvm, &linker);
    printf("%d %f %lf\n",*a,*c,*garbage);

    thread_method_frame_push(class_find_method(some_random_shit,"<clinit>@()V",1));

    JObject_t* object = object_create(some_random_shit);
    printf("object: %p\n",object);

    thread_frame_stack_push_reference(object);

    jvm.object_heap->gc(jvm.object_heap,0);

    JField_t* ptr_field = class_find_field(object_get_class(object),"ptr@Ljava/lang/Object;",0);
    assert(ptr_field);

    void* ptr_value = object_create(some_random_shit);
    object_set_field(object,ptr_field,ptr_value);
    
    void* got = NULL;
    object_get_field(object,ptr_field,&got);
    assert(got == ptr_value);
}