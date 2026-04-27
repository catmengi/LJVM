#pragma once

#include <stdint.h>


typedef struct JEEXClass_t JEEXClass_t;

//copied thoose to make jeex.h separate from other JEspresso headers!

//Value types
enum{
    EJEEXVT_BYTE = 'B',
    EJEEXVT_CHAR = 'C',
    EJEEXVT_DOUBLE = 'D',
    EJEEXVT_FLOAT = 'F',
    EJEEXVT_INT = 'I',
    EJEEXVT_LONG = 'J',
    EJEEXVT_REFERENCE = 'L',
    EJEEXVT_SHORT = 'S',
    EJEEXVT_BOOL = 'Z',
    EJEEXVT_VOID = 'V',
};
//==================================================================

typedef struct{
    uint16_t start_pc;
    uint16_t end_pc;
    uint16_t handler_pc;
    
    JEEXClass_t* type;
}JEEXBytecodeException_t;

typedef struct JEEXMethodBytecode_t{
    uint32_t code_length;
    uint16_t locals_count;
    uint16_t stack_size;

    uint8_t* code;

    uint16_t exception_count;
    JEEXBytecodeException_t* exceptions;
}JEEXMethodBytecode_t;

typedef struct JEEXMethod_t{
    uint32_t ID;
    char* mangled_name;

    union{
        uint8_t flags;
        struct{
            unsigned is_native:1;
            unsigned is_static:1;
        };
    }flags;

    struct{
        uint8_t return_type;
        uint8_t* arguments_types;
        uint8_t arguments_count;
    }prototype;

    union{
        JEEXMethodBytecode_t* bytecode;
        uint32_t native_id; //Used if method native to find native method in the native method table
    }code;
}JEEXMethod_t;

typedef struct{
    uint32_t ID;    

    uint8_t type;
    uint32_t offset;
}JEEXField_t;

typedef struct{
    unsigned count;
    JEEXMethod_t** methods;
}JEEXMethodTable_t;

typedef struct{
    unsigned count;
    JEEXField_t** fields;
}JEEXFieldTable_t;

typedef struct{
    void* value;
    uint8_t type;
}JEEXSymbol_t;

typedef struct{
    uint16_t length;
    uint8_t* string;
}JEEXRawUTF8_t;

typedef struct JEEXClass_t{
    uint32_t ID; //Might be useful to trace linker class from JEEX
    
    char* name; //Still required to find main class or for Class.forName()
    JEEXClass_t* parent;

    JEEXClass_t** children;
    JEEXClass_t** implements;
    unsigned children_count, implements_count;

    JEEXSymbol_t* symtab;
    unsigned symtab_length;
    
    uint32_t object_size; //Size of that class when created as object
    JEEXMethodTable_t vtable;
    JEEXFieldTable_t fields[2]; //1 - static, 0 - instance. Stored because we need GC to scan them
}JEEXClass_t;

enum{
    EJEEXID_CLASS = 0,
    EJEEXID_METHOD = 1,
    EJEEXID_FIELD = 2,
    EJEEXID_32CONST = 3,
    EJEEXID_64CONST = 4,
    EJEEXID_STRING = 5,
};

typedef struct{
    uint8_t type; //EJEEXID elements from enum upwards only!
    void* element;
}JEEXIDElement_t; //JEEX elemented accessed by ID.

typedef struct{
    JEEXIDElement_t* id_table;
    size_t id_table_length;
    size_t static_fields_size; //Size in bytes required to store static fields!
}JEEXHeader_t;