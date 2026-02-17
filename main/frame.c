#include "frame.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>

//Should i do SP checking here?
uint32_t JStack_pop32(JStack_t* stack){
    return stack->stack[--stack->sp];
}
uint64_t JStack_pop64(JStack_t* stack){
    uint64_t value = 0;
    memcpy(&value,&stack->stack[stack->sp -= 2],sizeof(uint64_t));

    return value;
}
void JStack_push32(JStack_t* stack, uint32_t value){
    stack->stack[stack->sp++] = value;
}

void JStack_push64(JStack_t* stack, uint64_t value){
    memcpy(&stack->stack[stack->sp],&value,sizeof(uint64_t));
    stack->sp += 2;
}

JStack_t* JFrame_get_stack(JFrame_t* frame){
    assert(frame->is_native == 0);
    JInterpreterFrame_t* iframe = frame->actual_frame;

    return &iframe->stack;
}

JLocals_t* JFrame_get_locals(JFrame_t* frame){
    assert(frame->is_native == 0);
    JInterpreterFrame_t* iframe = frame->actual_frame;

    return &iframe->locals;
}
