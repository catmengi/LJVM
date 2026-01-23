#pragma once

#include "reader.h"
#include "list.h"
#include "bumper.h"

#include <assert.h>
#include <string.h>

#define __FSJ_DO_BREAK__

#ifdef __FSJ_DO_BREAK__
static void FSJ_BREAK(){}
#define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %d\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); FSJ_BREAK(); goto label;}}
#else
#define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); goto label;}}
#endif

//#define __SUPPRESS_TODO__

#ifndef __SUPPRESS_TODO__
#define TODO(what) printf("%s TODO: '%s' at line %d\n",__PRETTY_FUNCTION__, (what), __LINE__)
#else
#define TODO(what)
#endif

typedef enum{
    CLASSLOADER_UNKNOWN = -1,
    CLASSLOADER_OK,
    CLASSLOADER_FILE_ERROR,
    CLASSLOADER_OOM,
}classloader_error_t;

typedef struct{
    bump_allocator_t loader_arena;

	char* loaded_from;
    struct list_head loaded_classes;

    struct{
        unsigned strliteral_count;
        unsigned classes_loaded;
		unsigned classes_referenced;
        unsigned constants_count_summary;
    }classes_stats;
}classloader_instance_t;

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
void classloader_set_origin(classloader_instance_t* instance, char* loaded_from);

void classloader_destroy(classloader_instance_t* instance);
classloader_error_t classloader_load_class(classloader_instance_t* instance, file_reader_t* reader);
classloader_error_t classloader_load_folder(classloader_instance_t* instance, const char* path);
