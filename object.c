#include "object.h"
#include "arena.h"
#include "class_linker.h"
#include "class_loader.h"
#include "jvm.h"
#include "jvm_internal.h"
#include "jvm_method.h"
#include "list.h"

#include "hashmap/hashmap.h"

#include <pthread.h>

#define MONITOR_TABLE_SIZE 128

static objectmanager_object_t* array_JLObject = NULL;

static uint32_t nextby(uint32_t value, uint32_t by){
    if (value % by == 0) {
        return value; // Already divisible by 16
    }
    return (value / by + 1) * by; // Calculate the next multiple of 16
}

jvm_error_t objectmanager_init_heap(jvm_instance_t* jvm, uint32_t heap_size){
    INIT_LIST_HEAD(&jvm->heap.object_list);

    unsigned reserved = nextby(heap_size * 0.0625, sizeof(objectmanager_object_t));
    jvm->heap.gc_heapsize = heap_size - reserved;

    jvm->heap.gc_heap = arena_new_dynamic(jvm->heap.gc_heapsize);
    assert(jvm->heap.gc_heap);

    array_JLObject = objectmanager_new_class_object(&(jvm_frame_t){jvm},classlinker_find_class(jvm->linker,"java/lang/Object"));
    assert(array_JLObject);

    jvm->heap.monitor_table.monitor_count = MONITOR_TABLE_SIZE;
    jvm->heap.monitor_table.monitors = arena_calloc(jvm->arena,jvm->heap.monitor_table.monitor_count,sizeof(*jvm->heap.monitor_table.monitors));
    assert(jvm->heap.monitor_table.monitors);

    for(unsigned i = 0; i < jvm->heap.monitor_table.monitor_count; i++){
        mutex_init(&jvm->heap.monitor_table.monitors[i].monitor);
        jvm->heap.monitor_table.monitors[i].used = false;
    }

    return JVM_OK;
}

static int allocate_monitor(jvm_instance_t* jvm){
    int found = EJOMMT_FALLBACK_GIL; //Use fallback GIL

    jvm_lock(jvm);
    for(unsigned i = 0; i < jvm->heap.monitor_table.monitor_count; i++){
        if(jvm->heap.monitor_table.monitors[i].used == false){
            jvm->heap.monitor_table.monitors[i].used = true;
            jvm->heap.monitor_table.monitors[i].last_user = NULL;
            found = i;
            break;
        }
    }
    jvm_unlock(jvm);

    return found;
}

void objectmanager_object_clone_into(objectmanager_object_t* object, struct list_head* object_list, void* memory){
    assert(memory);

    memcpy(memory,object,object->size);
    
    objectmanager_object_t* new_object = memory;

    INIT_LIST_HEAD(&new_object->list);

    if(object_list)
        list_add(&new_object->list,object_list);

    switch(new_object->type){
        case EJOMOT_CLASS:{
            void* cobject_memory = memory + sizeof(objectmanager_object_t);
            objectmanager_class_object_t* cobject = cobject_memory;
            new_object->data = cobject;

            void* cobject_fields_memory = cobject_memory + sizeof(objectmanager_class_object_t);
            void* cobject_fields_content_memory = cobject_fields_memory + ((cobject->class->generation + 1) * sizeof(classlinker_field_t*));

            cobject->fields = cobject_fields_memory;
    
            void* cur_fields_mem = cobject_fields_content_memory;
            for(classlinker_class_t* cur_class = cobject->class; cur_class; cur_class = cur_class->parent){
                classlinker_normalclass_t* class_info = cur_class->info;

                cobject->fields[cur_class->generation] = class_info->fields_count > 0 ? cur_fields_mem : NULL;
                cur_fields_mem += (class_info->fields_count * sizeof(classlinker_field_t));
            }
        }
        break;

        case EJOMOT_ARRAY:{
            objectmanager_array_object_t* array = memory + sizeof(objectmanager_object_t);
            new_object->data = array;

            array->JLObject = array_JLObject;
            array->elements = memory + sizeof(objectmanager_object_t) + sizeof(objectmanager_array_object_t);
        }
        break;
    }
}

