#pragma once

#include "cfg.h"
#include "jeex.h"
#include "jeex_builder.h"

typedef struct{
    uint16_t size;
    uint16_t start_pc; //PC when this CFG block starts
    uint16_t last_opcode; //Index of last generated opcode start. Need for jump patching. (same as in CFG module)
}JEEXCompilerCodeBlock_t;

typedef struct{
    JMethod_t* origin_method;
    JClass_t* origin_class;
    JEEXBuilder_t* builder;
    uint8_t* code;
    uint32_t code_length;
    uint32_t cur_offset;
}JEEXCompilerMethodInfo_t;

/*
    Compiler will work by:
        1.1 - iterating every class by their hierarchi from root classes.
        1.2 - iterating class' methods
            2.1 - iterating linker's method CFG.
            2.2 - get JEEX method by linker's method ID
            2.3 - if block is not generated yet, start generating
                - map every blocks java opcodes to JEEX opcodes. (without resolving jump offsets)

        ** steps 1.1 - 2.2 (including) **
            3.1 Get last opcode, if its a jump, insert proper jump offset
*/

JError_t JEEXCompiler_start(JEEXBuilder_t* jeex_builder);