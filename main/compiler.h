#pragma once

#include "jeex_builder.h"

typedef struct{
    uint16_t size;
    uint16_t offset; //Offset from method start. (parent block sizes sum)

    uint16_t last_opcode; //Index of last generated opcode start. Need for jump patching. (same as in CFG module)
    uint8_t* code;
}JEEXCompilerCodeBlock_t;

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