void objectmanager_scan_object(jvm_instance_t* jvm, objectmanager_object_t* object, struct list_head* output);
void objectmanager_scan_classes(jvm_instance_t* jvm, struct list_head* output){
    classlinker_class_t* cur = NULL;
    list_for_each_entry(cur,&jvm->linker->loaded_classes,list){
        if(cur->type == EClass){
            classlinker_normalclass_t* class_info = cur->info;
            for(unsigned i = 0; i < class_info->static_fields_count; i++){
                jvm_value_t value = class_info->static_fields[i].value;
                if(value.type == EJVT_REFERENCE){
                    objectmanager_object_t* object = *(void**)value.value;
                    if(object){
                        list_del_init(&object->list);
                        list_add(&object->list,output);
                        objectmanager_scan_object(jvm,object,output);                                
                    }
                }
            }

            for(unsigned i = 0; i < class_info->constant_pool.constants_count; i++){
                classlinker_constant_t* constant = &class_info->constant_pool.constants[i];
                if(constant->constant_type == EJCT_string){
                    objectmanager_object_t* string = constant->constant_value;

                    list_del_init(&string->list);
                    list_add(&string->list,output);
                    objectmanager_scan_object(jvm,string,output);
                }
            }
        }
    }
}
void objectmanager_scan_object(jvm_instance_t* jvm, objectmanager_object_t* object, struct list_head* output){
    if(object->gc_visited)
        return;
    else object->gc_visited = true; 

    switch(object->type){
        case EJOMOT_CLASS:{
            objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(object);
            for(classlinker_class_t* cur = cobject->class; cur; cur = cur->parent){
                classlinker_normalclass_t* class_info = cur->info;
                for(unsigned i = 0; i < class_info->fields_count; i++){
                    jvm_value_t value = cobject->fields[cur->generation][i].value;
                    if(value.type == EJVT_REFERENCE){
                        objectmanager_object_t* object = *(void**)value.value;
                        if(object){
                            list_del_init(&object->list);
                            list_add(&object->list,output);
                            objectmanager_scan_object(jvm,object,output);                            
                        }
                    }
                }
            }
        }
        break;

        case EJOMOT_ARRAY:{
            objectmanager_array_object_t* aobject = objectmanager_get_array_object_info(object);

            for(unsigned i = 0; i < aobject->count; i++){
                jvm_value_t value = aobject->elements[i];
                if(value.type == EJVT_REFERENCE){
                    objectmanager_object_t* object = *(void**)value.value;
                        if(object){
                            list_del_init(&object->list);
                            list_add(&object->list,output);
                            objectmanager_scan_object(jvm,object,output);                            
                        }
                }
            }
        }
        break;
    }
}

void objectmanager_scan_frame(jvm_frame_t* start, struct list_head* output){
    for(jvm_frame_t* cur = start; cur; cur = cur->previous_frame){
        //Step 1: scan locals
        for(unsigned i = 0; i < cur->method->frame_descriptor.locals_count; i++){
            jvm_value_t value = cur->locals[i];
            if(value.type == EJVT_REFERENCE){
                objectmanager_object_t* object = *(void**)value.value;
                if(object){
                    list_del_init(&object->list);
                    list_add(&object->list,output);
                    objectmanager_scan_object(cur->jvm,object,output);                            
                }
            }
        }

        //Step 2: scan stack
        for(unsigned i = 0; i < cur->stack.sp; i++){
            jvm_value_t value = cur->stack.stack[i];
            if(value.type == EJVT_REFERENCE){
                objectmanager_object_t* object = *(void**)value.value;
                    if(object){
                        list_del_init(&object->list);
                        list_add(&object->list,output);
                        objectmanager_scan_object(cur->jvm,object,output);                            
                    }               
            }
        }

        //Step 3: scan exceptions for native methods
        jvm_native_exception_t* exception = NULL;
        list_for_each_entry(exception,&cur->native_exceptions,list){
            list_del_init(&exception->exception_object->list);
            list_add(&exception->exception_object->list,output);
            objectmanager_scan_object(cur->jvm,exception->exception_object,output);           
        }
    }
}

