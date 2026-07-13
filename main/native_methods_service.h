/*
JEspressoVM - project to bring java bytecode execution to esp32 (and others)

Copyright (C) 2026  Vladislav Potrashkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "interpreter.h"
#include "jerror.h"
#include "thread.h"
#include <stdint.h>

//Either NULL, int32_t or int64_t

typedef struct{
    Error_t err; 
    char value[sizeof(int64_t)]; //Used in case when err == JERR_OK / JERR_EXCEPTION
}NativeMethodReturnValue_t;

typedef NativeMethodReturnValue_t (*NativeMethod_t)(Interpreter_t* ctx, Method_t* self, int32_t* args);

typedef struct{
    char* name; //In mangled form: name@()V for example 
    NativeMethod_t method;
}NativeMethodDescriptor_t;


//THIS IS NOT A REPLACEMENT FOR JAVA CLASS
//THIS STRUCTURE IS ONLY USED FOR ORGANISATION OF NATIVE METHODS
typedef struct{
    char* name;
    
    size_t methods_count;
    NativeMethodDescriptor_t* methods;
}NativeClass_t;

NativeMethod_t natives_find(char* class_name, char* method_name);