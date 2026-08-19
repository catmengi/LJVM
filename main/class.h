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

typedef struct Class_t Class_t;
typedef struct Interpreter_t Interpreter_t;

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

    SYMBOL_CLASS = 1, //Class_t*
    SYMBOL_STRING, //Object_t*
    SYMBOL_INT, //int32_t*
    SYMBOL_FLOAT, //float*
    SYMBOL_LONG, //int64_t*
    SYMBOL_DOUBLE, //double*
    SYMBOL_METHOD, //Method_t*
    SYMBOL_FIELD, //Field_t*

    //Proxy symbol are to be resolved in runtime. ClassProxySymbol_t*
    PROXY_SYMBOL_CLASS = 10,
    PROXY_SYMBOL_STRING,
    PROXY_SYMBOL_METHOD,
    PROXY_SYMBOL_FIELD,
    //================================================
}SymbolType_t;

typedef struct{
    SymbolType_t type;
    void* value; //Where it points to is decided by "type"
}ClassSymbol_t;

typedef struct{
    uint16_t origin_name_id; //name_id of class where this symbol lies.
    uint16_t self_name_id; //name_id of symbol that lies at origin. INVALID for PROXY_SYMBOL_CLASS. Must be decided what lies by ClassSymbol_t->type
}ClassProxySymbol_t;

typedef struct{
    size_t count;
    ClassSymbol_t* symbols;
}ClassSymbolTable_t;

typedef struct{
    JavaValueType_t type;
    ClassSymbol_t* info; //IF type is TYPE_REFERENCE, then this info must be populated with symbol pointing to class
}ValueType_t;

typedef struct Field_t{
    uint16_t name_id;

    ValueType_t type;
    size_t offset; //offset in bytes
    size_t size; //size in bytes

    Class_t* class; //Owner class of this field
    
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
    uint16_t vtable_index; //Index to class->vtable
    uint16_t interface_index; //Index to Implementation_t->vtable_indices;

    Class_t* class; //Owner class of this method
    
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

                unsigned is_clinit:1; //Required for automatically set Method's class clinit_stage to 2
            };
        };
    }flags; 

    ValueType_t return_type;

    int return_slots; //same as args_slots but for return value
    int args_slots; //Number of uint32_t slots that method require as arguments. INCLUDES this FOR NON STATIC METHODS

    void* code; //Pointer to MethodBytecode_t or to future native method descriptor
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
    uint8_t* code; //Code has already been patched, so every indice / offset in it will be in CPUs endiannes!

    size_t exception_count;
    MethodExceptionHandler_t* exceptions;
    BytecodeVerifierInfo_t* verifier_info; //Just exists, no one uses it now
}MethodBytecode_t;

typedef struct{
    size_t count;
    Method_t* methods;
}MethodTable_t;

typedef struct{
    Class_t* interface;

    size_t count;
    int32_t* vtable_indices; //Using vtable, cause children classes must be able to override implementations
}Implementation_t;

typedef struct{
    size_t count;
    Implementation_t* implementations;
}ImplementsTable_t;

typedef struct Class_t{
    //Linker info
    void* metadata; //Pointer to ClassLinkTimeMetadata_t. SHOULD NOT BE TOUCHED BY ANYONE EXPECT CLASS GENERATING CODE
    struct list_head list[2]; //Required for link-time hierarchy building

    //Class info
    uint16_t name_id;
    Class_t* parent;
    ImplementsTable_t implements; //Uses interface hierarchy flattening, so no complex walk required
    ClassSymbolTable_t symtab;
    //Object_t* jlClass; //java.lang.Class instance

    Thread_t* clinit_trigger; //Thread that triggered clinit
    struct list_head clinit_waiters; //List for syncronization of clinit invokers
    
    int clinit_stage; //0 - not started, 1 - in progress, 2 - done
    struct{
        union{
            uint32_t flags;
            struct{
                unsigned is_linked:1;
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
    FieldTable_t fields;

    size_t object_size; //Size of class data in BYTES (for easier allocation logic)
    void* sfields_storage; //Static fields storage

    //Method info
    MethodTable_t methods;

    size_t vtable_size;
    Method_t** vtable; 
    Method_t* clinit; //Cached for future uses. can be NULL
}Class_t;

void classes_init(); //Not to be called by user!

//Return: class related JERRs or JERR_OK
//name_id: ID of string inside stringpool (stringpool_add(""))
//out: where to store pointer to loaded / found classes
Error_t class_load_bynameid(uint16_t name_id, Class_t** out);

//Return: pointer to Method_t with name == name_id inside Class or NULL
//Class: class where to search method
//name_id: ID of string inside stringpool (stringpool_add(""))
Method_t* class_find_method(Class_t* class, uint16_t name_id);

//Return: is class compatible to assign with compatible_to
bool class_is_compatible(Class_t* class, Class_t* compatible_to);

//Return: is "is_subclass" is subclass of "to"
bool class_is_subclass(Class_t* is_subclass, Class_t* to);

//Return: pointer to Field_t with name == name_id inside Class or NULL
Field_t* class_find_field(Class_t* class, uint16_t name_id);

//Return: error that occured or JERR_OK
//Field: field to read / write. Class will be retrieved from it

//output: pointer to block of memory where to store field contents
Error_t class_read_static_field(Field_t* field, void* output);

//input: pointer to block of memory that must be copied into field
Error_t class_write_static_field(Field_t* field, void* input);
//=======================================