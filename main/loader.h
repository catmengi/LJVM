#pragma once
#include "parser.h"

typedef struct{
    char* name;
   
    size_t classfile_len;
    uint8_t* classfile; //Store raw java .class but in programm memory
}BuiltinClassEntry_t;

JRawClass_t* loader_load_class(char* name);
int loader_set_loadpath(char* path);