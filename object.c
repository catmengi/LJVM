#include "bumper.h"
#include "class.h"
#include "object.h"
#include "linker.h"
#include "list.h"
#include "ljvm.h"
#include "thread.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#define HEAP_SIZE 2 * 1024 * 1024
#define GC_SCRATCHPAD_SIZE 64 * 1024

static bool mark_sweep_compact_gc(JHeap_t* heap, unsigned required_memory);
JHeap_t* object_heap_create(){
    JHeap_t* heap = malloc(sizeof(*heap));
    if(!heap) goto exit;
    if(bumper_create(&heap->heap_arena,HEAP_SIZE)) goto exit;
    if(bumper_create(&heap->gc_scratchpad, GC_SCRATCHPAD_SIZE)) goto exit;

    heap->heap_end = bumper_arena_end(&heap->heap_arena);
    heap->heap_start = bumper_arena_start(&heap->heap_arena);
    heap->gc = mark_sweep_compact_gc;

    return heap;
exit:
    free(heap);
    return NULL;
}

JObject_t* object_create(JClass_t* class){
    JHeap_t* heap = current_JThread->jvm->object_heap;
    assert(heap);

    size_t object_size = sizeof(JObject_t) + class->info->fields_size;

    uint8_t* object_memory = bumper_calloc(&heap->heap_arena,1,object_size);
    if(!object_memory){
        TODO("GC is required");
        return NULL;
    }

    JObject_t* header = (void*)object_memory;
    header->flags.is_weakref = is_classes_compatible(class,class_find(current_JThread->jvm->linker,"java/lang/WeakReference"));

    header->magic = 0xCABEBABE;
    header->class = class;
    header->object_size = object_size;

    return header;
}

JObject_t* array_create(unsigned length, JValue_type_t type,...){
    JHeap_t* heap = current_JThread->jvm->object_heap;
    assert(heap);

    size_t object_size = sizeof(JObject_t) + sizeof(JArray_t) + (JValue_sizeof(type) * length);
    uint8_t* object_memory = bumper_calloc(&heap->heap_arena,1,object_size);
    if(!object_memory){
        TODO("GC is required");
        return NULL;
    }

    va_list va_list = {0};
    if(type == EJVT_REFERENCE) va_start(va_list,type);

    JObject_t* header = (void*)object_memory;
    header->magic = 0xCABEBABE;
    header->class = type == EJVT_REFERENCE ? va_arg(va_list,JClass_t*) : NULL;
    header->object_size = object_size;
    
    if(type == EJVT_REFERENCE) va_end(va_list);

    header->flags.is_array = 1;

    JArray_t* array = (void*)(object_memory + sizeof(JObject_t));
    array->length = length;
    array->type = type;

    return header;
}


//Using field pointer here instead of a name, to allow me to "cache" hot fields (in native code only, and only in theory)
JError_t object_get_field(JObject_t* object, JField_t* field, void* output){
    JError_t err = EJERR_OK;
    
    FAIL_SET_JUMP(object,err,EJERR_NULLPTR,exit);
    FAIL_SET_JUMP(!object->flags.is_array,err,EJERR_WRONG_OBJECT_TYPE,exit);
    FAIL_SET_JUMP(field,err,EJERR_INVALID_ARGUMENT,exit);
    FAIL_SET_JUMP(!field->flags.is_static,err,EJERR_INVALID_ARGUMENT,exit);
    FAIL_SET_JUMP(field->offset < object->class->info->fields_size,err,EJERR_TYPE_MISMATCH, exit);

    void* element = &((uint8_t*)object + sizeof(*object))[field->offset];
    if(field->type == EJVT_REFERENCE || field->type == EJVT_NATIVE){
        #ifdef JHEAP_DWORD_PTR
        *(void**)output = ptr_decompress(current_JThread->jvm->object_heap->heap_start, *(uint32_t*)element);
        #else
        *(void**)output = element;
        #endif
    } else memcpy(output,element,JValue_sizeof(field->type));

exit:
    return err;
}

