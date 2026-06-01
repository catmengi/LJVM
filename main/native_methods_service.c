#include "native_methods_service.h"
#include <string.h>

static NativeMethodEntry_t s_native_methods[] = {};


//Requires name in mangled form!
NativeMethod_t natives_find(char* class_name, char* method_name){
    for(unsigned i = 0; i < sizeof(s_native_methods) / sizeof(s_native_methods[0]); i++){
        NativeMethodEntry_t* entry = &s_native_methods[i];
        if(strcmp(entry->class_name, class_name) == 0 && strcmp(entry->mangled_name, method_name) == 0)
            return entry->method;
    }

    return NULL;
}