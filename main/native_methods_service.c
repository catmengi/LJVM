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

#include "native_methods_service.h"
#include "heap.h"
#include "thread.h"

#include <string.h>

extern NativeClass_t java_lang_Object;
extern NativeClass_t java_lang_System;
extern NativeClass_t java_io_NativeOutputStream;
static NativeClass_t* s_natives[] = {
    &java_lang_Object,
    &java_lang_System,
    &java_io_NativeOutputStream,
};

//Requires name in mangled form!
NativeMethod_t natives_find(char* class_name, char* method_name){
    for(unsigned i = 0; i < sizeof(s_natives) / sizeof(s_natives[0]); i++){
        NativeClass_t* class = s_natives[i];
        if(strcmp(class_name, class->name) == 0){
            for(unsigned j = 0; j < class->methods_count; j++){
                NativeMethodDescriptor_t* method = &class->methods[j];
                if(strcmp(method->name, method_name) == 0){
                    return method->method;
                }
            }
        }
    }

    return NULL;
}