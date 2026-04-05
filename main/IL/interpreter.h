#pragma once

#include "opcode.h"
#include "encode.h"

#include "esp_heap_caps.h"

typedef struct{
    uint32_t pc;
    int32_t regs[16];
}JILCallFrame_t;

#define JIL_CALLSTACK_FRAMES 256
#define JIL_SYSCALL_COUNT 256

#define JIL_ALLOC_CALLSTACK_ENTRIES(count) heap_caps_calloc((count), sizeof(JILCallFrame_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)

typedef struct{
    unsigned sp;
    unsigned size;
    JILCallFrame_t* call_stack;
}JILCallStack_t;

typedef struct{
    uint32_t* code;
    uint32_t (*syscall[JIL_SYSCALL_COUNT])(uint32_t param);

    JILCallStack_t call_stack;
}JILInterpreter_t;