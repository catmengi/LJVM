#pragma once

#include "jerror.h"
#include "thread.h"
#include <stdint.h>

//Either NULL, int32_t or int64_t

typedef struct{
    Error_t err; 
    char value[sizeof(int64_t)]; //Used in case when err == JERR_OK
}NativeMethodReturnValue_t;

typedef NativeMethodReturnValue_t (*NativeMethod_t)(Thread_t* thread, Method_t* self, int32_t* args);

typedef struct{
    char* mangled_name; //name@descriptor
    char* class_name;

    NativeMethod_t method;
}NativeMethodEntry_t;

NativeMethod_t natives_find(char* class_name, char* method_name);