#include "class.h"
#include "object.h"
#include "linker.h"
#include "ljvm.h"
#include "thread.h"

#include <stdlib.h>
#include <stdarg.h>

#define HEAP_SIZE 2 * 1024 * 1024

JHeap_t* object_heap_create(){
    JHeap_t* heap = malloc(sizeof(*heap));
    if(!heap) goto exit;
    if(bumper_create(&heap->arena,HEAP_SIZE)) goto exit;

    heap->heap_end = bumper_arena_end(&heap->arena);
    heap->heap_start = bumper_arena_start(&heap->arena);

    return heap;
exit:
    free(heap);
    return NULL;
}

JObject_t* object_create(JClass_t* class){
    JHeap_t* heap = current_JThread->jvm->object_heap;
    assert(heap);

    size_t object_size = sizeof(JObject_t) + class->info->fields_size;

    uint8_t* object_memory = bumper_calloc(&heap->arena,1,object_size);
    if(!object_memory){
        TODO("GC is required");
        return NULL;
    }

    JObject_t* header = (void*)object_memory;
    header->flags.is_weakref = linker_is_classes_compatible(class,linker_find(current_JThread->jvm->linker,"java/lang/WeakReference"));

    header->magic = 0xCAFEBABE;
    header->class = class;
    header->object_size = object_size;

    return header;
}

JObject_t* array_create(unsigned length, JValue_type_t type,...){
    JHeap_t* heap = current_JThread->jvm->object_heap;
    assert(heap);

    size_t object_size = sizeof(JObject_t) + sizeof(JArray_t) + (JValue_sizeof(type) * length);
    uint8_t* object_memory = bumper_calloc(&heap->arena,1,object_size);
    if(!object_memory){
        TODO("GC is required");
        return NULL;
    }

    va_list va_list = {0};
    va_start(va_list,type);

    JObject_t* header = (void*)object_memory;
    header->magic = 0xCAFEBABE;
    header->class = type == EJVT_REFERENCE ? va_arg(va_list,JClass_t*) : NULL;
    header->object_size = object_size;
    
    header->flags.is_array = 1;

    JArray_t* array = (void*)(object_memory + sizeof(JObject_t));
    array->length = length;
    array->type = type;
    array->items = (void*)(object_memory + sizeof(JObject_t) + sizeof(JArray_t));

    return header;
}

void* object_get_field(JObject_t* object, JField_t* field){
    void* field_content = NULL;
    
    FAIL_SET_JUMP(object,field_content,NULL,exit);
    FAIL_SET_JUMP(!object->flags.is_array, field_content, NULL,exit);
    FAIL_SET_JUMP(field,field_content,NULL,exit);

exit:
    return field_content;
}