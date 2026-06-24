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

#include "list.h"
#include "bumper.h"
#include "jerror.h"
#include "stream.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef enum{
    EJCT_NULL = 0, //NULL.....
    EJCT_CLASS = 7, //uint16_t
    EJCT_FIELDREF = 9, //JRaw_FMIM_ref_t*
    EJCT_METHODREF = 10, //JRaw_FMIM_ref_t*
    EJCT_INTERFACE_METHODREF = 11, //JRaw_FMIM_ref_t*
    EJCT_STRING = 8, //JRawString_t*
    EJCT_INT = 3, //uint32_t
    EJCT_FLOAT = 4, //uint32_t(float)
    EJCT_LONG = 5,  //uint64_t
    EJCT_DOUBLE = 6, //uint64_t(double)
    EJCT_NAMEANDTYPE = 12, //JRawNameAndType_t*
    EJCT_UTF8 = 1, //JRawUTF8_t*
}JConstantType_t;

typedef enum{
    EJAT_CODE, //"Code" JRawCodeAttribute_t
    EJAT_CONSTANTVALUE, //"ConstantValue" uint16_t*
    EJAT_STACKMAP, //"StackMap"
}JRawAttributeType_t;

typedef enum{
    VERIFIER_ITEM_TOP = 0,
    VERIFIER_ITEM_INT,
    VERIFIER_ITEM_FLOAT,
    VERIFIER_ITEM_DOUBLE,
    VERIFIER_ITEM_LONG,
    VERIFIER_ITEM_NULL,
    VERIFIER_ITEM_UNITIALIZED_THIS,
    VERIFIER_ITEM_OBJECT, //uint16_t cp_index;
    VERIFIER_ITEM_UNITIALIZED,
}JStackMapVerifierType_t;

typedef struct{
    uint8_t type;
    uint16_t ctx;
}JStackMapVerifierInfo_t;

typedef struct{
    uint16_t pc_pos;
    uint16_t locals_count;
    uint16_t stack_size;

    JStackMapVerifierInfo_t* locals;
    JStackMapVerifierInfo_t* stack;
}JStackMapFrame_t;

typedef struct{
    uint16_t entries_count;
    JStackMapFrame_t* entries;
}JStackMap_t;

typedef struct{
    uint16_t class_index;
    uint16_t nameandtype_index;
}JRaw_FMIM_ref_t;

typedef struct{
    uint16_t length;
    uint8_t* string;
}JRawUTF8_t;

typedef struct{
    uint16_t name_index;
    uint16_t descriptor_index;
}JRawNameAndType_t;

typedef struct{
    uint16_t utf8_index;
}JRawString_t;

typedef struct{
    void* value;
    uint8_t type;
}JConstant_t;

typedef struct{
    unsigned count;
    JConstant_t* constants;
}JConstantPool_t;

typedef struct{
    uint16_t max_stack;
    uint16_t max_locals;

    uint32_t code_length;
    uint8_t* code;

    uint16_t exception_table_length;
    struct{
        uint16_t start_pc;
        uint16_t end_pc;
        uint16_t handler_pc;
        uint16_t catch_type; //reference to EJCT_CLASS in constant pool
    }*exception_table;

    struct list_head attributes;
}JCodeAttribute_t;

//Using list for attributes to store only known to loader attributes, unknown just ignored
typedef struct{
    struct list_head list;

    uint8_t type;
    void* info;
}JRawAttribute_t;

typedef struct{
    uint16_t flags;
    uint16_t name_index;
    uint16_t descriptor_index;

    struct list_head attributes;
}JRawField_t;

typedef struct{
    uint16_t flags;
    uint16_t name_index;
    uint16_t descriptor_index;

    struct list_head attributes;
}JRawMethod_t;

typedef struct{
    union{
        uint32_t full_version;
        struct{
            uint16_t minor;
            uint16_t major;
        };
    }version;
    JConstantPool_t constantpool; //This constant pool will be filled with raw classfile-like structures
    uint16_t flags;

    uint16_t this_class;
    uint16_t super_class;

    uint16_t interfaces_count;
    uint16_t* interfaces;

    uint16_t fields_count;
    JRawField_t* fields;

    uint16_t methods_count;
    JRawMethod_t* methods;

    struct list_head attributes;
}JRawClass_t;

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
}JRawFlags_t;

void parser_init();

JConstant_t* parser_constantpool_get(JConstantPool_t* constantpool, unsigned index);
JRawClass_t* parser_parse_class(ClassStream_t* stream);