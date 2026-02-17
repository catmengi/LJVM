#include "vm.h"
#include "class.h"
#include "frame.h"
#include "freertos/idf_additions.h"
#include "jerror.h"
#include "linker.h"
#include "preloader.h"
#include "thread.h"

#include <assert.h>

static JError_t cogo_interpreter(JFrame_t* startup_frame){
    JError_t err = JERR_OK;
    JFrame_t* frame = startup_frame;
    FAIL_SET_JUMP(frame,err,JERR_BADPARAM,exit);
    
exit:
    return err;
}

int VM_init(VM_t* vm){
    INIT_LIST_HEAD(&vm->threads);
    vm->controls = (typeof(vm->controls)){0};
    vm->thread_lock = xSemaphoreCreateRecursiveMutex();
    assert(vm->thread_lock);
    assert(bumper_create(&vm->arena,VM_INSTANCE_MEMORY) == 0);
    assert(JLoader_init(&vm->loader,&vm->arena) == 0);
    assert(JLinker_init(&vm->linker,&vm->loader,&vm->arena) == 0);

    JPreloader_load_builtins(&vm->loader);
    JLinker_link(&vm->linker); //Pre link basic objects

    assert(JThread_init(vm,NULL));
    return 0;
}

JError_t VM_startup(VM_t* vm, char* class){
    JError_t err = JERR_OK;

    JClass_t* jclass = JClass_get(&vm->linker, class);
    FAIL_SET_JUMP(jclass,err,JERR_NOTFOUND,exit);

    JMethod_t* jmain = JClass_get_method(jclass, "main@([Ljava/lang/String;)V");
    FAIL_SET_JUMP(jmain,err,JERR_NOTFOUND,exit);
    FAIL_SET_JUMP(!jmain->flags.is_native && jmain->flags.is_staticlinked,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(cogo_interpreter(JThread_push_iframe(jmain)) == JERR_OK,err,JERR_UNKNOWN,exit);

    printf("TODO: invoke interpreter\n");
exit:
    return err;
}

