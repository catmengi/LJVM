#pragma once

#include <stdint.h>
#include "class.h"


typedef struct{
    size_t size; //Data + Object_t header
    Class_t* class;

    bool type; //0 - object, 1 - array
    void* data; //int32_t* for objects, Array_t* for array
}Object_t;