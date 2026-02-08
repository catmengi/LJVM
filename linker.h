#pragma once
#include "class.h"
#include "loader.h"
#include "bumper.h"
#include "hashmap.h"

typedef struct{
    bump_allocator_t* arena;
    JLoader_t* loader;

    hashmap_t class_map;
    struct list_head class_list;

    struct{
        size_t sfield_curoffset;
        uint8_t* sfield_memory;
    }linker_global_data;
}JLinker_t;

typedef struct{
    JRawClass_t* raw_self;
    hashmap_t fields;
    hashmap_t all_methods; //It have ALL references to methods

    size_t ifield_curoffset; //This is the current offset for instance fields. 
                             //It will be use like this: field->offset = ifield_curoffset; ifield_curoffset += (field size);
                             //And this is actually can (and most likely will) be used as class size for GC system
    size_t ifield_count;
    size_t sfield_count;
}JLinkerMetadata_t;

typedef struct{
    char* mangled_name; //name@description
    void* fn; //TODO: change to proper function ptr
}JENIMethod_t;

typedef struct{
    char* name;

    unsigned methods_count;
    JENIMethod_t* methods;
}JENIClass_t;

int JLinker_init(JLinker_t* linker, JLoader_t* loader, bump_allocator_t* arena);
JError_t JLinker_link(JLinker_t* linker);