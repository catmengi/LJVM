#pragma once

#include "class_linker.h"
#include "jvm.h"

#include <sys/types.h>

typedef enum{
    EJOMOT_CLASS,
    EJOMOT_ARRAY,
}objectmanager_object_type_t;

typedef struct{
    classlinker_class_t* class;
    classlinker_field_t** fields; //2D field array, addressed via class generation
}objectmanager_class_object_t;

typedef struct{
    size_t count;
    jvm_value_t* elements;
}objectmanager_array_object_t;

typedef struct{
    struct list_head list;

    jvm_instance_t* jvm;
    pthread_mutex_t monitor;

    objectmanager_object_type_t type;
    void* data;
}objectmanager_object_t;

jvm_error_t objectmanager_init_heap(jvm_frame_t* frame, uint32_t heap_size);

objectmanager_object_t* objectmanager_new_class_object(jvm_frame_t* frame,
                                                       classlinker_class_t* class);

objectmanager_object_t* objectmanager_new_array_object(jvm_frame_t* frame, jvm_value_type_t type,
                                                       size_t size);

bool objectmanager_class_object_is_compatible_to(objectmanager_class_object_t* class_object, classlinker_class_t* class);

classlinker_method_t* objectmanager_object_get_method(objectmanager_object_t* object,
                                                            char* name, char* description);

classlinker_field_t* objectmanager_class_object_get_field(
                        objectmanager_class_object_t* class_object,char* name);

objectmanager_class_object_t* objectmanager_get_class_object_info(objectmanager_object_t* object);
objectmanager_array_object_t* objectmanager_get_array_object_info(objectmanager_object_t* object);
