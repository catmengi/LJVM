#pragma once

#include <stdint.h>

#include "bumper.h"
#include "config.h"
#include "list.h"
#include "monitor.h"

typedef struct InterpreterFrame_t InterpreterFrame_t;
typedef struct Method_t Method_t;
typedef struct Class_t Class_t;
typedef struct Thread_t Thread_t;
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
    Thread_t* thread;

    bump_allocator_t arena; 
    InterpreterFrame_t* frame; //Top frame
    int32_t frame_count;

    char stackbuf[THREAD_STACK_SIZE]; //Buffer that arena uses
}Interpreter_t;

void interpreter_init();
Interpreter_t* interpreter_ctx_init(Thread_t* thread, Interpreter_t* ctx);
InterpreterFrame_t* interpreter_frame_push(Interpreter_t* ctx, Method_t* method);
InterpreterFrame_t* interpreter_frame_pop(Interpreter_t* ctx);
InterpreterFrame_t* interpreter_frame_get(Interpreter_t* ctx);

//Special function to invoke java method on top of current java thread
Error_t interpreter_method_invoke(Interpreter_t* ctx, Method_t* method, int32_t* arguments, void* return_value);
Error_t interpreter_execute(Interpreter_t* ctx);