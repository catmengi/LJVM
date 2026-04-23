#pragma once

#include "class.h"
#include "linker.h"
#include "jeex.h"
#include "bumper.h"

typedef struct{
    bump_allocator_t* output_arena; //Arena where JEX data will be stored
    JEEXHeader_t* jeex_header; //Required to track all JEEX classes, methods, fields.
    JLinker_t* linker;
}JEEXBuilder_t;

JError_t JEEX_create_builder(JEEXBuilder_t* builder, JLinker_t* linker ,bump_allocator_t* arena);
JError_t JEEX_build(JEEXBuilder_t* builder);