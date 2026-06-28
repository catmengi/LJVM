#pragma once

#include <stdint.h>

#include "class.h"
#include "bumper.h"
#include "config.h"

typedef struct InterpreterFrame_t InterpreterFrame_t;
typedef struct InterpreterFrame_t{
    size_t size; //required because of arena
    struct list_head held_monitors; //Inside of CallFrame_t because of java semantics for exception unwind

    Method_t* method;
    uint8_t* pc;
    uint32_t sp;
    
    int32_t* stack;
    int32_t* locals;

    uint32_t* shadow_locals;
    uint32_t* shadow_stack;

    InterpreterFrame_t* prev;
}InterpreterFrame_t;

typedef struct{
    //Object_t* exception_passthrough; 
    bump_allocator_t arena; 
    InterpreterFrame_t* frame; //Top frame

    char stackbuf[THREAD_STACK_SIZE]; //Buffer that arena uses
}Interpreter_t;

void interpreter_init();
void interpreter_ctx_init(Interpreter_t* ctx);
InterpreterFrame_t* interpreter_frame_push(Method_t* method);
InterpreterFrame_t* interpreter_frame_pop();
InterpreterFrame_t* interpreter_frame_get();

//Special function to invoke java method on top of current java thread
Error_t interpreter_method_invoke(Method_t* method, int32_t* arguments, void* return_value);
Error_t interpreter_execute();