size_t hash_ptr(const void* data){
    return hashmap_hash_default(&data,sizeof(&data));
}

int ptr_cmp(const void* a, const void* b){
    return !(a == b);
}

void objectmanager_gc(jvm_instance_t* jvm, unsigned required_memory){
    jvm_lock(jvm);

    struct list_head used_objects_list = LIST_HEAD_INIT(used_objects_list);
    struct list_head copied_objects_list = LIST_HEAD_INIT(copied_objects_list);
    objectmanager_scan_classes(jvm,&used_objects_list);

    jvm_thread_t* current_thread = NULL, *tmp_thread;
    list_for_each_entry_safe(current_thread,tmp_thread,&jvm->threads, list){
        if(current_thread != jvm_current_thread){ //try to not stop gc itself)

            //Call function to freeze thread (implemented via java because it is OS dependant. Know how to implement it on Windows and FreeRTOS but not on posix)
            jvm_frame_t freeze_frame = {
                .jvm = jvm,
                .locals = (jvm_value_t[1]){C_TO_NEW_JVM_VALUE(current_thread->JThread)},
            };
            INIT_LIST_HEAD(&freeze_frame.native_exceptions);

            classlinker_method_t* freeze_method = objectmanager_class_object_get_method(&freeze_frame,objectmanager_get_class_object_info(current_thread->JThread),
                                                                             "os_suspend", "()V");

            if(freeze_method){
                freeze_frame.method = freeze_method;
                freeze_method->fn(&freeze_frame);
            }
            

            //Add it to used list and scan what it contains

            if(current_thread->JThread){
                list_del_init(&current_thread->JThread->list); //Remove from main object list and add to used list
                list_add(&current_thread->JThread->list,&used_objects_list);
                objectmanager_scan_object(jvm,current_thread->JThread,&used_objects_list);
            }
        }


        objectmanager_scan_frame(current_thread->topmost_frame, &used_objects_list);
    }

    size_t used_memory_ammount = 0;
    objectmanager_object_t* early_oom_counter_cur = NULL;
    list_for_each_entry(early_oom_counter_cur, &used_objects_list, list){
        used_memory_ammount += early_oom_counter_cur->size;
    }

    if(jvm->heap.gc_heapsize - used_memory_ammount < required_memory){ //Not enough memory. Can give up already
        objectmanager_object_t* cur_object = NULL;
        list_for_each_entry(cur_object,&used_objects_list,list){
            list_del_init(&cur_object->list);
            list_add(&cur_object->list,&jvm->heap.object_list); //return them to main heap object list
        }

        printf("%s: Early OOM, cannot find enough memory for allocation!\n",__PRETTY_FUNCTION__);
        return;
    }

    HASHMAP(void,objectmanager_object_t) pointer_fix_table = {0};
    HASHMAP(void,objectmanager_object_t) reverse_pointer_fix_table = {0};
    hashmap_init(&pointer_fix_table,hash_ptr,ptr_cmp);
    hashmap_init(&reverse_pointer_fix_table,hash_ptr,ptr_cmp);


    objectmanager_object_t* finalize_cur = NULL, *finalize_tmp;
    list_for_each_entry_safe(finalize_cur,finalize_tmp,&jvm->heap.object_list,list){
        classlinker_method_t* finalize_method = objectmanager_class_object_get_method(&(jvm_frame_t){jvm}, objectmanager_get_class_object_info(finalize_cur), "finalize", "()V");
        if(finalize_method){
            jvm_value_t args[] = {{EJVT_REFERENCE}};
            *(void**)args[0].value = finalize_cur;
                
                
            jvm_value_t stack[finalize_method->frame_descriptor.stack_size]; //I love using stack memory so much!
            jvm_frame_t finalize_frame = {
                .jvm = jvm,
                .method = finalize_method,
                .previous_frame = &(jvm_frame_t){
                    .method = &(classlinker_method_t){
                        .name = "GC_exception_catcher_stub",
                        .flags = ACC_NATIVE,
                    },
                    .jvm = jvm,
                },
                .native_exceptions = LIST_HEAD_INIT(finalize_frame.native_exceptions),
                .locals = (jvm_value_t[]){},
                .stack.stack = stack,
            };
            INIT_LIST_HEAD(&finalize_frame.previous_frame->native_exceptions);
            finalize_method->fn(&finalize_frame);

            objectmanager_class_object_t* ccur = objectmanager_get_class_object_info(finalize_cur);
            if(ccur->monitor_id >= 0){
                jvm->heap.monitor_table.monitors[ccur->monitor_id].used = false; //Free mutex
            }
        }
    }

    objectmanager_object_t* used_object = NULL; //Copy used objects
    list_for_each_entry(used_object,&used_objects_list,list){
        objectmanager_object_t* new_object = malloc(used_object->size);
        assert(new_object);

        objectmanager_object_clone_into(used_object, &copied_objects_list, new_object);
        hashmap_insert(&reverse_pointer_fix_table,new_object,used_object,NULL);
    }
    INIT_LIST_HEAD(&jvm->heap.object_list);
    arena_reset_zero(jvm->heap.gc_heap); //Now reset heap to 0

    objectmanager_object_t* copy_to_put = NULL;
    objectmanager_object_t* copy_to_put_tmp = NULL;
    list_for_each_entry_safe(copy_to_put,copy_to_put_tmp,&copied_objects_list,list){

        objectmanager_object_t* new_object = arena_alloc(jvm->heap.gc_heap,copy_to_put->size);
        assert(new_object);

        objectmanager_object_clone_into(copy_to_put,&jvm->heap.object_list, new_object);

        void* old_object = hashmap_get(&reverse_pointer_fix_table,copy_to_put);
        hashmap_insert(&pointer_fix_table,old_object,new_object,NULL);
        hashmap_insert(&reverse_pointer_fix_table,new_object,old_object,NULL);

        new_object->gc_visited = false; //reset mark

        list_del(&copy_to_put->list);
        free(copy_to_put);
    }

    list_for_each_entry_safe(current_thread,tmp_thread, &jvm->threads, list){
        for(jvm_frame_t* cur = current_thread->topmost_frame; cur; cur = cur->previous_frame){
            current_thread->JThread = hashmap_get(&pointer_fix_table,current_thread->JThread);

            //Step 1: patch local variables
            for(unsigned i = 0; i < cur->method->frame_descriptor.locals_count; i++){
                jvm_value_t* value = &cur->locals[i];
                if(value->type == EJVT_REFERENCE && *(void**)value->value){
                    *(void**)value->value = hashmap_get(&pointer_fix_table,*(void**)value->value);  
                }
            }

            //Step 2: patch stack:
            for(unsigned i = 0; i < cur->stack.sp; i++){
                jvm_value_t* value = &cur->stack.stack[i];
                if(value->type == EJVT_REFERENCE && *(void**)value->value != NULL){
                    *(void**)value->value = hashmap_get(&pointer_fix_table,*(void**)value->value);  
                }
            }

            jvm_native_exception_t* native_exception = NULL;
            list_for_each_entry(native_exception,&cur->native_exceptions,list){
                native_exception->exception_object = hashmap_get(&pointer_fix_table,native_exception->exception_object);
            } 
        }
    }

    objectmanager_object_t* cur_object = NULL;
    list_for_each_entry(cur_object,&jvm->heap.object_list,list){ //Patch all objects reference fields
        switch(cur_object->type){
            case EJOMOT_CLASS:{
                objectmanager_class_object_t* class_object = cur_object->data;
                for(classlinker_class_t* cur = class_object->class; cur; cur = cur->parent){
                    assert(cur->type == EClass);

                    classlinker_normalclass_t* class_info = cur->info;
                    for(unsigned i = 0; i < class_info->fields_count; i++){
                        jvm_value_t* value = &class_object->fields[cur->generation][i].value;

                        if(value->type == EJVT_REFERENCE && *(void**)value->value){
                            *(void**)value->value = hashmap_get(&pointer_fix_table,*(void**)value->value);        
                        }
                    }
                }
            }
            break;

            case EJOMOT_ARRAY:{
                objectmanager_array_object_t* array_object = cur_object->data;
                for(unsigned i = 0; i < array_object->count; i++){
                    jvm_value_t* value = &array_object->elements[i];
                    if(value->type == EJVT_REFERENCE && *(void**)value->value){
                        *(void**)value->value = hashmap_get(&pointer_fix_table,*(void**)value->value);  
                    }
                }
            }
            break;
        }
    }

    classlinker_class_t* cur_class = NULL;
    list_for_each_entry(cur_class,&jvm->linker->loaded_classes,list){ //Path static fields of classes
        if(cur_class->type == Earray) continue;
        classlinker_normalclass_t* class_info = cur_class->info;
        for(unsigned i = 0; i < class_info->static_fields_count; i++){
            jvm_value_t* value = &class_info->static_fields[i].value;
            if(value->type == EJVT_REFERENCE && *(void**)value->value){
                *(void**)value->value = hashmap_get(&pointer_fix_table,*(void**)value->value);
            }
        }

        for(unsigned i = 0; i < class_info->constant_pool.constants_count; i++){
            classlinker_constant_t* constant = &class_info->constant_pool.constants[i];
            if(constant->constant_type == EJCT_string){ //Patch strings, because they may also be moved
                constant->constant_value = hashmap_get(&pointer_fix_table,constant->constant_value);
            }
        }
    }

    hashmap_cleanup(&pointer_fix_table);
    hashmap_cleanup(&reverse_pointer_fix_table);

    list_for_each_entry_safe(current_thread,tmp_thread,&jvm->threads, list){
        if(current_thread != jvm_current_thread){

            //Call function to unfreeze thread (implemented via java because it is OS dependant. Know how to implement it on Windows and FreeRTOS but not on posix)
            jvm_frame_t unfreeze_frame = {
                .jvm = jvm,
                .locals = (jvm_value_t[1]){C_TO_NEW_JVM_VALUE(current_thread->JThread)},
            };
            INIT_LIST_HEAD(&unfreeze_frame.native_exceptions);

            classlinker_method_t* unfreeze_method = objectmanager_class_object_get_method(&unfreeze_frame,objectmanager_get_class_object_info(current_thread->JThread),
                                                                             "os_resume", "()V");
            if(unfreeze_method){
                unfreeze_frame.method = unfreeze_method;
                unfreeze_method->fn(&unfreeze_frame);
            }

        }
    }

    jvm_unlock(jvm);
}

