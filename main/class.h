#pragma once
#include "cfg.h"
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
    uint32_t ID;

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
    uint8_t arguments_count;
}JMethodPrototype_t;

#include "cfg.h"
typedef struct JMethod_t{
    uint32_t ID;

    char* name; //This name is not java's raw name, it is name@description
    uint16_t vtable_index; //valid when is_static == 0

    JClass_t* owner;
    JMethodPrototype_t prototype;

    union{
        uint32_t flags;
        struct{
            bool is_set:1; //This is for internal linker purposes
            bool is_static:1;
            bool is_native:1;
            bool is_frominterface:1;
            bool is_final:1;
        };
    }flags;

    //Thoose fields are only valid when method is not native!
    JCodeAttribute_t* code;
    JCFG_t cfg; 
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
    EJRCT_STRING, //JRawUTF8_t*
    EJRCT_INT, //uint32_t*
    EJRCT_LONG, //uint64_t*
    EJRCT_FLOAT, //float*
    EJRCT_DOUBLE, //double*
    EJRCT_CLASS, //JClass_t*
    EJRCT_FIELD, //JField_t*
    EJRCT_METHOD, //JMethod_t*
    EJRCT_INTERFACEMETHODREF, //JMethod_t*
};

typedef struct{
    uint16_t class_index; //Class index is index from raw class file!
    uint8_t symbol_type;
    void* value;
}JClassSymbol_t;

typedef struct{
    uint16_t length;
    JClassSymbol_t* symbols;
}JClassSymtab_t;

typedef struct JClass_t{
    uint32_t ID;

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
        uint16_t count;
        JClass_t** implement;
    }interfaces;

    JClassSymtab_t symtab; //Constant pool but eats less memory!
    JMethodTable_t vtable; //Vtable for instance methods
    size_t ifields_size; //Ammount of memory required for ifields of this object

    void* metadata;
}JClass_t;

//Class index is index from raw class file!
JClassSymbol_t* JClassSymtab_get_symbol(JClassSymtab_t* symtab, uint16_t class_index);

//Return index of symbol in symbol table
unsigned JClassSymtab_indexof(JClassSymtab_t* symtab, JClassSymbol_t* symbol);