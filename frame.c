#include "frame.h"
#include "linker.h"

typedef struct JFrame_t JFrame_t;
typedef struct JLocals_t{
    unsigned size; //Locals size in words (uint32_t)
    uint32_t* locals;
}JLocals_t;

typedef struct JStack_t{
    unsigned size; //Stack size in words (uint32_t)
    unsigned sp;
    uint32_t* stack;
}JStack_t;

typedef struct JFrame_t{
    JFrame_t* prev;
    JMethod_t* method;
    unsigned ip;

    JStack_t stack;
    JLocals_t locals;
    unsigned frame_size; //Full size of frame in bytes, including stack and locals
}JFrame_t;

typedef struct{
    JFrame_t* top_frame;

}JThread_t;