objectmanager_object_t* objectmanager_new_class_object(jvm_frame_t* frame,
                                                       classlinker_class_t* class){
    if(!class) return NULL;

    unsigned alloc_size = sizeof(objectmanager_object_t) + sizeof(objectmanager_class_object_t) + ((class->generation + 1) * sizeof(classlinker_field_t*));
    objectmanager_object_t* new_object = NULL;
    jvm_lock(frame->jvm);

    for(classlinker_class_t* cur = class; cur; cur = cur->parent){
        classlinker_normalclass_t* class_info = cur->info;
        alloc_size += (class_info->fields_count * sizeof(class_info->fields[0])) + 1;
    }

    void* memory = arena_calloc(frame->jvm->heap.gc_heap,1,alloc_size);
    if(memory == NULL){
        jvm_unlock(frame->jvm);
        objectmanager_gc(frame->jvm,alloc_size);

        jvm_lock(frame->jvm);
        memory = arena_calloc(frame->jvm->heap.gc_heap,1,alloc_size);
        if(memory == NULL) goto exit;
    }

    void* cobject_memory = memory + sizeof(objectmanager_object_t);
    void* cobject_fields_memory = cobject_memory + sizeof(objectmanager_class_object_t);
    void* cobject_fields_content_memory = cobject_fields_memory + ((class->generation + 1) * sizeof(classlinker_field_t*));

    new_object = memory;
    objectmanager_class_object_t* cobject = cobject_memory;

    INIT_LIST_HEAD(&new_object->list);
    list_add(&new_object->list,&frame->jvm->heap.object_list);

    new_object->type = EJOMOT_CLASS;

    new_object->data = cobject;
    new_object->size = alloc_size;

    cobject->class = class;
    cobject->fields = cobject_fields_memory;
    cobject->monitor_id = EJOMMT_UNITIALISED;

    void* cur_fields_mem = cobject_fields_content_memory;
    for(classlinker_class_t* cur_class = cobject->class; cur_class; cur_class = cur_class->parent){
        classlinker_normalclass_t* class_info = cur_class->info;

        cobject->fields[cur_class->generation] = class_info->fields_count > 0 ? cur_fields_mem : NULL;
        cur_fields_mem += (class_info->fields_count * sizeof(classlinker_field_t));

        memcpy(cobject->fields[cur_class->generation],class_info->fields,class_info->fields_count * sizeof(classlinker_field_t));
    }

exit:
    jvm_unlock(frame->jvm);
    return new_object;
}