JError_t object_set_field(JObject_t* object, JField_t* field, void* value){
    JError_t err = EJERR_OK;
    
    FAIL_SET_JUMP(object,err,EJERR_NULLPTR,exit);
    FAIL_SET_JUMP(!object->flags.is_array,err,EJERR_WRONG_OBJECT_TYPE,exit);
    FAIL_SET_JUMP(field,err,EJERR_INVALID_ARGUMENT,exit);
    FAIL_SET_JUMP(!field->flags.is_static,err,EJERR_INVALID_ARGUMENT,exit);
    FAIL_SET_JUMP(field->offset < object->class->info->fields_size,err,EJERR_TYPE_MISMATCH, exit);

    void* element = &((uint8_t*)object + sizeof(*object))[field->offset];
        if(field->type == EJVT_REFERENCE || field->type == EJVT_NATIVE){
        #ifdef JHEAP_DWORD_PTR
        uint32_t compressed = ptr_compress(current_JThread->jvm->object_heap->heap_start, value);
        memcpy(element,&compressed,sizeof(uint32_t));
        #else
        *(void**)element = value;
        #endif
    } else memcpy(element,value,JValue_sizeof(field->type));

exit:
    return err;
}

JClass_t* object_get_class(JObject_t* object){
    return object ? object->class : NULL;
}

JMethod_t* object_get_method(JObject_t* object, char* method_name){
    JMethod_t* ret = NULL;
    if(object && !object->flags.is_array){
        ret = class_find_method(object->class, method_name, false);
    }
    return ret;
}

JError_t array_get_at(JObject_t* object, unsigned index, void* output){
    JError_t err = EJERR_OK;

    FAIL_SET_JUMP(object,err,EJERR_NULLPTR,exit);
    FAIL_SET_JUMP(object->flags.is_array,err,EJERR_WRONG_OBJECT_TYPE,exit);

    JArray_t* array = (void*)((uint8_t*)object + sizeof(*object));
    FAIL_SET_JUMP(array->length > index,err,EJERR_OUTOFBOUNDS,exit);

    uint8_t* elements = ((uint8_t*)array + sizeof(*array));
    void* element = &elements[(index * JValue_sizeof(array->type))];

    if(array->type == EJVT_REFERENCE || array->type == EJVT_NATIVE){
        #ifdef JHEAP_DWORD_PTR
        *(void**)output = ptr_decompress(current_JThread->jvm->object_heap->heap_start, *(uint32_t*)element);
        #else
        *(void**)output = element;
        #endif
    } else memcpy(output,element,JValue_sizeof(array->type));

exit:
    return err;
}

JError_t array_set_at(JObject_t* object, unsigned index, void* value){
    JError_t err = EJERR_OK;

    FAIL_SET_JUMP(object,err,EJERR_NULLPTR,exit);
    FAIL_SET_JUMP(object->flags.is_array,err,EJERR_WRONG_OBJECT_TYPE,exit);

    JArray_t* array = (void*)((uint8_t*)object + sizeof(*object));
    FAIL_SET_JUMP(array->length > index,err,EJERR_OUTOFBOUNDS,exit);

    uint8_t* elements = ((uint8_t*)array + sizeof(*array));
    void* element = &elements[(index * JValue_sizeof(array->type))];
    
    if(array->type == EJVT_REFERENCE || array->type == EJVT_NATIVE){
        #ifdef JHEAP_DWORD_PTR
        uint32_t compressed = ptr_compress(current_JThread->jvm->object_heap->heap_start, value);
        memcpy(element,&compressed,sizeof(uint32_t));
        #else
        *(void**)element = value;
        #endif
    } else memcpy(element,value,JValue_sizeof(array->type));
exit:
    return err;
}

static bool mark_sweep_compact_gc(JHeap_t* heap, unsigned required_memory){
}