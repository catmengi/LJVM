#pragma once
#include <stdint.h>
#include "jeex.h"

typedef struct{
    JEEXMethod_t* method;
    JEEXClass_t* class;

    uint8_t* pc;
    uint16_t sp;
    
    uint32_t* locals;
    uint32_t* stack;
}VMFrame_t;

typedef struct{
    uint16_t cstack_size;

    uint16_t csp;
    VMFrame_t* frames;
}VMCallStack_t;