objectmanager_object_t* objectmanager_new_array_object(jvm_frame_t* frame, jvm_value_type_t type,
                                                       size_t size){
    unsigned alloc_size = sizeof(objectmanager_object_t) + sizeof(objectmanager_array_object_t) + (sizeof(jvm_value_t) * size);
    objectmanager_object_t* array_obj = NULL;
    jvm_lock(frame->jvm);

    void* memory = arena_calloc(frame->jvm->heap.gc_heap,1,alloc_size);
    if(memory == NULL){
        jvm_unlock(frame->jvm);
        objectmanager_gc(frame->jvm,alloc_size);
        
        jvm_lock(frame->jvm);
        memory = arena_calloc(frame->jvm->heap.gc_heap,1,alloc_size);
        if(memory == NULL) goto exit;
    }

    array_obj = memory;
    objectmanager_array_object_t* array = memory + sizeof(objectmanager_object_t);

    INIT_LIST_HEAD(&array_obj->list);
    list_add(&array_obj->list,&frame->jvm->heap.object_list);
    
    array_obj->type = EJOMOT_ARRAY;
    array_obj->data = array;
    array_obj->size = alloc_size;

    array->JLObject = array_JLObject;
    array->elements = memory + sizeof(objectmanager_object_t) + sizeof(objectmanager_array_object_t);
    array->count = size;

    for(unsigned i = 0; i < array->count; i++){
        array->elements[i].type = type;
    }

exit:
    jvm_unlock(frame->jvm);
    return array_obj;
}

