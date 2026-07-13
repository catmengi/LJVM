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

#include "heap.h"
#include "bumper.h"
#include "class.h"
#include "classtable.h"
#include "config.h"
#include "interpreter.h"
#include "jerror.h"
#include "list.h"
#include "thread.h"
#include "monitor.h"
#include "memman.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>

#ifdef TARGET_ESPIDF
#include "freertos/freeRTOS.h"
#include "freertos/sem.h"
static SemaphoreHandle_t s_heap_lock = NULL;
#else
#include <pthread.h>
static pthread_mutex_t s_heap_lock = {0};
#endif

static struct list_head s_gc_thread_list;
static bump_allocator_t* s_arena = NULL;
static bool is_initialised = false;

static int value_type_size[] = {
    [TYPE_VOID] = 0,
    [TYPE_BYTE] = sizeof(int8_t),
    [TYPE_BOOL] = sizeof(bool),
    [TYPE_CHAR] = sizeof(int16_t), //Java....
    [TYPE_SHORT] = sizeof(int16_t),
    [TYPE_INT] = sizeof(int32_t),
    [TYPE_FLOAT] = sizeof(float),
    [TYPE_LONG] = sizeof(int64_t),
    [TYPE_DOUBLE] = sizeof(double),
    [TYPE_REFERENCE] = sizeof(Object_t*),
};

static void heap_enter_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_lock(&s_heap_lock);
    #else
    xSemaphoreTakeRecursive(s_heap_lock, portMAX_DELAY);
    #endif
}

static void heap_exit_critical(){
    #ifdef TARGET_LINUX
    pthread_mutex_unlock(&s_heap_lock);
    #else
    xSemaphoreGiveRecursive(s_heap_lock);
    #endif    
}

void heap_init(){
    INIT_LIST_HEAD(&s_gc_thread_list);
    if(!is_initialised){

        #ifdef TARGET_LINUX
        pthread_mutexattr_t attr = {0};
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&s_heap_lock, &attr);
        #else
        s_heap_lock = xSemaphoreCreateRecursiveMutex();
        assert(s_heap_lock);
        #endif
    
        assert((s_arena = memman_get(VM_GC_ARENA_ID)));
        is_initialised = true;
    } else bumper_reset(s_arena);
}

int heap_array_type_size(JavaValueType_t type){
    return value_type_size[type];
}

Error_t heap_class_object_alloc(Class_t* class, Object_t** output){
    Error_t err = JERR_OK;
    heap_enter_critical();

    FAIL_SET_JUMP(class->flags.is_abstract == 0 && class->flags.is_interface == 0, err, JERR_TYPECHECK_FAILURE, exit);
    FAIL_SET_JUMP(class->flags.is_array == 0, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);

    if(sizeof(Object_t) + class->object_size > (bumper_size(s_arena) - bumper_used(s_arena))){
        heap_exit_critical();
        heap_gc_start();
        heap_enter_critical();
    }

    Object_t* object = NULL;
    FAIL_SET_JUMP((object = bumper_calloc(s_arena, 1, sizeof(*object) + class->object_size)), err, JERR_OOM, exit);

    INIT_LIST_HEAD(&object->list);
    object->class = class;
    object->forward = 0;
    object->ident = rand();

    *output = object;

exit:
    heap_exit_critical();
    return err;
}

