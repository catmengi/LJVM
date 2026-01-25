#pragma once
#include "bumper.h"
#include "class.h"
#include "list.h"

typedef struct linker_t{
    struct list_head classes;
    classloader_instance_t* loader;

    bump_allocator_t arena;

    struct{
        unsigned top_level_classes;
    }linker_stats;
}linker_t;

int linker_init(linker_t* linker, classloader_instance_t* loader);
JClass_t* class_find(linker_t* linker, char* name);
JError_t linker_link(linker_t* linker);

JField_t* class_find_field(JClass_t* class, char* name, bool is_static);
void* class_get_staticfield(JField_t* field);
JMethod_t* class_find_method(JClass_t* class, char* mangled_name, bool is_static); //mangledd_name is name+description string in this format: name@description
bool is_classes_compatible(JClass_t* class, JClass_t* compatible_to);