objectmanager_class_object_t* objectmanager_get_class_object_info(objectmanager_object_t* object){
    if(!object) return NULL;

    if(object->type == EJOMOT_CLASS){
        return object->data;
    }
    return ((objectmanager_array_object_t*)object->data)->JLObject->data;
    
}

objectmanager_array_object_t* objectmanager_get_array_object_info(objectmanager_object_t* object){
    if(!object) return NULL;

    if(object->type == EJOMOT_ARRAY){
        return object->data;
    }
    return NULL;
}

classlinker_field_t* objectmanager_class_object_get_field(jvm_frame_t* frame, objectmanager_class_object_t* class_object,
                                                          char* name){
    if(!name || !class_object) return NULL;

    for(classlinker_class_t* cur = class_object->class; cur; cur = cur->parent){
        classlinker_normalclass_t* class_info = cur->info;
        for(unsigned i = 0; i < class_info->fields_count; i++){            
            if(strcmp(class_object->fields[cur->generation][i].name, name) == 0){

                if((class_object->fields[cur->generation][i].flags & ACC_PRIVATE) == ACC_PRIVATE){
                    if(frame == NULL || cur == frame->method->class)
                        return &class_object->fields[cur->generation][i];
                    else continue;
                } else return &class_object->fields[cur->generation][i];
            }
        }
    }
    return NULL;
}

classlinker_method_t* objectmanager_class_object_get_method(jvm_frame_t* frame, objectmanager_class_object_t* object,
                                                            char* name, char* description){
    if(!object || !name) return NULL;
    return classlinker_find_method(frame, object->class,name,description);
}

