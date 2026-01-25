#pragma once

#include "class_loader.h"
#include "fht.h"
#include "list.h"


#define DEBUG_LOG printf

typedef struct JClass_t JClass_t;
typedef struct JMethod_t JMethod_t;
typedef struct JField_t JField_t;

typedef enum{
    EJVT_BYTE = 'B',
    EJVT_CHAR = 'C',
    EJVT_DOUBLE = 'D',
    EJVT_FLOAT = 'F',
    EJVT_INT = 'I',
    EJVT_LONG = 'J',
    EJVT_REFERENCE = 'L',
    EJVT_NATIVE = '*',
    EJVT_SHORT = 'S',
    EJVT_BOOL = 'Z',
    EJVT_VOID = 'V',
}JValue_type_t;

typedef enum{
	ACC_PUBLIC = 0x0001,
	ACC_FINAL = 0x0010,
	ACC_SUPER = 0x0020,
	ACC_INTERFACE = 0x0200,
	ACC_ABSTRACT = 0x0400,
	ACC_SYNTHETIC = 0x1000,
	ACC_ANNOTATION = 0x2000,
	ACC_ENUM = 0x4000,
	ACC_PRIVATE = 0x0002,
	ACC_PROTECTED = 0x0004,
	ACC_STATIC = 0x0008,
	ACC_VOLATILE = 0x0040,
	ACC_TRANSIENT = 0x0080,
	ACC_SYNCHRONIZED = 0x0020,
	ACC_BRIDGE = 0x0040,
	ACC_VARARGS = 0x0080,
	ACC_NATIVE = 0x0100,
	ACC_STRICT = 0x08000,
}JFlags_t;

typedef struct JClass_interfaces_t{
    JClass_t** implement;
    unsigned count;
}JClass_interfaces_t;

typedef struct JClass_constant_t{
    void* value;
    classloader_constant_type_t type;
}JClass_constant_t;

typedef struct JClass_constantpool_t{
    JClass_constant_t* constants;
    unsigned count;
}JClass_constantpool_t;

typedef struct JClass_t{
    struct list_head list;

    union{ //Thoose flags are custom
        bool flags;
        struct{
            bool is_array:1;
            bool non_builtin:1;
            bool top_level:1;
        };
    }flags;

    JClass_t* parent;
    JClass_interfaces_t implements;
    char* name;

    struct{
        JClass_constantpool_t constant_pool;

        unsigned fields_size; //Size of all fields in bytes
        fht_t* fields; //Field lookup table
        fht_t* methods; //Method lookup table

        uint8_t* static_fields; //Storage for static fields
    }*info;
    
}JClass_t;

typedef struct JField_t{
    char* mangled_name;
    JClass_t* owner;

    JValue_type_t type;
    union{        //I dont really want to use masking flag system
        bool value;
        struct{
            bool is_static:1;
            bool is_private:1;
        };
    }flags;

    size_t offset; //It will be used for accessing values inside of Objects, since they will be just array of bytes + header
}JField_t;

typedef struct JFrame_t JFrame_t;
typedef int JError_t;

typedef JError_t (*JMethod_fn_t)();
typedef struct JMethod_t{
    char* mangled_name; //name@description
    JValue_type_t return_type; //Required for proper invokation(selects which function will push retval to previous frame's stack)
    JClass_t* owner;

    union{
        bool value;
        struct{
            bool is_static:1;
            bool is_native:1;
            bool is_synchronized:1;
            bool non_builtin:1;
        };
    }flags;

    struct{
        unsigned locals_count;
        unsigned stack_size;

        unsigned arguments_count;
        JValue_type_t* argument_types;
        
    }frame_info;

    JMethod_fn_t method;
    void* userctx;
}JMethod_t;

unsigned JValue_sizeof(JValue_type_t type);