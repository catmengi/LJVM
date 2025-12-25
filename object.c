#include "object.h"
#include "class_linker.h"
#include "jvm.h"

jvm_error_t objectmanager_init_heap(jvm_frame_t* frame, uint32_t heap_size){
    INIT_LIST_HEAD(&frame->jvm->heap.object_list);

    return JVM_OK;
}

objectmanager_object_t* objectmanager_new_class_object(jvm_frame_t* frame,
                                                       classlinker_class_t* class){

    TODO("Implement GC based memory manager! Using malloc() stub for now");

    objectmanager_object_t* new = malloc(sizeof(*new));
    assert(new);

    INIT_LIST_HEAD(&new->list);
    list_add(&new->list,&frame->jvm->heap.object_list);

    new->type = EJOMOT_CLASS;
    new->data = malloc(sizeof(objectmanager_class_object_t));
    assert(new->data);

    objectmanager_class_object_t* class_object = new->data;
    class_object->class = class;
    class_object->fields = calloc(class->generation + 1, sizeof(*class_object->fields));

    for(classlinker_class_t* cur = class; cur; cur = cur->parent){
        classlinker_normalclass_t* class_info = cur->info;
        class_object->fields[cur->generation] = calloc(class_info->fields_count,
                                            sizeof(*class_object->fields[0]));

        for(unsigned i = 0; i < class_info->fields_count; i++){
            class_object->fields[cur->generation][i] = class_info->fields[i];
            class_object->fields[cur->generation][i].name = \
                        strdup(class_object->fields[cur->generation][i].name);
        }
    }

    return new;
}

objectmanager_object_t* objectmanager_new_array_object(jvm_frame_t* frame, jvm_value_type_t type,
                                                       size_t size){
    TODO("GC based memory manager for arrays. Using malloc() for now");
    objectmanager_object_t* new = malloc(sizeof(*new));

    INIT_LIST_HEAD(&new->list);
    list_add(&new->list,&frame->jvm->heap.object_list);

    new->type = EJOMOT_ARRAY;
    new->data = calloc(1,sizeof(objectmanager_array_object_t));

    objectmanager_array_object_t* array_object = new->data;
    array_object->count = size;
    array_object->elements = calloc(size,sizeof(*array_object->elements));

    for(unsigned i = 0; i < size; i++){
        array_object->elements[i].type = type;
    }

    return new;
}

objectmanager_class_object_t* objectmanager_get_class_object_info(objectmanager_object_t* object){
    objectmanager_class_object_t* ret = NULL;
    if(object->type == EJOMOT_CLASS){
        ret = object->data;
    }
    return ret;
}

objectmanager_array_object_t* objectmanager_get_array_object_info(objectmanager_object_t* object){
    objectmanager_array_object_t* ret = NULL;
    if(object->type == EJOMOT_ARRAY){
        ret = object->data;
    }
    return ret;
}

classlinker_field_t* objectmanager_class_object_get_field(objectmanager_class_object_t* class_object,
                                                          char* name){
    classlinker_field_t* found = NULL;
    for(classlinker_class_t* cur = class_object->class; cur; cur = cur->parent){
        classlinker_normalclass_t* class_info = cur->info;
        for(unsigned i = 0; i < class_info->fields_count; i++){
            if(strcmp(class_object->fields[cur->generation][i].name, name) == 0){
                return &class_object->fields[cur->generation][i];
            }
        }
    }
    return NULL;
}

classlinker_method_t* objectmanager_class_object_get_method(objectmanager_class_object_t* class_object,
                                                            char* name, char* description){
    return classlinker_find_method(class_object->class,name,description);
}

bool objectmanager_class_object_is_compatible_to(objectmanager_class_object_t* class_object, classlinker_class_t* class){
    return classlinker_is_classes_compatible(class_object->class,class);
}