objectmanager_object_t* objectmanager_object_clone(jvm_frame_t* frame, objectmanager_object_t* object){
    jvm_lock(frame->jvm);
    objectmanager_object_t* new_object = arena_alloc(frame->jvm->heap.gc_heap,object->size);
    if(new_object == NULL){
        objectmanager_gc(frame->jvm,object->size);

        new_object = arena_alloc(frame->jvm->heap.gc_heap,object->size);
        if(new_object == NULL){
            jvm_unlock(frame->jvm);
            return NULL;
        }
    }
    objectmanager_object_clone_into(object,&frame->jvm->heap.object_list,new_object);
    objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(new_object);
    cobject->monitor_id = EJOMMT_UNITIALISED;

    jvm_unlock(frame->jvm);
    return new_object;
}

void objectmanager_object_synclock(objectmanager_object_t* object){
    objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(object);
    if(cobject){
        if(cobject->monitor_id == EJOMMT_UNITIALISED)
            cobject->monitor_id = allocate_monitor(jvm_current_thread->jvm);

        mutex_t* lock = cobject->monitor_id == -1 ? &jvm_current_thread->jvm->heap.fallback_monitor :
                                                    &jvm_current_thread->jvm->heap.monitor_table.monitors[cobject->monitor_id].monitor;

        jvm_current_thread->jvm->heap.monitor_table.monitors[cobject->monitor_id].last_user = jvm_current_thread;
        mutex_lock(lock);
    }
}
void objectmanager_object_syncunlock(objectmanager_object_t* object){
    objectmanager_class_object_t* cobject = objectmanager_get_class_object_info(object);
    if(cobject){
        if(cobject->monitor_id == EJOMMT_UNITIALISED)
            cobject->monitor_id = allocate_monitor(jvm_current_thread->jvm);

        mutex_t* lock = cobject->monitor_id == -1 ? &jvm_current_thread->jvm->heap.fallback_monitor :
                                                    &jvm_current_thread->jvm->heap.monitor_table.monitors[cobject->monitor_id].monitor;

        mutex_unlock(lock);
    }
}

static uint32_t h31_hash(const char* s, size_t len)
{
    uint32_t h = 0;
    while (len) {
        h = 31 * h + *s++;
        --len;
    }
    return h;
}
uint32_t objectmanager_hash(objectmanager_object_t* object){
    uint32_t hash = 0;
    if(!object) return 0;

    switch(object->type){
        case EJOMOT_CLASS:{
            objectmanager_class_object_t* cobject = object->data;

            for(classlinker_class_t* cur = cobject->class; cur; cur = cur->parent){
                classlinker_normalclass_t* class_info = cur->info;
                hash += h31_hash(cur->this_name, strlen(cur->this_name));

                for(unsigned i = 0; i < class_info->fields_count; i++){
                    classlinker_field_t* field = &cobject->fields[cur->generation][i];
                    jvm_value_t* value = &field->value;

                    if(value->type != EJVT_REFERENCE && value->type != EJVT_ARRAYDIM && value->type != EJVT_NATIVEPTR){
                        hash += h31_hash((char*)value,sizeof(*value));
                    } else if(value->type == EJVT_REFERENCE && *(void**)value->value){
                        hash += objectmanager_hash(*(void**)value->value);
                    }
                }
            }
        }
        break;

        case EJOMOT_ARRAY:{
            objectmanager_array_object_t* aobject = object->data;
            for(unsigned i = 0; i < aobject->count; i++){
                jvm_value_t* value = &aobject->elements[i];

                if(value->type != EJVT_REFERENCE && value->type != EJVT_ARRAYDIM && value->type != EJVT_NATIVEPTR){
                    hash += h31_hash((char*)value,sizeof(*value));
                } else if(value->type == EJVT_REFERENCE && *(void**)value->value){
                    hash += objectmanager_hash(*(void**)value->value);
                }
            }
        }
        break;
    }

    return hash;
}

bool objectmanager_class_object_is_compatible_to(objectmanager_class_object_t* class_object, classlinker_class_t* class){
    return classlinker_is_classes_compatible(class_object->class,class);
}