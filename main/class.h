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

#include <stdint.h>
#include <stdbool.h>
#include "list.h"
#include "jerror.h"
#include "monitor.h"
#include "interpreter.h"

#define MAX_LOADED_CLASSES 1024
typedef struct Class_t Class_t;

typedef enum{
    TYPE_BYTE = 'B',
    TYPE_CHAR = 'C',
    TYPE_DOUBLE = 'D',
    TYPE_FLOAT = 'F',
    TYPE_INT = 'I',
    TYPE_LONG = 'J',
    TYPE_SHORT = 'S',
    TYPE_BOOL = 'Z',
    TYPE_VOID = 'V',
    TYPE_REFERENCE = 'L',
}JavaValueType_t;

typedef enum{
    SYMBOL_NONE,

    SYMBOL_CLASS = 1,
    SYMBOL_STRING,
    SYMBOL_INT,
    SYMBOL_FLOAT,
    SYMBOL_LONG,
    SYMBOL_DOUBLE,
    SYMBOL_METHOD,
    SYMBOL_FIELD,

    //Proxy symbol are to be resolved in runtime
    PROXY_SYMBOL_CLASS = 10,
    PROXY_SYMBOL_STRING,
    PROXY_SYMBOL_METHOD,
    PROXY_SYMBOL_FIELD,
    //================================================

}SymbolType_t;

typedef struct{
    struct list_head list; //Since this probably gonna be inside temporary arena, we might use a list
    unsigned cp_index;
    unsigned symtab_index;
}ConstantPoolPatchSymbol_t;

typedef struct{
    atomic_flag spinlock; 
    SymbolType_t type;
    void* value;
}ClassSymbol_t;

typedef struct{
    uint16_t origin_name_id;
    uint16_t self_name_id;
}ClassProxySymbol_t;

typedef struct{
    size_t count;
    ClassSymbol_t* symbols;
}ClassSymbolTable_t;

typedef struct Field_t{
    uint16_t name_id;
    JavaValueType_t type; //Still store type separately for faster opcodes on resolved fields
    size_t offset; //offset in bytes
    size_t size; //size in bytes
    
    Class_t* class;

    struct{
        union{
            uint32_t flags;
            struct{
                unsigned is_static:1;
                unsigned is_public:1;
                unsigned is_protected:1;
                unsigned is_private:1;
                unsigned is_volatile:1;
            };
        };
    }flags;     

    ClassSymbol_t* constantvalue; //If non NULL getstatic / putstatic must initialise field with that value then set to NULL! Will lead to ClassSymbol_t* 
                         //So if constantpool entry was resolved, and this field wasnt initalised yet, we can use resolved value and vise-versa
}Field_t;

typedef struct Method_t{
    uint16_t name_id;
    uint16_t vtable_index;
    uint16_t interface_index;

    Class_t* class;
    
    struct{
        union{
            uint32_t flags;
            struct{
                unsigned is_native:1;
                unsigned is_static:1;
                unsigned is_special:1;
                unsigned is_virtual:1; //!(is_static || is_special)
                unsigned is_interface:1;
                unsigned is_syncronized:1;
                unsigned is_public:1;
                unsigned is_protected:1;
                unsigned is_private:1;
                unsigned is_abstract:1;
            };
        };
    }flags; 

    JavaValueType_t return_type;

    unsigned args_slots; //SP offset
    unsigned args_bitmap_size; //GC bitmap
    uint32_t* args_bitmap;

    void* code;
}Method_t;

typedef struct{
    uint16_t start_pc;
    uint16_t end_pc;

    uint16_t handler_pc;
    ClassSymbol_t* type;
}MethodExceptionHandler_t;

typedef struct{
    size_t count;
    Field_t* fields;
}FieldTable_t;

typedef struct{
    uint8_t type;
    uint16_t ctx; //addition info for some types
}BytecodeVerifierVariable_t;

typedef struct{
    uint16_t pc;
    uint16_t locals_count;
    uint16_t stack_size;

    BytecodeVerifierVariable_t* locals;
    BytecodeVerifierVariable_t* stack;
}BytecodeVerifierFrame_t;

typedef struct{
    uint16_t frame_count;
    BytecodeVerifierFrame_t* frames;
}BytecodeVerifierInfo_t;

typedef struct{
    uint16_t max_stack;
    uint16_t max_locals;

    uint32_t code_length;
    uint8_t* code;

    size_t exception_count;
    MethodExceptionHandler_t* exceptions;
    BytecodeVerifierInfo_t* verifier_info;
}MethodBytecode_t;

typedef struct{
    size_t count;
    Method_t* methods;
}MethodTable_t;

typedef struct{
    uint8_t is_root; //Only set to 1 if class have no parent!
    uint16_t parent_name_id;
    
    size_t implements_count;
    uint16_t* implements; //name_ids

    struct list_head cp_patch_list;
}ClassLinkTimeMetadata_t;

typedef struct{
    Class_t* interface;

    size_t methods_count;
    int32_t* vtable_index; //Need for extension override
}Implementation_t;

typedef struct{
    size_t count;
    Implementation_t* implementations;
}ImplementsTable_t;

typedef struct{
    uint16_t interface_name_id; 
    uint16_t itable_index;
}ClassIMethodSymbol_t;

typedef struct Class_t{
    //atomic_flag spinlock;

    //Linker info
    void* metadata;
    struct list_head list[2]; //Required for link-time hierarchy building

    //Class info
    uint16_t name_id;
    Class_t* parent;
    ImplementsTable_t implements;
    ClassSymbolTable_t symtab;
    Object_t* class_object; //java.lang.Class instance

    _Atomic(Thread_t*) clinit_trigger; //Thread that triggered clinit
    atomic_int clinit_stage; //0 - not started, 1 - in progress, 2 - done
    atomic_int link_stage; //TODO: refactor current linker and use it instead of a flag!
                                //0 - not started, 1 - in progress, 2 - done

    struct{
        union{
            uint32_t flags;
            struct{
                unsigned is_linked:1; //TODO: Refactor linker!
                unsigned is_array:1;
                unsigned is_interface:1;
                unsigned is_final:1;
                unsigned is_abstract:1;
                //TODO: other flags
            };
        };
    }flags;
    JavaValueType_t array_type; //Only usable if is_array == 1, other wise TYPE_VOID

    //Fields info
    //Separated to simplify GC scan logic.
    FieldTable_t instance_fields;
    FieldTable_t static_fields;

    size_t object_size; //Size of class data in BYTES (for easier allocation logic)
    void* sfields_storage; //Static fields storage

    //Method info
    //TODO: methods structure
    MethodTable_t methods;

    size_t vtable_size;
    Method_t** vtable;
}Class_t;

void classes_init(); //Not to be called by user!

Error_t class_resolv_symbol(Interpreter_t* ctx, ClassSymbol_t* symbol);
Error_t class_load_bynameid(uint16_t name_id, Class_t** out);

Method_t* class_find_method(Class_t* class, uint16_t name_id);
bool class_is_compatible(Class_t* class, Class_t* compatible_to);
bool class_is_subclass(Class_t* is_subclass, Class_t* to);

Field_t* class_find_static_field(Class_t* class, uint16_t name_id);
Field_t* class_find_instance_field(Class_t* class, uint16_t name_id);