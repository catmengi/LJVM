#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct{
    unsigned size;
    unsigned sp;
    uint32_t* stack;
}JStack_t;

typedef struct{
    unsigned size;
    uint32_t* locals;
}JLocals_t;

typedef struct JFrame_t JFrame_t;
typedef struct JFrame_t{
    JFrame_t* prev; //It generaly should be only interpreter frame, because NI wont allow calling java from native

    uint32_t size:31; //Size of full frame, used to unwind bumper
    bool is_native:1;
    void* actual_frame;
}JFrame_t;

typedef struct{
    /*Interpreter only stuff*/
    JStack_t stack;
    JLocals_t locals;

    //TODO: current StackMap frame
    unsigned pc;
    /*======================*/
}JInterpreterFrame_t;

typedef struct{
    /*TODO: handles and other shit*/
    /*=================*/
}JNativeFrame_t;
