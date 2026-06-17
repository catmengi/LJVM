#pragma once

#include <stdint.h>
#include "class.h"

typedef struct{
    uint32_t forward_ptr;
    Class_t* class;
}Object_t;

typedef struct{
    size_t count;
    JavaValueType_t type;

    void* array;
}ArrayObject_t;

//Object_t header
//==================
//(int32_t fields[class->fields_count[1]] || ArrayObject_t header)
//==========================================
//int(32/64)_t element[array->count];