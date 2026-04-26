#pragma once
#include "cfg.h"
#include "class.h"
#include "loader.h"
#include "bumper.h"
#include "hashmap.h"
#include "bstable.h"

#define JFID_CLASS 0
#define JFID_METHOD 1
#define JFID_FIELD 2

typedef struct JLinker_t{
    bump_allocator_t* arena;
    JLoader_t* loader;

    hashmap_t class_map;
    struct list_head class_list;
    struct list_head root_list;


    struct{
        union{ //Runtime only flags
            uint16_t flags;
            struct{
                bool is_firstlaunch:1;
            };
        }linker_flags;
        uint32_t max_ID; //Used to give unique IDs to classes, methods and fields
        size_t sfield_curoffset; //use this field as the size
                                 //for allocating static field memory
    }linker_global_data;
}JLinker_t;

typedef struct{
    JRawClass_t* raw_self;
    bstable_t fields;

    bstable_t methods; //It have ALL references to methods
    size_t methods_count[2]; //counter of all methods in class. 0 - instance methods, 1 - staticly linked
    size_t fields_count[2]; //Counter of all fields in class. 0 - instance methods, 1 - static

    size_t ifield_curoffset; //This is the current offset for instance fields. 
                             //It will be use like this: field->offset = ifield_curoffset; ifield_curoffset += (field size);
                             //And this is actually can (and most likely will) be used as class size for GC system
}JLinkerMetadata_t;

typedef struct{
    char* mangled_name; //name@description
    void* fn; //TODO: change to proper function ptr. Or dont if i will use libffi in future
}JENIMethod_t;

typedef struct{
    char* name;

    unsigned methods_count;
    JENIMethod_t* methods;
}JENIClass_t;

int JLinker_init(JLinker_t* linker, JLoader_t* loader, bump_allocator_t* arena);
JError_t JLinker_link(JLinker_t* linker);
JClass_t* JClass_get(JLinker_t* linker, char* class_name);
JMethod_t* JClass_get_method(JClass_t* class, char* method_name);
JField_t* JClass_get_field(JClass_t* class, char* field_name);