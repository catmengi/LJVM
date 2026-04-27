#pragma once

#include "class.h"
#include "linker.h"
#include "jeex.h"
#include "bumper.h"

typedef struct{
    bump_allocator_t* arena; //Arena where JEX data will be stored
    JEEXHeader_t* jeex; //Required to track all JEEX classes, methods, fields.
    JLinker_t* linker;
}JEEXBuilder_t;

JError_t JEEXBuilder_init(JEEXBuilder_t* builder, JLinker_t* linker ,bump_allocator_t* arena);
JError_t JEEXBuilder_build(JEEXBuilder_t* builder);