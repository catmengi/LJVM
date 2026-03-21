#pragma once
#include "bumper.h"
#include "class.h"
#include "opcodes.h"
#include "cfg.h"
#include "hashmap.h"

typedef enum{
    EJSYMT_UNKNOWN, //Unknown at compile time symbol should be lookedup by name by loader (used for native methods)
    EJSYMT_CLASS, 
    EJSYMT_METHOD, //Static method, vtable indices will be insterted into code via movi
}JSymbolType_t;

typedef struct{
    JSymbolType_t type;
    uint32_t index;
    void* value;
}JSymbol_t;

typedef struct{
    bump_allocator_t* arena;
    hashmap_t symmap;
    uint32_t cur_symindex;
}JSymbolTable_t;

typedef struct{
    bump_allocator_t* arena;

    JLinker_t* linker;
    JSymbolTable_t symtab;
}JCompiler_t;