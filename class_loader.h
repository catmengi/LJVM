#pragma once

#include "reader.h"
#include "arena.h"
#include "list.h"

#define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %x\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); *(int*)1 = 0;goto label;}}
#define TODO(what) printf("%s: TODO:: '%s' at line %d\n",__PRETTY_FUNCTION__, (what), __LINE__)

typedef enum{
    CLASSLOADER_UNKNOWN = -1,
    CLASSLOADER_OK,
    CLASSLOADER_FILE_ERROR,
    CLASSLOADER_OOM,
}classloader_error_t;

typedef struct{
    struct list_head loaded_classes;
    Arena* loader_arena;

    struct{
        unsigned strliteral_count;
        unsigned classes_loaded;
		unsigned classes_referenced;
        unsigned constants_count_summary;
    }classes_stats;
}classloader_instance_t;

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
}class_access_flags_t;

typedef enum{
	EJCT_utf8 = 1,
	EJCT_int = 3,
	EJCT_float,
	EJCT_long,
	EJCT_double,
	EJCT_class,
	EJCT_string,
	EJCT_fieldref,
	EJCT_methodref,
	EJCT_interfacemethodref,
	EJCT_nameandtype,

	EJCT_methodhandle = 15,
	EJCT_methodtype,

	EJCT_invokedynamic = 18,
	EJCT_unitialised_string = 100,
}classloader_constant_type_t;

typedef struct{
    classloader_constant_type_t type;
    void* data;
}classloader_constant_t;

typedef struct{
	uint16_t name_index;
	uint32_t length;
	void* classloader_attribute; //pointer to struct describing attribute
}classloader_attribute_t;

typedef struct{
	uint16_t access_flags;
	uint16_t name_index;
	uint16_t descriptor_index;

	uint16_t  attributes_count;
	classloader_attribute_t* attributes;
}classloader_field_t;

typedef struct{
	uint16_t access_flags;
	uint16_t name_index;
	uint16_t descriptor_index;

	uint16_t  attributes_count;
	classloader_attribute_t* attributes;
}classloader_method_t;

typedef struct{
	struct list_head list;
	struct {
		unsigned utf8_count;
	}class_stats;

	uint16_t constants_count;
	classloader_constant_t* constants;

	uint16_t access_flags;

	uint16_t this_class; //index to constant pool (Econstant_class)
	uint16_t super_class; //index to constant_pool (Econstant_class)

	uint16_t interfaces_count;
	uint16_t* interfaces;

	uint16_t fields_count;
	classloader_field_t* fields;

	uint16_t methods_count;
	classloader_method_t* methods;

	uint16_t attributes_count;
	classloader_attribute_t* attributes;
}classloader_class_t;

typedef struct{
	uint16_t name_index;
}classloader_constant_class_t;

typedef struct{
	uint16_t class_index;
	uint16_t name_and_type_index;
}classloader_constant_fmim_t; //Fieldref, methodref, interfacemethodref

typedef struct{
	uint16_t content_index; //index to utf8
}classloader_constant_string_t;

typedef struct{
	uint32_t integer;
}classloader_constant_int_t;

typedef struct{
	uint32_t float_bytes;
}classloader_constant_float_t;

typedef struct{
	uint32_t high_bytes;
	uint32_t low_bytes;
}classloader_constant_long_t;

typedef struct{
	uint32_t high_bytes;
	uint32_t low_bytes;
}classloader_constant_double_t;

typedef struct{
	uint16_t name_index;
	uint16_t descriptor_index;
}classloader_constant_name_and_type_t;

typedef struct{
	uint16_t length;
	uint8_t* bytes;
}classloader_constant_utf8_t;

typedef struct{
	uint8_t reference_kind;
	uint16_t reference_index;
}classloader_constant_methodhandle_t;

typedef struct{
	uint16_t descriptor_index;
}classloader_constant_methodtype_t;

typedef struct{ //probably will be not implemented
	uint16_t bootstrap_method_attr_index;
	uint16_t name_and_type_index;
}classloader_constant_invokedynamic_t;

classloader_instance_t* classloader_new();
classloader_error_t classloader_load_class(classloader_instance_t* instance, file_reader_t* reader);
int classloader_load_jar(classloader_instance_t* instance, const char* jar);
