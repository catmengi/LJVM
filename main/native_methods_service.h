#pragma once

#include "jerror.h"
#include <stdint.h>

typedef Error_t (*NativeMethod_t)(/*TODO*/);

typedef struct{
    char* mangled_name; //name@descriptor
    char* class_name;

    NativeMethod_t method;
}NativeMethodEntry_t;

NativeMethod_t natives_find(char* class_name, char* method_name);