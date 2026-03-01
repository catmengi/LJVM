#pragma once
#include "loader.h"

typedef enum{
    EJVT_BYTE = 'B',
    EJVT_CHAR = 'C',
    EJVT_DOUBLE = 'D',
    EJVT_FLOAT = 'F',
    EJVT_INT = 'I',
    EJVT_LONG = 'J',
    EJVT_REFERENCE = 'L',
    EJVT_SHORT = 'S',
    EJVT_BOOL = 'Z',
    EJVT_VOID = 'V',
}JValueType_t;
typedef struct JClass_t JClass_t;

typedef struct{
    JClass_t* should_implement;
    char* name;
}JInterfaceMethodRef_t; //I think i would implement default methods in my JVM.... But i dont think someone
                        //will be able to use them

typedef struct{
    char* name; //This name is not java's raw name, it is name@description
    JClass_t* owner;

    union{
        uint32_t flags;
        struct{
            bool is_static:1;
            bool is_alligned:1; //I think this would be used in future to decide should we enter or not critical section
            bool is_unitialised:1; //This flags will be only set if: field is static, field have constantValue attribute
                                   //If this flags set, getstatic should initialise memory leading to this field with value from constvalue
                                   //Based on fields type
        };
    }flags;
    void* constvalue; //Point somewhere meaningfull only if is_unitialised == 1

    JValueType_t type;
    unsigned offset;
}JField_t;

typedef struct{
    JValueType_t return_type;
    JValueType_t* argument_types;
    unsigned arguments_count;
}JMethodPrototype_t;
typedef struct{
    char* name; //This name is not java's raw name, it is name@description
    uint16_t vtable_index;

    JClass_t* owner;
    JMethodPrototype_t prototype;

    union{
        uint32_t flags;
        struct{
            bool is_staticlinked:1; //This is for linker internal purposes
            bool is_set:1; //This is for internal linker purposes
            bool is_static:1;
            bool is_native:1;
            bool is_frominterface:1;
            bool is_final:1;
        };
    }flags;

    void* method_info; //Method info, either JCompiledCode_t or native method stuff
}JMethod_t;

typedef struct{
    unsigned count;
    JMethod_t** methods;
}JMethodTable_t;

typedef struct{
    unsigned count;
    JField_t** fields;
}JFieldTable_t;

typedef struct JLinker_t JLinker_t;

enum{
    EJRCT_NULL, //nothinh
    EJRCT_STRING, //TODO
    EJRCT_INT, //uint32_t*
    EJRCT_LONG, //uint64_t*
    EJRCT_FLOAT, //float*
    EJRCT_DOUBLE, //double*
    EJRCT_CLASS, //JClass_t*
    EJRCT_FIELD, //JField_t*
    EJRCT_METHOD, //JMethod_t*
    EJRCT_METHODREF, //uint16_t*
    EJRCT_INTERFACEMETHODREF, //JInterfaceMethodRef_t;
};

typedef struct JClass_t{
    struct list_head list; //Used to store class in class_list of linker.
    struct list_head as_child; //Used to store class in child_list of parent. 
    struct list_head children; //Used to store subclasses in it.

    char* name;
    JClass_t* parent;
    JLinker_t* linker;

    union{
        uint32_t flags;
        struct{
            bool is_final:1;
            bool is_initialised:1;
        };
    }flags;
    
    struct{
        unsigned count;
        JClass_t** implement;
    }interfaces;

    struct{
        unsigned size;
        struct{
            void* value;
            uint8_t type;
        }*constants;
    }constantpool;

    JMethodTable_t vtable; //Vtable for instance methods
    JFieldTable_t fields[2]; //0: instance fields, 1: static fields. Will be used for GC scanning
    size_t ifields_size; //Ammount of memory required for ifields of this object
    //TODO: stackmap field if not native

    void* metadata;
}JClass_t;