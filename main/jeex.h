#pragma once

#include <stdint.h>



typedef struct JEEXClass_t JEEXClass_t;

//CEJEEXOPyed thoose to make jeex.h separate from other JEspresso headers!
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

enum{
    EJEEXST_NULL,
    EJEEXST_STRING,
    EJEEXST_INT,
    EJEEXST_LONG,
    EJEEXST_FLOAT,
    EJEEXST_DOUBLE,
    EJEEXST_CLASS,
    EJEEXST_FIELD,
    EJEEXST_METHOD,
    EJEEXST_INTERFACEMETHODREF,
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

    uint16_t exception_count;
    JEEXBytecodeException_t* exceptions;

    uint8_t* code;
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


typedef enum {
    // ===== Miscellaneous / NOP =====
    EJEEXOP_NEJEEXOP,

    // ===== Constants =====
    EJEEXOP_PUSHCONST, //4 byte operand
    EJEEXOP_LDC, //2 byte operand

    // ===== Local variable loads =====
    EJEEXOP_LLOAD32, //2 byte operand
    EJEEXOP_LLOAD64, //2 byte operand

    // ===== Array loads =====
    EJEEXOP_ALOAD8, 
    EJEEXOP_ALOAD16,
    EJEEXOP_ALOAD32,
    EJEEXOP_ALOAD64,

    // ===== Local variable stores =====
    EJEEXOP_LSTORE32, //2 byte operand
    EJEEXOP_LSTORE64, //2 byte operand

    // ===== Array stores =====
    EJEEXOP_ASTORE8,
    EJEEXOP_ASTORE16,
    EJEEXOP_ASTORE32,
    EJEEXOP_ASTORE64,

    // ===== Stack manipulation =====
    EJEEXOP_POP,
    EJEEXOP_POP2,
    EJEEXOP_DUP,
    EJEEXOP_DUPx1,
    EJEEXOP_DUPx2,
    EJEEXOP_DUP2,
    EJEEXOP_DUP2x1,
    EJEEXOP_DUP2x2,
    EJEEXOP_SWAP,

    // ===== Arithmetic: Addition =====
    EJEEXOP_IADD32,
    EJEEXOP_IADD64,
    EJEEXOP_FADD32,
    EJEEXOP_FADD64,

    // ===== Arithmetic: Subtraction =====
    EJEEXOP_ISUB32,
    EJEEXOP_ISUB64,
    EJEEXOP_FSUB32,
    EJEEXOP_FSUB64,

    // ===== Arithmetic: Multiplication =====
    EJEEXOP_IMUL32,
    EJEEXOP_IMUL64,
    EJEEXOP_FMUL32,
    EJEEXOP_FMUL64,

    // ===== Arithmetic: Division =====
    EJEEXOP_IDIV32,
    EJEEXOP_IDIV64,
    EJEEXOP_FDIV32,
    EJEEXOP_FDIV64,

    // ===== Arithmetic: Remainder =====
    EJEEXOP_IREM32,
    EJEEXOP_IREM64,
    EJEEXOP_FREM32,
    EJEEXOP_FREM64,

    // ===== Arithmetic: Negation =====
    EJEEXOP_INEG32,
    EJEEXOP_INEG64,
    EJEEXOP_FNEG32,
    EJEEXOP_FNEG64,

    // ===== Bitwise shifts =====
    EJEEXOP_ISHL32,
    EJEEXOP_ISHL64,
    EJEEXOP_ISHR32,
    EJEEXOP_ISHR64,
    EJEEXOP_IUSHR32,
    EJEEXOP_IUSHR64,

    // ===== Bitwise logical =====
    EJEEXOP_IAND32,
    EJEEXOP_IAND64,
    EJEEXOP_IOR32,
    EJEEXOP_IOR64,
    EJEEXOP_IXOR32,
    EJEEXOP_IXOR64,

    // ===== Local variable increment =====
    EJEEXOP_LINC, //2 byte operand + 1 byte operand

    // ===== Type conversions =====
    EJEEXOP_I32toI64,
    EJEEXOP_I32toF32,
    EJEEXOP_I32toF64,
    EJEEXOP_I64toI32,
    EJEEXOP_I64toF32,
    EJEEXOP_I64toF64,
    EJEEXOP_F32toI32,
    EJEEXOP_F32toI64,
    EJEEXOP_F32toF64,
    EJEEXOP_F64toI32,
    EJEEXOP_F64toI64,
    EJEEXOP_F64toF32,

    // ===== Comparisons =====
    EJEEXOP_I32cmpeq,
    EJEEXOP_I32cmplt,
    EJEEXOP_I32cmpgt,
    EJEEXOP_I32cmplteq,
    EJEEXOP_I32cmpgteq,
    
    EJEEXOP_I64cmpeq,
    EJEEXOP_I64cmplt,
    EJEEXOP_I64cmpgt,
    EJEEXOP_I64cmplteq,
    EJEEXOP_I64cmpgteq,
    
    EJEEXOP_F32cmpeq,
    EJEEXOP_F32cmplt,
    EJEEXOP_F32cmpgt,
    EJEEXOP_F32cmplteq,
    EJEEXOP_F32cmpgteq,
    
    EJEEXOP_F64cmpeq,
    EJEEXOP_F64cmplt,
    EJEEXOP_F64cmpgt,
    EJEEXOP_F64cmplteq,
    EJEEXOP_F64cmpgteq,

    EJEEXOP_IFz,
    EJEEXOP_IFnz,
    EJEEXOP_IFgtz,
    EJEEXOP_IFltz,
    EJEEXOP_IFgtez,
    EJEEXOP_IFltez,

    // ===== Control flow =====
    EJEEXOP_GOTO, //4 byte operand
    EJEEXOP_JSR, //4 byte operand
    EJEEXOP_RET,
    EJEEXOP_TABLESWITCH, //TODO:
    EJEEXOP_LOOKUPSWITCH, //TODO:
    EJEEXOP_RETURN32,
    EJEEXOP_RETURN64,
    EJEEXOP_RETURN,
    EJEEXOP_ATHROW,

    // ===== Field access =====
    EJEEXOP_GETSTATIC, //2 byte operand
    EJEEXOP_PUTSTATIC, //2 byte operand
    EJEEXOP_GETFIELD, //2 byte operand
    EJEEXOP_PUTFIELD, //2 byte operand

    // ===== Method invocation =====
    EJEEXOP_INVOKEVIRTUAL, //2 byte operand
    EJEEXOP_INVOKESPECIAL, //2 byte operand
    EJEEXOP_INVOKESTATIC, //2 byte operand
    EJEEXOP_INVOKEINTERFACE, //2 byte operand
    EJEEXOP_INVOKENATIVE, //2 byte operand

    // ===== Object & array creation / operations =====
    EJEEXOP_NEW, //2 byte operand
    EJEEXOP_NEWARRAY, //1 byte operand
    EJEEXOP_ANEWARRAY, //2 byte operand
    EJEEXOP_ARRAYLENGTH,
    EJEEXOP_CHECKCAST, //2 byte operand
    EJEEXOP_INSTANCEOF, //2 byte operand
    EJEEXOP_MULTIANEWARRAY, //2 byte operand + 1 byte operand

    // ===== Monitors =====
    EJEEXOP_MONITORENTER,
    EJEEXOP_MONITOREXIT

}JEEXOpcodes_t;