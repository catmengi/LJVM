#pragma once

#include "IL.h"
#include <stdint.h>

#ifndef FAIL_SET_JUMP
    #define __FSJ_DO_BREAK__

    #ifdef __FSJ_DO_BREAK__
        static void FSJ_BREAK(){}
        #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %d\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); FSJ_BREAK(); goto label;}}
    #else
        #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); goto label;}}
    #endif
#endif

typedef struct{
    uint32_t return_to; //Like on xtensa with a0 register.
    uint32_t pc; //Program counter, obviously
    uint32_t regs[16];
}JILFrame_t;


//Per thread JIL execution engine.
#define JIL_CALLSTACK_SIZE 512
typedef struct{
    unsigned sp;
    JILFrame_t* entries; //PSRAM allocated
}JILCallStack_t;