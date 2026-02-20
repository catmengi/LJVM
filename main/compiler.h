#pragma once

#include <stdint.h>
#include "class.h"

//I plan to implement simple compiler for xtensa lx7 (esp32s3)
//I dont think this project will be run on any other platform
//So only AOT will exist...... Maybe.... If i dont rage remove it and make interpreter

typedef struct JCFGBlock_t JCFGBlock_t;
typedef enum{
    EJCFGBT_CODE, //No children
    EJCFGBT_JSR, //children[0] -> block with opcodes right after opcode, children[1] -> block with opcodes at offset
    EJCFGBT_GOTO, //children[0] -> block with opcodes at offset
    EJCFGBT_IF, //children[0] -> fallthrough, children[1] -> jump
}JCFGBlockType_t;

typedef struct JCFGBlock_t{
    unsigned block_id;
    unsigned visit_id; //Id to make bugged lookup loops imposible
    JCFGBlockType_t block_type;

    uint32_t jbpc_start, jbpc_end; //Block start / end in java bytecode
    uint32_t mcpc_start, mcpc_end; //Block start / end in native code
    uint32_t mcsize; //Machine code size


    unsigned children_count;
    JCFGBlock_t** children;
}JCFGBlock_t;

typedef struct{
    unsigned start_pc, end_pc; //Where exception might happen in java bytecode
    unsigned catch_type; //0 for finally?

    JCFGBlock_t* handler; //Block of handler
}JCFGException_t;

typedef struct JMethodCompiler_t{
    unsigned last_block_id;
    unsigned last_visit_id; //Counter for lookup iterations

    JMethod_t* method;
    bump_allocator_t* arena; //Arena for data ONLY
    //TODO: bump_allocator_t* code_arena; //executable memory arena?

    JCFGBlock_t* root; //Main code start

    //TODO: exception parsing
    unsigned exceptions_count;
    JCFGException_t* exceptions;
    //========================


    JCodeAttribute_t* bytecode;
}JMethodCompiler_t;

typedef struct{
    JMethodCompiler_t* compiler; //One per method
    void* code;

}JCompiledMethod_t;

//TODO: proper compiler init function!

JError_t JCFG_generate(JMethodCompiler_t* compiler, JCFGBlock_t** block_output, unsigned start_pc);
void JCFG_test(JCFGBlock_t* root);