Error_t heap_array_object_alloc(Class_t* class, int32_t length, Object_t** output){
    Error_t err = JERR_OK;
    heap_enter_critical();

    FAIL_SET_JUMP(class->flags.is_array == 1, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(length >= 0, err, JERR_BADPARAM, exit);

    if(sizeof(Object_t) + sizeof(int32_t) + value_type_size[class->array_type] * length > (bumper_size(s_arena) - bumper_used(s_arena))){
        heap_exit_critical();
        heap_gc_start();
        heap_enter_critical();
    }

    Object_t* object = NULL;
    FAIL_SET_JUMP((object = bumper_calloc(s_arena, 1, sizeof(*object) + sizeof(int32_t) + (value_type_size[class->array_type] * length))), err, JERR_OOM, exit);

    INIT_LIST_HEAD(&object->list);
    object->class = class;
    object->forward = 0;
    object->ident = rand();

    *(int32_t*)(((char*)object) + sizeof(*object)) = length;
    *output = object;

exit:
    heap_exit_critical();
    return err;
}

Error_t heap_class_object_get_fields(Object_t* object, int32_t** output){
    if(!object) return JERR_NULLPOINTER;

    *output = (int32_t*)((char*)object + sizeof(*object));
    return JERR_OK;
}

Error_t heap_array_object_get_length(Object_t* object, int32_t* output){
    if(!object) return JERR_NULLPOINTER;

    *output = *(int32_t*)(((char*)object) + sizeof(*object));
    return JERR_OK;
}

Error_t heap_array_object_get_elements(Object_t* object, void** output){
    if(!object) return JERR_NULLPOINTER;
    *output = (((char*)object) + sizeof(*object) + sizeof(int32_t));

    return JERR_OK;
}

uint32_t heap_object_get_hashcode(Object_t* object){
    return object->ident;
}

void heap_gc_thread_register(Thread_t* thread){
    list_del_init(&thread->gc_list);
    list_add(&thread->gc_list, &s_gc_thread_list);
}

void heap_gc_thread_unregister(Thread_t* thread){
    list_del_init(&thread->gc_list);
}

static void gc_scan_threads(struct list_head* output_list){
    Thread_t* thread = NULL;
    list_for_each_entry(thread, &s_gc_thread_list, gc_list){
        for(InterpreterFrame_t* cur = thread->interpreter.frame; cur; cur = cur->prev){
            for(unsigned i = 0; i < cur->sp; i++){
                if(SHADOW_GET_REF(cur->shadow_stack, i)){
                    Object_t* object = (Object_t*)cur->stack[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        object->forward = GC_MARK_SENTINEL;
                        INIT_LIST_HEAD(&object->list);
                        list_add_tail(&object->list, output_list);
                    }
                }
            }

            for(unsigned i = 0; i < ((MethodBytecode_t*)cur->method->code)->max_locals; i++){
                if(SHADOW_GET_REF(cur->shadow_locals, i)){
                    Object_t* object = (Object_t*)cur->locals[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        object->forward = GC_MARK_SENTINEL;
                        INIT_LIST_HEAD(&object->list);
                        list_add_tail(&object->list, output_list);
                    }
                }
            }

            Monitor_t* monitor = NULL;
            list_for_each_entry(monitor, &cur->held_monitors, list){
                Object_t* object = monitor->owner_object;
                if(object && object->forward != GC_MARK_SENTINEL){
                    object->forward = GC_MARK_SENTINEL;
                    INIT_LIST_HEAD(&object->list);
                    list_add_tail(&object->list, output_list);
                }                
            }
        }  
    }
}

extern struct list_head* classtable_entries_get();
static void gc_scan_classes(struct list_head* output_list){
    struct list_head* classtable_entries = classtable_entries_get();
    ClasstableEntry_t* entry = NULL;

    list_for_each_entry(entry, classtable_entries, list){
        for(unsigned i = 0; i < CLASSTABLE_ENTRY_ITEMS_COUNT; i++){
            Class_t* class = entry->items[i];
            if(class){
                Object_t* object = class->class_object;
                if(object && object->forward != GC_MARK_SENTINEL){
                    object->forward = GC_MARK_SENTINEL;
                    INIT_LIST_HEAD(&object->list);
                    list_add_tail(&object->list, output_list);
                }

                int32_t* storage = class->storage;
                for(unsigned i = 0; i < class->static_fields.count; i++){
                    Field_t* field = &class->static_fields.fields[i];
                    if(field->type == TYPE_REFERENCE){
                        Object_t* object = (Object_t*)storage[field->offset];
                        if(object && object->forward != GC_MARK_SENTINEL){
                            object->forward = GC_MARK_SENTINEL;
                            INIT_LIST_HEAD(&object->list);
                            list_add_tail(&object->list, output_list);
                        }                
                    }
                }                

                for(unsigned i = 0; i < class->symtab.count; i++){
                    ClassSymbol_t* sym = &class->symtab.symbols[i];
                    if(sym->type == SYMBOL_STRING){
                        Object_t* object = sym->value;
                        if(object && object->forward != GC_MARK_SENTINEL){
                            object->forward = GC_MARK_SENTINEL;
                            INIT_LIST_HEAD(&object->list);
                            list_add_tail(&object->list, output_list);                    
                        }
                    }
                }                
            }
        }
    }
}


/*
static void gc_scan_stringpool(struct list_head* output_list){
}
*/

static void gc_scan(){
    LIST_HEAD(root_list);

    gc_scan_classes(&root_list);
    gc_scan_threads(&root_list);
    //gc_scan_stringpool(&root_list);

    Object_t *object = NULL, *tmp = NULL;

    while(!list_empty(&root_list)){
        list_for_each_entry_safe(object, tmp, &root_list, list){
            list_del(&object->list);

            if(!object->class->flags.is_array){
                int32_t* storage = (int32_t*)((char*)object + sizeof(*object));
                for(Class_t* cur = object->class; cur; cur = cur->parent){
                    for(unsigned i = 0; i < cur->instance_fields.count; i++){
                        Field_t* field = &cur->instance_fields.fields[i];

                        if(field->type == TYPE_REFERENCE){
                            Object_t* found = (Object_t*)storage[field->offset];
                            if(found && found->forward != GC_MARK_SENTINEL){
                                found->forward = GC_MARK_SENTINEL;
                                INIT_LIST_HEAD(&found->list);
                                list_add_tail(&found->list, &root_list);
                            }
                        }
                    }
                }
            } else if(object->class->array_type == TYPE_REFERENCE){
                int32_t length = 0;
                Object_t** elements = NULL;

                assert(heap_array_object_get_length(object, &length) == JERR_OK);
                assert(heap_array_object_get_elements(object, (void**)&elements) == JERR_OK);

                for(unsigned i = 0; i < length; i++){
                    Object_t* found = elements[i];
                    if(found && found->forward != GC_MARK_SENTINEL){
                        found->forward = GC_MARK_SENTINEL;
                        INIT_LIST_HEAD(&found->list);
                        list_add_tail(&found->list, &root_list);
                    }                    
                }
            }
        }
    }
}

static void* gc_calculate_forwards(struct list_head* live_list){
    bump_allocator_t calculation_arena = *s_arena; //hack to make it 100% match with actual bumper logic in future(in case alligment is added)

    void* heap_end = calculation_arena.last_end;
    void* heap_start = calculation_arena.memory;
    void* scanner = heap_start;

    bumper_reset(&calculation_arena);

    while(scanner < heap_end){
        Object_t* object = scanner; //Can we trust that every data on heap arena is object?
        size_t object_size = object->class->flags.is_array ? (value_type_size[object->class->array_type] * (*(int32_t*)(((char*)object) + sizeof(*object)))) + sizeof(int32_t) + sizeof(*object) 
                                                           : object->class->object_size + sizeof(Object_t);

        if(object->forward == GC_MARK_SENTINEL){
            object->forward = bumper_alloc(&calculation_arena, object_size); //This DOES NOT modify heap contents, its not calloc!
            assert(object->forward && "WTF we even failed HERE???");

            INIT_LIST_HEAD(&object->list); //We need to resurect this list from the dead!
            list_add_tail(&object->list,live_list);
        } else {
            monitor_free(object);
            //TODO: call finalize
        }

        scanner += object_size;
    }

    return calculation_arena.last_end;
}

//Why not use live objects list while we already have it in object header?
static void gc_patch(struct list_head* live_list){

    //Thread patching phase ====
    Thread_t* thread = NULL;
    list_for_each_entry(thread, &s_gc_thread_list, gc_list){
        for(InterpreterFrame_t* cur = thread->interpreter.frame; cur; cur = cur->prev){
            for(unsigned i = 0; i < cur->sp; i++){
                if(SHADOW_GET_REF(cur->shadow_stack, i)){
                    Object_t* object = (Object_t*)cur->stack[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        cur->stack[i] = (int32_t)object->forward;
                    }
                }
            }

            for(unsigned i = 0; i < ((MethodBytecode_t*)cur->method->code)->max_locals; i++){
                if(SHADOW_GET_REF(cur->shadow_locals, i)){
                    Object_t* object = (Object_t*)cur->locals[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        cur->locals[i] = (int32_t)object->forward;
                    }
                }
            }

            Monitor_t* monitor = NULL;
            list_for_each_entry(monitor, &cur->held_monitors, list){
                Object_t* object = monitor->owner_object;
                if(object && object->forward != GC_MARK_SENTINEL){
                    monitor->owner_object = object->forward;
                }                
            }
        }
    } 
    //====================================
    
    //Class patching phase ============
    struct list_head* classtable_entries = classtable_entries_get();
    ClasstableEntry_t* entry = NULL;

    list_for_each_entry(entry, classtable_entries, list){
        for(unsigned i = 0; i < CLASSTABLE_ENTRY_ITEMS_COUNT; i++){
            Class_t* class = entry->items[i];
            if(class){
                Object_t* object = class->class_object;
                if(object && object->forward != GC_MARK_SENTINEL){
                    class->class_object = object->forward;
                }

                int32_t* storage = class->storage;
                for(unsigned i = 0; i < class->static_fields.count; i++){
                    Field_t* field = &class->static_fields.fields[i];
                    if(field->type == TYPE_REFERENCE){
                        Object_t* object = (Object_t*)storage[field->offset];
                        if(object && object->forward != GC_MARK_SENTINEL){
                            storage[field->offset] = (int32_t)object->forward;
                        }                
                    }
                }

                for(unsigned i = 0; i < class->symtab.count; i++){
                    ClassSymbol_t* sym = &class->symtab.symbols[i];
                    if(sym->type == SYMBOL_STRING){
                        Object_t* object = sym->value;
                        if(object && object->forward != GC_MARK_SENTINEL){
                            sym->value = object->forward;                
                        }
                    }
                }
            }
        }
    }
    //=============================

    //Object patching phase
    Object_t *object = NULL, *tmp = NULL;
    list_for_each_entry_safe(object, tmp, live_list, list){
        if(!object->class->flags.is_array){
            int32_t* storage = (int32_t*)((char*)object + sizeof(*object));
            for(Class_t* cur = object->class; cur; cur = cur->parent){
                for(unsigned i = 0; i < cur->instance_fields.count; i++){
                    Field_t* field = &cur->instance_fields.fields[i];

                    if(field->type == TYPE_REFERENCE){
                        Object_t* found = (Object_t*)storage[field->offset];
                        if(found && found->forward != GC_MARK_SENTINEL){
                            storage[field->offset] = (int32_t)found->forward;
                        }
                    }
                }
            }
        } else if(object->class->array_type == TYPE_REFERENCE){
            int32_t length = 0;
            Object_t** elements = NULL;

            assert(heap_array_object_get_length(object, &length) == JERR_OK);
            assert(heap_array_object_get_elements(object, (void**)&elements) == JERR_OK);

            for(unsigned i = 0; i < length; i++){
                Object_t* found = elements[i];
                if(found && found->forward != GC_MARK_SENTINEL){
                    elements[i] = found->forward;
                }                    
            }
        }
    }
    //=====================

    //Patch stringpool
    /*
    JavaStringPoolEntry_t* jstringpool = jstringpool_get_pool();
    for(unsigned i = 0; i < JAVASTRINGPOOL_SIZE; i++){
        JavaStringPoolEntry_t* entry = &jstringpool[i];

        Object_t* object = entry->object;
        if(object && object->forward != GC_MARK_SENTINEL){
            entry->object = object->forward;
        }           
    }
    */
    //=====================
}

static void gc_move(struct list_head* live_list){
    Object_t *object = NULL, *tmp = NULL;
    list_for_each_entry_safe(object, tmp, live_list, list){
        list_del_init(&object->list);
        size_t object_size = object->class->flags.is_array ? (value_type_size[object->class->array_type] * (*(int32_t*)(((char*)object) + sizeof(*object)))) + sizeof(int32_t) + sizeof(*object) 
                                                           : object->class->object_size + sizeof(Object_t);

        void* move_to = (void*)object->forward;
        object->forward = 0;

        memmove(move_to, object, object_size);
    }

    memset(s_arena->last_end, 0, bumper_size(s_arena) - bumper_used(s_arena));
}

extern void thread_notify_send(Thread_t* thread);
void heap_gc_start(){
    //thread_safepoint_check();
    thread_safepoint_request();
    heap_enter_critical();

    gc_scan();

    LIST_HEAD(live_list);
    s_arena->last_end = gc_calculate_forwards(&live_list);
    gc_patch(&live_list);
    gc_move(&live_list);

    heap_exit_critical();
    thread_safepoint_release();
}