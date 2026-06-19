#include "heap.h"
#include "bumper.h"
#include "class.h"
#include "config.h"
#include "jerror.h"
#include "list.h"
#include "thread.h"

#include <stdlib.h>
#include <assert.h>

static struct list_head s_gc_thread_list;
static bump_allocator_t s_gc_heap = {0};
static bool is_initialised = false;

void heap_init(){
    INIT_LIST_HEAD(&s_gc_thread_list);
    
    if(!is_initialised){
        assert(bumper_create(&s_gc_heap, OBJECT_HEAP_SIZE) == 0);
        is_initialised = true;
    } else bumper_reset(&s_gc_heap);
}

Error_t heap_class_object_alloc(Class_t* class, int32_t* output){
    Error_t err = JERR_OK;

    FAIL_SET_JUMP(class->flags.is_abstract == 0 && class->flags.is_interface == 0, err, JERR_TYPECHECK_FAILURE, exit);
    FAIL_SET_JUMP(class->flags.is_array == 0, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);

    if((sizeof(Object_t) + (class->object_size * sizeof(int32_t))) > (bumper_size(&s_gc_heap) - bumper_used(&s_gc_heap))){
        heap_gc_start();
    }

    Object_t* object = NULL;
    FAIL_SET_JUMP((object = bumper_calloc(&s_gc_heap, 1, sizeof(*object))), err, JERR_OOM, exit);
    FAIL_SET_JUMP(bumper_calloc(&s_gc_heap, class->object_size, sizeof(int32_t)), err, JERR_UNKNOWN, exit); //Looks wrong, but works properly because this is the simple bumper allocator + no real concurency

    object->class = class;
    object->forward = 0;
    INIT_LIST_HEAD(&object->list);

    *output = (int32_t)object;

exit:
    return err;
}

Error_t heap_class_object_get_fields(Object_t* object, int32_t** output){
    if(object->class->flags.is_array) return JERR_TYPECHECK_FAILURE;

    *output = (int32_t*)((char*)object + sizeof(*object));
    return JERR_OK;
}

Error_t heap_array_object_alloc(Class_t* class, int32_t count, int32_t* output){
    assert(0 && "Unimplemented");

    Error_t err = JERR_OK;
    FAIL_SET_JUMP(class->flags.is_array == 1, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(count >= 0, err, JERR_BADPARAM, exit);

exit:
    return err;
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
        for(CallFrame_t* cur = thread->top_frame; cur; cur = cur->prev){
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
        }
    }
}

extern struct list_head* classes_get_all();
static void gc_scan_classes(struct list_head* output_list){
    struct list_head* class_list = classes_get_all();

    Class_t* class = NULL;
    list_for_each_entry(class, class_list, list){
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
    }
}

static void gc_scan(){
    LIST_HEAD(root_list);

    gc_scan_classes(&root_list);
    gc_scan_threads(&root_list);

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
            } else assert(0 && "TODO:");
        }
    }
}

static void* gc_calculate_forwards(struct list_head* live_list){
    bump_allocator_t calculation_arena = s_gc_heap; //hack to make it 100% match with actual bumper logic in future(in case alligment is added)
    
    void* heap_end = calculation_arena.last_end;
    void* heap_start = calculation_arena.memory;
    void* scanner = heap_start;

    bumper_reset(&calculation_arena);

    while(scanner < heap_end){
        Object_t* object = scanner; //Can we trust that every data on heap arena is object?
        size_t object_size = (object->class->flags.is_array ? assert(0 && "TODO: array support"), 0 : (object->class->object_size * sizeof(int32_t))) + sizeof(Object_t);

        if(object->forward == 4152828064){
            printf("BREAK NOW!\n");
        }
        if(object->forward == GC_MARK_SENTINEL){
            object->forward = (uint32_t)bumper_alloc(&calculation_arena, object_size); //This DOES NOT modify heap contents, its not calloc!
            assert(object->forward && "WTF we even failed HERE???");

            INIT_LIST_HEAD(&object->list); //We need to resurect this list from the dead!
            list_add_tail(&object->list,live_list);
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
        for(CallFrame_t* cur = thread->top_frame; cur; cur = cur->prev){
            for(unsigned i = 0; i < cur->sp; i++){
                if(SHADOW_GET_REF(cur->shadow_stack, i)){
                    Object_t* object = (Object_t*)cur->stack[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        cur->stack[i] = object->forward;
                    }
                }
            }

            for(unsigned i = 0; i < ((MethodBytecode_t*)cur->method->code)->max_locals; i++){
                if(SHADOW_GET_REF(cur->shadow_locals, i)){
                    Object_t* object = (Object_t*)cur->locals[i];
                    if(object && object->forward != GC_MARK_SENTINEL){
                        cur->locals[i] = object->forward;
                    }
                }
            }
        }
    } 
    //====================================
    
    //Class patching phase ============
    struct list_head* class_list = classes_get_all();

    Class_t* class = NULL;
    list_for_each_entry(class, class_list, list){
        int32_t* storage = class->storage;
        for(unsigned i = 0; i < class->static_fields.count; i++){
            Field_t* field = &class->static_fields.fields[i];
            if(field->type == TYPE_REFERENCE){
                Object_t* object = (Object_t*)storage[field->offset];
                if(object && object->forward != GC_MARK_SENTINEL){
                    storage[field->offset] = object->forward;
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
                            storage[field->offset] = found->forward;
                        }
                    }
                }
            }
        } else assert(0 && "TODO:");
    }
    //=====================
}

#include <string.h>
static int gc_iterations = 0;
static void gc_move(struct list_head* live_list){
    Object_t *object = NULL, *tmp = NULL;
    list_for_each_entry_safe(object, tmp, live_list, list){
        list_del_init(&object->list);
        size_t object_size = (object->class->flags.is_array ? assert(0 && "TODO: array support"), 0 : (object->class->object_size * sizeof(int32_t))) + sizeof(Object_t);

        void* move_to = (void*)object->forward;
        object->forward = 0;

        memmove(move_to, object, object_size);
    }

    memset(s_gc_heap.last_end, 0, bumper_size(&s_gc_heap) - bumper_used(&s_gc_heap));
}

void heap_gc_start(){
    gc_scan();

    LIST_HEAD(live_list);
    s_gc_heap.last_end = gc_calculate_forwards(&live_list);
    gc_patch(&live_list);
    gc_move(&live_list);

    gc_iterations++;
}