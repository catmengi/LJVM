#include "native_methods_service.h"

#include <string.h>

static NativeMethodReturnValue_t debug_native(Thread_t* thread,Method_t* self, int32_t* params){
    NativeMethodReturnValue_t retval = {.err = JERR_OK};
    *(int32_t*)retval.value = params[0] + 488;

    return retval;
}

static NativeMethodReturnValue_t debug_print(Thread_t* thread, Method_t* self, int32_t* params){
    printf("debug: %ld\n", params[0]);
    return (NativeMethodReturnValue_t){.err = JERR_OK, .value = {0}};
}

static NativeMethodEntry_t* s_native_methods[] = {&(NativeMethodEntry_t){"debug_native@(I)I", "dummy", debug_native}, &(NativeMethodEntry_t){"debug_print@(I)V", "dummy", debug_print}};


//Requires name in mangled form!
NativeMethod_t natives_find(char* class_name, char* method_name){
    for(unsigned i = 0; i < sizeof(s_native_methods) / sizeof(s_native_methods[0]); i++){
        NativeMethodEntry_t* entry = s_native_methods[i];
        if(strcmp(entry->class_name, class_name) == 0 && strcmp(entry->mangled_name, method_name) == 0)
            return entry->method;
    }

    return NULL;
}