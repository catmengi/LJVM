#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "bumper.h"
#include "loader.h"
#include "class.h"

//I plan to implement simple compiler for xtensa lx7 (esp32s3)
//I dont think this project will be run on any other platform
//So only AOT will exist...... Maybe.... If i dont rage remove it and make interpreter

typedef struct JCFGBlock_t JCFGBlock_t;
typedef enum{
    EJCFGBT_CODE, //0 to 1 children. If have children, then it is basically a fallthrough. (Used to implement labels)
                  //Because goto/jsr targets SHOULD BE start of a block(to make compiler simpler)
    EJCFGBT_JSR, //children[0] -> label with opcodes right after opcode, children[1] -> label with opcodes at offset
    EJCFGBT_GOTO, //children[0] -> goto label
    EJCFGBT_IF, //children[0] -> fallthrough, children[1] -> jump
    EJCFGBT_END, //This block just ended, dont try to link something after it
}JCFGBlockType_t;

typedef struct JCFGBlock_t JCFGBlock_t;
typedef struct JCFGBlock_t{
    unsigned children_count;
    JCFGBlock_t** children;

    JCFGBlockType_t type;
    uint32_t start_pc, end_pc;

    union{
        uint8_t flags;
        struct{
            unsigned generated:1;
        };
    }flags;
}JCFGBlock_t;

typedef struct JCFGException_t{
    JCFGBlock_t* handler; //CFG handler block of the interrupt
    uint32_t start_pc, end_pc; //PC of block that triggers the exception
    uint16_t catch_type; 
}JCFGException_t;

typedef struct{
    bump_allocator_t* arena;
    JCFGBlock_t* root;
    
    unsigned labels_count;
    unsigned block_count;
    uint32_t* labels;
    JCFGBlock_t** blocks; //Same size as labels_count

    unsigned exceptions_count;
    JCFGException_t* exceptions;

    JCodeAttribute_t* bytecode;
}JCFG_t;

JError_t JCFG_init(JCFG_t* cfg, JCodeAttribute_t* bytecode, bump_allocator_t* arena);
JError_t JCFG_build(JCFG_t* cfg);