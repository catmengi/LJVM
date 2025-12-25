#pragma once

#include "class_loader.h"

#include "list.h"
#include "os_support.h"

typedef enum{
    CLASSLINKER_UNKNOWN = -1,
    CLASSLINKER_OK = 0,
    CLASSLINKER_OOM,
    CLASSLINKER_UNRESOLVED,
    CLASSLINKER_INVALID_CLASS,
    CLASSLINKER_NOTFOUND,
    CLASSLINKER_UNSUPPORTED,
}classlinker_error_t;

typedef struct classlinker_jni_list_t classlinker_jni_list_t;
typedef struct{
    Arena* arena;

    struct list_head loaded_classes;
    classlinker_jni_list_t* jni_list;
}classlinker_instance_t;


typedef enum{
    EClass,
    Earray,
}classlinker_classtype_t;

typedef struct classlinker_class_t classlinker_class_t;
typedef struct classlinker_class_t{
    struct list_head list;
    pthread_mutex_t monitor;

    classloader_class_t* raw_class; //Temporary pointer, used only while linking

    classlinker_class_t* parent;
    classlinker_classtype_t type;

    unsigned implements_count;
    classlinker_class_t** implements; //implements theese interfaces

    unsigned generation; //will be used to access fields in objects
    char* this_name;
    char* parent_name; //temporary pointer, used only while linking

    void* info; //classlinker_normalclass_t* for Class or NULL for Array class
}classlinker_class_t;

typedef enum{
    ATTRIBUTE_UNKNOWN,
    ATTRIBUTE_CONSTVALUE,
    ATTRIBUTE_CODE,
}classlinker_attributetype_t;

typedef struct{
    classlinker_attributetype_t type;
    void* info;
}classlinker_attribute_t;

typedef struct{
    uint16_t start_pc;
    uint16_t end_pc;
    uint16_t handler_pc;
    
    classlinker_class_t* exception_class; //NULL if any
}classlinker_exceptiontable_t;

typedef struct{    
    uint16_t stack_size;
    uint16_t locals_count;

    uint32_t code_length;
    uint8_t* code;

    uint16_t exceptiontable_size;
    classlinker_exceptiontable_t* exception_table;
}classlinker_bytecode_t;

typedef struct{
    unsigned arguments_count;
    char** args; //String array

    char return_type;  //Letter here is java's return type
}classlinker_methoddescription_t;

typedef struct{
    uint16_t stack_size;
    uint16_t locals_count;
    uint16_t arguments_count;
}classlinker_frame_descriptor_t;

#include "jvm_method.h"
typedef struct classlinker_method_t{
    classlinker_class_t* class;
    bool not_builtin;

    class_access_flags_t flags;
    char* name;

    classlinker_methoddescription_t description;
    char* raw_description;

    classlinker_frame_descriptor_t frame_descriptor;
    void* userctx;
    jvm_method_t fn;
}classlinker_method_t;

#include "jvm.h"
typedef struct{
    class_access_flags_t flags;
    jvm_value_t value;
    char* name;
}classlinker_field_t;

typedef struct{
    char* name;
    char* descriptor; 
}classlinker_nameandtype_t;

typedef struct{
    classlinker_class_t* class;
    classlinker_nameandtype_t nameandtype;
}classlinker_fmimref_t; //field, method, interface method reference

typedef struct{
    uint8_t kind;
    void* reference;
}classlinker_methodhandle_t;

typedef struct{
    classloader_constant_type_t constant_type;
    void* constant_value;
}classlinker_constant_t;

typedef struct{
    classlinker_constant_t* constants;
    uint16_t constants_count;
}classlinker_constantpool_t;

typedef struct{
    classlinker_constantpool_t constant_pool;

    size_t fields_count, static_fields_count;
    classlinker_field_t* fields;
    classlinker_field_t* static_fields;

    uint16_t methods_count;
    classlinker_method_t* methods;
}classlinker_normalclass_t;

typedef struct{
    classlinker_method_t* method;
    char* class_name;
}classlinker_jni_t;

typedef struct classlinker_jni_list_t{
    unsigned fn_count;
    classlinker_jni_t* fns;
}classlinker_jni_list_t;

classlinker_instance_t* classlinker_new();
classlinker_error_t classlinker_link(classlinker_instance_t* linker, classloader_instance_t* loader);
classlinker_class_t* classlinker_find_class(classlinker_instance_t* linker, char* name);

classlinker_method_t* classlinker_find_method(classlinker_class_t* class, char* name, char* description);
classlinker_field_t* classlinker_find_staticfield(classlinker_class_t* class, char* field_name);

classlinker_class_t* classlinker_find_method_class(classlinker_class_t* class, char* name, char* description);

bool classlinker_is_classes_compatible(classlinker_class_t* class, classlinker_class_t* compatible_to);
