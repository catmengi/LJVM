#include "thread.h"
#include "bumper.h"
#include "class.h"
#include "config.h"
#include "heap.h"
#include "jerror.h"
#include "list.h"
#include "native_methods_service.h"
#include "parser.h"
#include "opcodes.h"
#include "lb_endian.h"
#include "stringpool.h"
#include "monitor.h"


#include <assert.h>
#include <string.h>
#include <math.h>


static struct list_head s_active_threads;
static struct list_head s_free_threads;

static Thread_t s_threads[THREAD_MAX_COUNT] = {0};
static size_t s_active_threads_count = 0;

CallFrame_t* thread_frame_push(Thread_t* thread, Method_t* method);
CallFrame_t* thread_frame_pop(Thread_t* thread);

void threads_init(){
    s_active_threads_count = 0;

    INIT_LIST_HEAD(&s_active_threads);
    INIT_LIST_HEAD(&s_free_threads);

    for(unsigned i = 0; i < THREAD_MAX_COUNT; i++){
        Thread_t* thread = &s_threads[i];

        thread->top_frame = NULL;
        bumper_create_from(&thread->frame_allocator, thread->stackbuf, THREAD_STACK_SIZE);

        INIT_LIST_HEAD(&thread->list);
        list_add(&thread->list, &s_free_threads);
    }
}

struct list_head* threads_get_schedule_list(){
    return &s_active_threads;
}

Thread_t* thread_alloc(){
    Thread_t* inactive = NULL;
    list_for_each_entry(inactive, &s_free_threads, list) break;

    if(inactive){
        INIT_LIST_HEAD(&inactive->joiners);
        INIT_LIST_HEAD(&inactive->gc_list);

        list_del(&inactive->list);

        memset(inactive->stackbuf, 0, sizeof(inactive->stackbuf));
        bumper_reset(&inactive->frame_allocator);
    }

    return inactive;
}

void thread_start(Thread_t* thread, Method_t* method, int32_t* args){
    thread->state = THREAD_ACTIVE;
    thread->opcode_quota = THREAD_LOWEST_QUOTA * THREAD_DEFAULT_PRIORITY;
    
    CallFrame_t* base_frame = thread_frame_push(thread, method);
    memcpy(base_frame->locals, args, method->args_slots * sizeof(int32_t));

    heap_gc_thread_register(thread);

    INIT_LIST_HEAD(&thread->list);
    list_add(&thread->list, &s_active_threads);

    s_active_threads_count++;
}

void thread_kill(Thread_t* thread){
    Thread_t* wakeup = NULL, *tmp = NULL;
    list_for_each_entry_safe(wakeup, tmp, &thread->joiners, list){
        list_del_init(&wakeup->list);
        list_add(&wakeup->list, &s_active_threads);

        s_active_threads_count++;
    }

    s_active_threads_count--;

    heap_gc_thread_unregister(thread);
    list_del_init(&thread->list);
    list_add(&thread->list, &s_free_threads);    
}

CallFrame_t* thread_frame_push(Thread_t* thread, Method_t* method){
    assert(!method->flags.is_native);
    
    MethodBytecode_t* bytecode = method->code;
    size_t frame_size = (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + sizeof(CallFrame_t) + (((bytecode->max_stack + 31) / 32) * sizeof(int32_t)) + (((bytecode->max_locals + 31) / 32) * sizeof(int32_t));

    char* frame_memory = bumper_calloc(&thread->frame_allocator, 1, frame_size);

    if(frame_memory){
        CallFrame_t* frame = (void*)frame_memory;
        INIT_LIST_HEAD(&frame->held_monitors);

        frame->frame_size = frame_size;
        frame->locals = (void*)(frame_memory + sizeof(CallFrame_t));
        frame->stack = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + sizeof(CallFrame_t));
        frame->shadow_locals = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + sizeof(CallFrame_t));
        frame->shadow_stack = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + (((bytecode->max_locals + 31) / 32) * sizeof(int32_t)) + sizeof(CallFrame_t));

        frame->pc = bytecode->code;
        frame->method = method;
        frame->sp = 0;

        frame->prev = thread->top_frame;
        thread->top_frame = frame;

        memcpy(frame->shadow_locals, method->args_bitmap, method->args_bitmap_size * sizeof(uint32_t));
        return frame;
    }

    return NULL;
}

CallFrame_t* thread_frame_get(Thread_t* thread){
    return thread->top_frame;
}

CallFrame_t* thread_frame_pop(Thread_t* thread){
    if(thread->top_frame){
        CallFrame_t* frame = thread->top_frame;
        thread->top_frame = frame->prev;

        bumper_unwind(&thread->frame_allocator, frame->frame_size);
        return thread->top_frame;
    }
    return NULL;
}

//Will not work from any code which execution was triggered by java_method_invoke
void thread_join(Thread_t* thread, Thread_t* join_to){
    list_del_init(&thread->list);
    list_add(&thread->list, &join_to->joiners);
}


//Will not work from any code which execution was triggered by java_method_invoke
void thread_sleep(Thread_t* thread, uint32_t ms){
    assert(0 && "TODO: sleep");
}

static Error_t interpret_bytecode(Thread_t* thread);

//NOTE: this method is basically an infinite loop until all threads die!
Error_t thread_schedule(){
    Error_t err = JERR_OK;

    while(s_active_threads_count){
        Thread_t *thread = NULL, *tmp = NULL;
        list_for_each_entry_safe(thread, tmp, &s_active_threads, list){
            switch(thread->state){
                case THREAD_ACTIVE:{
                    err = interpret_bytecode(thread);
                    FAIL_SET_JUMP(err == JERR_OK || err == JERR_SCHEDULE, err, err, exit);

                    if(err != JERR_SCHEDULE) thread_kill(thread);
                }
                break;

                case THREAD_BLOCKED_SLEEP:{
                    assert(0 && "TODO: THREAD_BLOCKED_SLEEP");
                }
                break;

                default: return JERR_UNKNOWN;
            }
        }
    }

exit:
    return err;
}

// 32-bit stack operations
#define STACK_PUSH_INT(frame, value) do { \
    (frame)->stack[(frame)->sp] = (value); \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
} while(0)

// Similarly for STACK_PUSH_FLOAT, STACK_PUSH_REF, STACK_PUSH_LONG, STACK_PUSH_DOUBLE.

#define STACK_PUSH_FLOAT(frame, value) ({ \
    union { float f; int32_t i; } _u = { .f = (value) }; \
    (frame)->stack[(frame)->sp] = _u.i; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
})

#define STACK_PUSH_REF(frame, value) ({ \
    (frame)->stack[(frame)->sp] = (int32_t)(uintptr_t)(value); \
    SHADOW_SET_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
})

#define STACK_POP_INT(frame) ({ \
    if ((frame)->sp == 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->stack[(frame)->sp]; \
})

#define STACK_POP_FLOAT(frame) ({ \
    if ((frame)->sp == 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    union { int32_t i; float f; } _u = { .i = (frame)->stack[(frame)->sp] }; \
    _u.f; \
})

#define STACK_POP_REF(frame) ({ \
    if ((frame)->sp == 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 1) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (void*)(uintptr_t)(frame)->stack[(frame)->sp]; \
})

// 64-bit stack operations
#define STACK_PUSH_LONG(frame, value) ({ \
    uint64_t _v = (value); \
    uint32_t _high = (uint32_t)(_v >> 32); \
    uint32_t _low  = (uint32_t)_v; \
    (frame)->stack[(frame)->sp] = _high; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
    (frame)->stack[(frame)->sp] = _low; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
})

#define STACK_PUSH_DOUBLE(frame, value) ({ \
    union { double d; uint64_t u; } _u = { .d = (value) }; \
    STACK_PUSH_LONG(frame, _u.u); \
})

#define STACK_POP_LONG(frame) ({ \
    if ((frame)->sp < 2) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    uint32_t _low = (frame)->stack[(frame)->sp]; \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    uint32_t _high = (frame)->stack[(frame)->sp]; \
    ((uint64_t)_high << 32) | _low; \
})

#define STACK_POP_DOUBLE(frame) ({ \
    uint64_t _u = STACK_POP_LONG(frame); \
    union { uint64_t u; double d; } _conv = { .u = _u }; \
    _conv.d; \
})

// Local variable stores
#define LOCAL_STORE_INT(frame, value, idx) ({ \
    (frame)->locals[idx] = (value); \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx); \
})

#define LOCAL_STORE_FLOAT(frame, value, idx) ({ \
    union { float f; int32_t i; } _u = { .f = (value) }; \
    (frame)->locals[idx] = _u.i; \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx); \
})

#define LOCAL_STORE_REF(frame, value, idx) ({ \
    (frame)->locals[idx] = (int32_t)(uintptr_t)(value); \
    SHADOW_SET_REF((frame)->shadow_locals, idx); \
})

#define LOCAL_STORE_LONG(frame, value, idx) ({ \
    uint64_t _v = (value); \
    uint32_t _high = (uint32_t)(_v >> 32); \
    uint32_t _low  = (uint32_t)_v; \
    (frame)->locals[idx] = _high; \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx); \
    (frame)->locals[idx + 1] = _low; \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx + 1); \
})

#define LOCAL_STORE_DOUBLE(frame, value, idx) ({ \
    union { double d; uint64_t u; } _u = { .d = (value) }; \
    LOCAL_STORE_LONG(frame, _u.u, idx); \
})

// Local variable loads
#define LOCAL_LOAD_INT(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->locals[idx]; \
})

#define LOCAL_LOAD_FLOAT(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    union { int32_t i; float f; } _u = { .i = (frame)->locals[idx] }; \
    _u.f; \
})

#define LOCAL_LOAD_REF(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 1) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (void*)(uintptr_t)(frame)->locals[idx]; \
})

#define LOCAL_LOAD_LONG(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 0 || \
        SHADOW_GET_REF((frame)->shadow_locals, idx + 1) != 0) { \
        err = JERR_TYPECHECK_FAILURE; goto exit; \
    } \
    uint32_t _high = (frame)->locals[idx]; \
    uint32_t _low  = (frame)->locals[idx + 1]; \
    ((uint64_t)_high << 32) | _low; \
})

#define LOCAL_LOAD_DOUBLE(frame, idx) ({ \
    uint64_t _u = LOCAL_LOAD_LONG(frame, idx); \
    union { uint64_t u; double d; } _conv = { .u = _u }; \
    _conv.d; \
})

#define STACK_PUSH_GENERIC(frame, type, value)\
({switch(type){\
        case TYPE_INT:\
        case TYPE_CHAR:\
        case TYPE_SHORT:\
        case TYPE_BOOL:\
        case TYPE_BYTE:\
            STACK_PUSH_INT(frame, *(int32_t*)value);\
            break;\
        case TYPE_REFERENCE:\
            STACK_PUSH_REF(frame, *(void**)value);\
            break;\
        case TYPE_FLOAT:\
            STACK_PUSH_FLOAT(frame, *(float*)value);\
            break;    \
        case TYPE_DOUBLE:\
            STACK_PUSH_DOUBLE(frame, *(double*)value);\
            break;        \
        case TYPE_LONG:\
            STACK_PUSH_LONG(frame, *(int64_t*)value);\
            break;\
        default: break;\
}})

#define STACK_POP_GENERIC(frame, type, value)({    \
    switch(type){\
        case TYPE_INT:\
        case TYPE_BOOL:\
        case TYPE_BYTE:\
        case TYPE_SHORT:\
        case TYPE_CHAR:\
            *(int32_t*)value = STACK_POP_INT(frame);\
            break;\
        case TYPE_REFERENCE:\
            *(void**)value = STACK_POP_REF(frame);\
            break;\
        case TYPE_FLOAT:\
            *(float*)value = STACK_POP_FLOAT(frame);\
            break;\
        case TYPE_LONG:\
            *(int64_t*)value = STACK_POP_LONG(frame);\
            break;\
        case TYPE_DOUBLE:\
            *(double*)value = STACK_POP_DOUBLE(frame);\
            break;\
        default: break;\
}})

static Error_t native_method_invoke(Thread_t* thread, CallFrame_t* frame, Method_t* method){
    Error_t err = JERR_OK;

    int32_t* args = &frame->stack[frame->sp -= method->args_slots];

    NativeMethod_t native = natives_find(stringpool_get(method->class->name_id), stringpool_get(method->name_id));
    FAIL_SET_JUMP(native, err, JERR_NOTFOUND, exit);

    NativeMethodReturnValue_t retval = native(thread,method,args);
    FAIL_SET_JUMP(retval.err == JERR_OK, err, retval.err, exit);

    STACK_PUSH_GENERIC(frame, method->return_type, retval.value);

exit:
    if(err == JERR_EXCEPTION){
        return thread_throw_exception(thread, *(Object_t**)retval.value);
    }
    return err;
}

static bool interpreter_check_arguments(Method_t* method, uint32_t* shadow_stack, uint32_t sp){
    for(unsigned i = 0; i < method->args_slots; i++){
        if(SHADOW_GET_REF(shadow_stack, sp + i) != SHADOW_GET_REF(method->args_bitmap, i)) return false;
    }

    return true;
}


Error_t java_method_invoke(Method_t* method, int32_t* arguments, void* return_value){
    Error_t err = JERR_OK;
    Thread_t thread = {0};
    
    FAIL_SET_JUMP(method && (arguments || method->args_slots == 0) && (return_value || method->return_type == TYPE_VOID), err, JERR_BADPARAM, exit);

    thread.state = THREAD_PSEUDO;
    INIT_LIST_HEAD(&thread.list);
    INIT_LIST_HEAD(&thread.joiners);
    INIT_LIST_HEAD(&thread.gc_list);

    thread.opcode_quota = 0;
    bumper_create_from(&thread.frame_allocator,thread.stackbuf, THREAD_STACK_SIZE);

    heap_gc_thread_register(&thread);

    CallFrame_t* retstub = thread_frame_push(&thread, &(Method_t){.code = 
                                                &(MethodBytecode_t){.code_length = 2,
                                                                    .code = (uint8_t[2]){EJOPCODE_RETURN, EJOPCODE_RETURN},
                                                                    .max_stack = 2 + method->flags.is_native ? method->args_slots : 0,
                                                                   }, .args_bitmap_size = 0});
    if(method->flags.is_native){
       assert(0 && "TODO:");
    } else {
        CallFrame_t* frame = thread_frame_push(&thread, method);
        memcpy(frame->locals, arguments, method->args_slots * sizeof(int32_t));

        while((err = interpret_bytecode(&thread)) == JERR_SCHEDULE) {;}
        FAIL_SET_JUMP(err == JERR_OK, err, err, exit);
    }
    STACK_POP_GENERIC(retstub, method->return_type, return_value);
    heap_gc_thread_unregister(&thread);

exit:
    return err;
}


Error_t thread_throw_exception(Thread_t* thread, Object_t* exception_object){
    size_t unwind_by = 0;
    for(CallFrame_t* frame = thread->top_frame; frame; frame = frame->prev, thread->top_frame = frame){
        MethodBytecode_t* bytecode = frame->method->code;
        assert(frame == thread->top_frame);

        for(unsigned i = 0; i < bytecode->exception_count; i++){
            MethodExceptionHandler_t* exception = &bytecode->exceptions[i];
            if(exception->start_pc + bytecode->code <= frame->pc && exception->end_pc + bytecode->code > frame->pc){
                ClassSymbol_t* exception_type_symbol = exception->type;

                if(exception->type != NULL){
                    Error_t resolv_error = class_resolv_symbol(exception_type_symbol);
                    if(resolv_error != JERR_OK || exception_type_symbol->type != SYMBOL_CLASS) continue; //Java spec blah blah
                }

                if(exception->type == NULL || class_is_compatible(exception_object->class,exception_type_symbol->value)){
                    bumper_unwind(&thread->frame_allocator, unwind_by);
                    //TODO: monitor  support

                    frame->pc = bytecode->code + exception->handler_pc;
                    frame->sp = 0;
                    STACK_PUSH_REF(frame, exception_object);

                    return JERR_SCHEDULE;
                }
            }
        }

        unwind_by += frame->frame_size;
    }

    return JERR_UNHANDLED_EXCEPTION;
}

static Error_t interpret_bytecode(Thread_t* thread) {
    Error_t err = JERR_OK;
    size_t opcodes_executed = 0;
    CallFrame_t* frame = thread_frame_get(thread);

    static const JavaValueType_t sym_to_value_type[] = {
        [SYMBOL_INT] = TYPE_INT,
        [SYMBOL_FLOAT] = TYPE_FLOAT,
        [SYMBOL_LONG] = TYPE_LONG,
        [SYMBOL_DOUBLE] = TYPE_DOUBLE,
        [SYMBOL_STRING] = TYPE_REFERENCE,
        [SYMBOL_CLASS] = TYPE_REFERENCE,
    };

    void* opcode_labels[256] = {
        // Loads (explicit per type)
        [EJOPCODE_ILOAD]   = &&EJOPCODE_ILOAD,
        [EJOPCODE_ILOAD_0] = &&EJOPCODE_ILOAD_0,
        [EJOPCODE_ILOAD_1] = &&EJOPCODE_ILOAD_1,
        [EJOPCODE_ILOAD_2] = &&EJOPCODE_ILOAD_2,
        [EJOPCODE_ILOAD_3] = &&EJOPCODE_ILOAD_3,

        [EJOPCODE_FLOAD]   = &&EJOPCODE_FLOAD,
        [EJOPCODE_FLOAD_0] = &&EJOPCODE_FLOAD_0,
        [EJOPCODE_FLOAD_1] = &&EJOPCODE_FLOAD_1,
        [EJOPCODE_FLOAD_2] = &&EJOPCODE_FLOAD_2,
        [EJOPCODE_FLOAD_3] = &&EJOPCODE_FLOAD_3,

        [EJOPCODE_ALOAD]   = &&EJOPCODE_ALOAD,
        [EJOPCODE_ALOAD_0] = &&EJOPCODE_ALOAD_0,
        [EJOPCODE_ALOAD_1] = &&EJOPCODE_ALOAD_1,
        [EJOPCODE_ALOAD_2] = &&EJOPCODE_ALOAD_2,
        [EJOPCODE_ALOAD_3] = &&EJOPCODE_ALOAD_3,
        [EJOPCODE_ACONST_NULL] = &&EJOPCODE_ACONST_NULL,

        // Stores (explicit per type)
        [EJOPCODE_ISTORE]   = &&EJOPCODE_ISTORE,
        [EJOPCODE_ISTORE_0] = &&EJOPCODE_ISTORE_0,
        [EJOPCODE_ISTORE_1] = &&EJOPCODE_ISTORE_1,
        [EJOPCODE_ISTORE_2] = &&EJOPCODE_ISTORE_2,
        [EJOPCODE_ISTORE_3] = &&EJOPCODE_ISTORE_3,

        [EJOPCODE_FSTORE]   = &&EJOPCODE_FSTORE,
        [EJOPCODE_FSTORE_0] = &&EJOPCODE_FSTORE_0,
        [EJOPCODE_FSTORE_1] = &&EJOPCODE_FSTORE_1,
        [EJOPCODE_FSTORE_2] = &&EJOPCODE_FSTORE_2,
        [EJOPCODE_FSTORE_3] = &&EJOPCODE_FSTORE_3,

        [EJOPCODE_ASTORE]   = &&EJOPCODE_ASTORE,
        [EJOPCODE_ASTORE_0] = &&EJOPCODE_ASTORE_0,
        [EJOPCODE_ASTORE_1] = &&EJOPCODE_ASTORE_1,
        [EJOPCODE_ASTORE_2] = &&EJOPCODE_ASTORE_2,
        [EJOPCODE_ASTORE_3] = &&EJOPCODE_ASTORE_3,

        // Constants
        [EJOPCODE_ICONST_0]   = &&EJOPCODE_ICONST_0,
        [EJOPCODE_ICONST_1]   = &&EJOPCODE_ICONST_1,
        [EJOPCODE_ICONST_2]   = &&EJOPCODE_ICONST_2,
        [EJOPCODE_ICONST_3]   = &&EJOPCODE_ICONST_3,
        [EJOPCODE_ICONST_4]   = &&EJOPCODE_ICONST_4,
        [EJOPCODE_ICONST_5]   = &&EJOPCODE_ICONST_5,
        [EJOPCODE_ICONST_M1]  = &&EJOPCODE_ICONST_M1,

        [EJOPCODE_PUTSTATIC]  = &&EJOPCODE_PUTSTATIC,
        [EJOPCODE_GETSTATIC]  = &&EJOPCODE_GETSTATIC,
        [EJOPCODE_PUTFIELD] = &&EJOPCODE_PUTFIELD,
        [EJOPCODE_GETFIELD] = &&EJOPCODE_GETFIELD,

        [EJOPCODE_BIPUSH]     = &&EJOPCODE_BIPUSH,
        [EJOPCODE_SIPUSH]     = &&EJOPCODE_SIPUSH,

        [EJOPCODE_RETURN]     = &&EJOPCODE_RETURN,
        [EJOPCODE_IRETURN]    = &&EJOPCODE_IRETURN,
        [EJOPCODE_FRETURN]    = &&EJOPCODE_FRETURN,
        [EJOPCODE_LRETURN]    = &&EJOPCODE_LRETURN,
        [EJOPCODE_DRETURN]    = &&EJOPCODE_DRETURN,
        [EJOPCODE_ARETURN]    = &&EJOPCODE_ARETURN,

        [EJOPCODE_IF_ICMPEQ]  = &&EJOPCODE_IF_ICMPEQ,
        [EJOPCODE_IF_ICMPNE]  = &&EJOPCODE_IF_ICMPNE,
        [EJOPCODE_IF_ICMPGE]  = &&EJOPCODE_IF_ICMPGE,
        [EJOPCODE_IF_ICMPGT]  = &&EJOPCODE_IF_ICMPGT,
        [EJOPCODE_IF_ICMPLE]  = &&EJOPCODE_IF_ICMPLE,
        [EJOPCODE_IF_ICMPLT]  = &&EJOPCODE_IF_ICMPLT,
        [EJOPCODE_IFNULL] = &&EJOPCODE_IFNULL,
        [EJOPCODE_IFNONNULL] = &&EJOPCODE_IFNONNULL,

        [EJOPCODE_IADD]       = &&EJOPCODE_IADD,
        [EJOPCODE_ISUB]       = &&EJOPCODE_ISUB,
        [EJOPCODE_IMUL]       = &&EJOPCODE_IMUL,
        [EJOPCODE_IDIV]       = &&EJOPCODE_IDIV,
        [EJOPCODE_IREM]       = &&EJOPCODE_IREM,

        [EJOPCODE_LADD]       = &&EJOPCODE_LADD,
        [EJOPCODE_LSUB]       = &&EJOPCODE_LSUB,
        [EJOPCODE_LMUL]       = &&EJOPCODE_LMUL,
        [EJOPCODE_LDIV]       = &&EJOPCODE_LDIV,
        [EJOPCODE_LREM]       = &&EJOPCODE_LREM,

        [EJOPCODE_FADD]       = &&EJOPCODE_FADD,
        [EJOPCODE_FSUB]       = &&EJOPCODE_FSUB,
        [EJOPCODE_FMUL]       = &&EJOPCODE_FMUL,
        [EJOPCODE_FDIV]       = &&EJOPCODE_FDIV,
        [EJOPCODE_FREM]       = &&EJOPCODE_FREM,

        [EJOPCODE_DADD]       = &&EJOPCODE_DADD,
        [EJOPCODE_DSUB]       = &&EJOPCODE_DSUB,
        [EJOPCODE_DMUL]       = &&EJOPCODE_DMUL,
        [EJOPCODE_DDIV]       = &&EJOPCODE_DDIV,
        [EJOPCODE_DREM]       = &&EJOPCODE_DREM,

        [EJOPCODE_IINC]       = &&EJOPCODE_IINC,
        [EJOPCODE_GOTO]       = &&EJOPCODE_GOTO,
        [EJOPCODE_JSR]        = &&EJOPCODE_JSR,
        [EJOPCODE_JSR_W]      = &&EJOPCODE_JSR_W,
        [EJOPCODE_RET]        = &&EJOPCODE_RET,

        [EJOPCODE_INVOKESTATIC]  = &&EJOPCODE_INVOKESTATIC,
        [EJOPCODE_INVOKEVIRTUAL] = &&EJOPCODE_INVOKEVIRTUAL,
        [EJOPCODE_INVOKESPECIAL] = &&EJOPCODE_INVOKESPECIAL,
        [EJOPCODE_INVOKEINTERFACE] = &&EJOPCODE_INVOKEINTERFACE,
        [EJOPCODE_DUP] = &&EJOPCODE_DUP,
        [EJOPCODE_NEW] = &&EJOPCODE_NEW,
        [EJOPCODE_NEWARRAY] = &&EJOPCODE_NEWARRAY,
        [EJOPCODE_LDC] = &&EJOPCODE_LDC,
        [EJOPCODE_LDC2_W] = &&EJOPCODE_LDC2_W,
        [EJOPCODE_LDC_W] = &&EJOPCODE_LDC_W,
        [EJOPCODE_POP] = &&EJOPCODE_POP,

        [EJOPCODE_ARRAYLENGTH] = &&EJOPCODE_ARRAYLENGTH,
        [EJOPCODE_IALOAD] = &&EJOPCODE_IALOAD,
        [EJOPCODE_FALOAD] = &&EJOPCODE_FALOAD,
        [EJOPCODE_BALOAD] = &&EJOPCODE_BALOAD,
        [EJOPCODE_CALOAD] = &&EJOPCODE_CALOAD,
        [EJOPCODE_SALOAD] = &&EJOPCODE_SALOAD,
        [EJOPCODE_LALOAD] = &&EJOPCODE_LALOAD,
        [EJOPCODE_DALOAD] = &&EJOPCODE_DALOAD,
        [EJOPCODE_AALOAD] = &&EJOPCODE_AALOAD,
    
        [EJOPCODE_IASTORE] = &&EJOPCODE_IASTORE,
        [EJOPCODE_FASTORE] = &&EJOPCODE_FASTORE,
        [EJOPCODE_BASTORE] = &&EJOPCODE_BASTORE,
        [EJOPCODE_CASTORE] = &&EJOPCODE_CASTORE,
        [EJOPCODE_SASTORE] = &&EJOPCODE_SASTORE,
        [EJOPCODE_LASTORE] = &&EJOPCODE_LASTORE,
        [EJOPCODE_DASTORE] = &&EJOPCODE_DASTORE,
        [EJOPCODE_AASTORE] = &&EJOPCODE_AASTORE,

        [EJOPCODE_ANEWARRAY] = &&EJOPCODE_ANEWARRAY,
    };

    // Helper to advance PC and jump to next opcode
    #define NEXT() ({ \
        frame->pc += 1 + JOpcode_args_sizes[*frame->pc]; \
        if (opcodes_executed++ == thread->opcode_quota && thread->opcode_quota > 0) \
            return JERR_SCHEDULE; \
        goto *opcode_labels[*frame->pc]; \
    })

    // -----------------------------------------------------------------
    // Interpreter starts here
    // -----------------------------------------------------------------
    goto *opcode_labels[*frame->pc];

    // ========== LOADS ==========
    EJOPCODE_ILOAD:
        STACK_PUSH_INT(frame, LOCAL_LOAD_INT(frame, *(frame->pc + 1)));
        NEXT();

    EJOPCODE_ILOAD_0:
        STACK_PUSH_INT(frame, LOCAL_LOAD_INT(frame, 0));
        NEXT();
    EJOPCODE_ILOAD_1:
        STACK_PUSH_INT(frame, LOCAL_LOAD_INT(frame, 1));
        NEXT();
    EJOPCODE_ILOAD_2:
        STACK_PUSH_INT(frame, LOCAL_LOAD_INT(frame, 2));
        NEXT();
    EJOPCODE_ILOAD_3:
        STACK_PUSH_INT(frame, LOCAL_LOAD_INT(frame, 3));
        NEXT();

    EJOPCODE_FLOAD:
        STACK_PUSH_FLOAT(frame, LOCAL_LOAD_FLOAT(frame, *(frame->pc + 1)));
        NEXT();

    EJOPCODE_FLOAD_0:
        STACK_PUSH_FLOAT(frame, LOCAL_LOAD_FLOAT(frame, 0));
        NEXT();
    EJOPCODE_FLOAD_1:
        STACK_PUSH_FLOAT(frame, LOCAL_LOAD_FLOAT(frame, 1));
        NEXT();
    EJOPCODE_FLOAD_2:
        STACK_PUSH_FLOAT(frame, LOCAL_LOAD_FLOAT(frame, 2));
        NEXT();
    EJOPCODE_FLOAD_3:
        STACK_PUSH_FLOAT(frame, LOCAL_LOAD_FLOAT(frame, 3));
        NEXT();

    EJOPCODE_ALOAD:
        STACK_PUSH_REF(frame, LOCAL_LOAD_REF(frame, *(frame->pc + 1)));
        NEXT();

    EJOPCODE_ALOAD_0:
        STACK_PUSH_REF(frame, LOCAL_LOAD_REF(frame, 0));
        NEXT();
    EJOPCODE_ALOAD_1:
        STACK_PUSH_REF(frame, LOCAL_LOAD_REF(frame, 1));
        NEXT();
    EJOPCODE_ALOAD_2:
        STACK_PUSH_REF(frame, LOCAL_LOAD_REF(frame, 2));
        NEXT();
    EJOPCODE_ALOAD_3:
        STACK_PUSH_REF(frame, LOCAL_LOAD_REF(frame, 3));
        NEXT();
    EJOPCODE_ACONST_NULL:
        STACK_PUSH_REF(frame, NULL);
        NEXT();

    // ========== STORES ==========
    EJOPCODE_ISTORE:
        LOCAL_STORE_INT(frame, STACK_POP_INT(frame), *(frame->pc + 1));
        NEXT();

    EJOPCODE_ISTORE_0:
        LOCAL_STORE_INT(frame, STACK_POP_INT(frame), 0);
        NEXT();
    EJOPCODE_ISTORE_1:
        LOCAL_STORE_INT(frame, STACK_POP_INT(frame), 1);
        NEXT();
    EJOPCODE_ISTORE_2:
        LOCAL_STORE_INT(frame, STACK_POP_INT(frame), 2);
        NEXT();
    EJOPCODE_ISTORE_3:
        LOCAL_STORE_INT(frame, STACK_POP_INT(frame), 3);
        NEXT();

    EJOPCODE_FSTORE:
        LOCAL_STORE_FLOAT(frame, STACK_POP_FLOAT(frame), *(frame->pc + 1));
        NEXT();

    EJOPCODE_FSTORE_0:
        LOCAL_STORE_FLOAT(frame, STACK_POP_FLOAT(frame), 0);
        NEXT();
    EJOPCODE_FSTORE_1:
        LOCAL_STORE_FLOAT(frame, STACK_POP_FLOAT(frame), 1);
        NEXT();
    EJOPCODE_FSTORE_2:
        LOCAL_STORE_FLOAT(frame, STACK_POP_FLOAT(frame), 2);
        NEXT();
    EJOPCODE_FSTORE_3:
        LOCAL_STORE_FLOAT(frame, STACK_POP_FLOAT(frame), 3);
        NEXT();

    EJOPCODE_ASTORE:
        LOCAL_STORE_REF(frame, STACK_POP_REF(frame), *(frame->pc + 1));
        NEXT();

    EJOPCODE_ASTORE_0:
        LOCAL_STORE_REF(frame, STACK_POP_REF(frame), 0);
        NEXT();
    EJOPCODE_ASTORE_1:
        LOCAL_STORE_REF(frame, STACK_POP_REF(frame), 1);
        NEXT();
    EJOPCODE_ASTORE_2:
        LOCAL_STORE_REF(frame, STACK_POP_REF(frame), 2);
        NEXT();
    EJOPCODE_ASTORE_3:
        LOCAL_STORE_REF(frame, STACK_POP_REF(frame), 3);
        NEXT();

    // ========== CONSTANTS ==========
    EJOPCODE_ICONST_0:
        STACK_PUSH_INT(frame, 0);
        NEXT();
    EJOPCODE_ICONST_1:
        STACK_PUSH_INT(frame, 1);
        NEXT();
    EJOPCODE_ICONST_2:
        STACK_PUSH_INT(frame, 2);
        NEXT();
    EJOPCODE_ICONST_3:
        STACK_PUSH_INT(frame, 3);
        NEXT();
    EJOPCODE_ICONST_4:
        STACK_PUSH_INT(frame, 4);
        NEXT();
    EJOPCODE_ICONST_5:
        STACK_PUSH_INT(frame, 5);
        NEXT();
    EJOPCODE_ICONST_M1:
        STACK_PUSH_INT(frame, -1);
        NEXT();

    EJOPCODE_BIPUSH:
        STACK_PUSH_INT(frame, (int32_t)*(int8_t*)(frame->pc + 1));
        NEXT();

    EJOPCODE_SIPUSH:
        STACK_PUSH_INT(frame, (int32_t)(int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1)));
        NEXT();

    // ========== RETURNS ==========
    EJOPCODE_RETURN:{
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        if (!(frame = thread_frame_pop(thread)))
            goto exit;   // root method exit
        NEXT();
    }

    EJOPCODE_IRETURN: {
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        int32_t ret = STACK_POP_INT(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_INT(frame, ret);
        NEXT();
    }

    EJOPCODE_FRETURN:{
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        float ret = STACK_POP_FLOAT(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_FLOAT(frame, ret);
        NEXT();
    }

    EJOPCODE_ARETURN:{
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        void* ret = STACK_POP_REF(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_REF(frame, ret);
        NEXT();
    }

    EJOPCODE_LRETURN:{
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        uint64_t ret = STACK_POP_LONG(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_LONG(frame, ret);
        NEXT();
    }

    EJOPCODE_DRETURN:{
        Monitor_t *monitor = NULL, *tmp = NULL;
        list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
            FAIL_SET_JUMP((err = monitor_exit(monitor, thread)) == JERR_OK, err, err, exit);
        }

        double ret = STACK_POP_DOUBLE(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_DOUBLE(frame, ret);
        NEXT();
    }

    // ========== FIELD ACCESS ==========
    EJOPCODE_PUTSTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;
        field->constantvalue = NULL;
        
        STACK_POP_GENERIC(frame, field->type, &field->class->storage[field->offset]);
        NEXT();
    }

    EJOPCODE_GETSTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;
        void* value = &field->class->storage[field->offset];

        if (field->constantvalue) {
            FAIL_SET_JUMP((err = class_resolv_symbol(field->constantvalue)) == JERR_OK, err, err, exit);
            memcpy(value, (field->constantvalue->type == SYMBOL_STRING) ? &field->constantvalue->value : field->constantvalue->value,
                   ((field->type == TYPE_LONG || field->type == TYPE_DOUBLE) ? 2 : 1) * sizeof(int32_t));
            field->constantvalue = NULL;
        }

        STACK_PUSH_GENERIC(frame, field->type, value);
        NEXT();
    }
    
    EJOPCODE_GETFIELD:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;
        Object_t* object = STACK_POP_REF(frame);

        FAIL_SET_JUMP(class_is_compatible(object->class, field->class), err, JERR_TYPECHECK_FAILURE, exit);

        int32_t* fields = NULL;
        FAIL_SET_JUMP((err = heap_class_object_get_fields(object, &fields)) == JERR_OK, err, err, exit);

        assert(frame->sp < ((MethodBytecode_t*)frame->method->code)->max_stack);
        STACK_PUSH_GENERIC(frame, field->type, &fields[field->offset]);

        NEXT();
    }

    EJOPCODE_PUTFIELD:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;
        int64_t value = 0;

        STACK_POP_GENERIC(frame, field->type, &value);
        Object_t* object = STACK_POP_REF(frame);

        FAIL_SET_JUMP(class_is_compatible(object->class, field->class), err, JERR_TYPECHECK_FAILURE, exit);

        int32_t* fields = NULL;
        FAIL_SET_JUMP((err = heap_class_object_get_fields(object, &fields)) == JERR_OK, err, err, exit);
        memcpy(&fields[field->offset], &value, field->size);

        NEXT();
    }

    // ========== METHOD INVOCATION ==========
    EJOPCODE_INVOKESPECIAL:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* method = sym->value;

        if (!method->flags.is_native) {
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            FAIL_SET_JUMP(interpreter_check_arguments(method, frame->shadow_stack, frame->sp - method->args_slots), err, JERR_TYPECHECK_FAILURE, exit);
            if(method->flags.is_syncronized){
                FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - method->args_slots], thread)) == JERR_OK, err, 
                ({
                    //Monitor is locked!
                    //Monitor wasnt yet added to frame, so we can safely remove it
                    thread_frame_pop(thread);
                    (err);
                }), exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(thread, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKESTATIC:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* method = sym->value;

        if (!method->flags.is_native){
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            FAIL_SET_JUMP(interpreter_check_arguments(method, frame->shadow_stack, frame->sp - method->args_slots), err, JERR_TYPECHECK_FAILURE, exit);
            if(method->flags.is_syncronized){
                FAIL_SET_JUMP((err = monitor_enter(method->class->class_object, thread)) == JERR_OK, err, 
                ({
                    //Monitor is locked!
                    //Monitor wasnt yet added to frame, so we can safely remove it
                    thread_frame_pop(thread);
                    (err);
                }), exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];

            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(thread, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKEVIRTUAL:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* template = sym->value;
        FAIL_SET_JUMP(interpreter_check_arguments(template, frame->shadow_stack, frame->sp - template->args_slots), err, JERR_TYPECHECK_FAILURE, exit);

        Object_t* object = (Object_t*)frame->stack[frame->sp - template->args_slots];
        Class_t* object_class = object->class;
        FAIL_SET_JUMP(template->flags.is_virtual, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(class_is_compatible(object_class, template->class), err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(template->vtable_index < object_class->vtable_size, err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = object_class->vtable[template->vtable_index];

        if (!method->flags.is_native) {
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            if(method->flags.is_syncronized){
                FAIL_SET_JUMP((err = monitor_enter(object, thread)) == JERR_OK, err, 
                ({
                    //Monitor is locked!
                    //Monitor wasnt yet added to frame, so we can safely remove it
                    thread_frame_pop(thread);
                    (err);
                }), exit);
            }
            
            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(thread, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKEINTERFACE:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* template = sym->value;
        FAIL_SET_JUMP(interpreter_check_arguments(template, frame->shadow_stack, frame->sp - template->args_slots), err, JERR_TYPECHECK_FAILURE, exit);

        Object_t* object = (Object_t*)frame->stack[frame->sp - template->args_slots];
        Class_t* object_class = object->class;
        FAIL_SET_JUMP(!template->flags.is_static, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(class_is_compatible(object_class, template->class), err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = NULL;
        for(unsigned i = 0; i < object_class->implements.count; i++){
            Implementation_t* implementation = &object_class->implements.implementations[i];
            if(implementation->interface->name_id == template->class->name_id){
                FAIL_SET_JUMP(implementation->methods_count > template->interface_index, err, JERR_TYPECHECK_FAILURE, exit);
                method = implementation->methods[template->interface_index];
            }
        }
        FAIL_SET_JUMP(method, err, JERR_NOTFOUND, exit);

        if (!method->flags.is_native) {
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            if(method->flags.is_syncronized){
                FAIL_SET_JUMP((err = monitor_enter(object, thread)) == JERR_OK, err, 
                ({
                    //Monitor is locked!
                    //Monitor wasnt yet added to frame, so we can safely remove it
                    thread_frame_pop(thread);
                    (err);
                }), exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(thread, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    // ========== CONDITIONAL BRANCHES (int) ==========
    #define IF_CMP(OP) ({ \
        int32_t v2 = STACK_POP_INT(frame); \
        int32_t v1 = STACK_POP_INT(frame); \
        int16_t offset = (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1)); \
        if (v1 OP v2) {frame->pc += offset; goto *opcode_labels[*frame->pc];}\
        NEXT(); \
    })

    EJOPCODE_IF_ICMPEQ: IF_CMP(==);
    EJOPCODE_IF_ICMPNE: IF_CMP(!=);
    EJOPCODE_IF_ICMPLT: IF_CMP(<);
    EJOPCODE_IF_ICMPLE: IF_CMP(<=);
    EJOPCODE_IF_ICMPGT: IF_CMP(>);
    EJOPCODE_IF_ICMPGE: IF_CMP(>=);

    // ========== INTEGER ARITHMETIC ==========
    #define BINARY_INT(OP) do { \
        int32_t v2 = STACK_POP_INT(frame); \
        int32_t v1 = STACK_POP_INT(frame); \
        STACK_PUSH_INT(frame, v1 OP v2); \
        NEXT(); \
    } while (0)

    EJOPCODE_IADD: BINARY_INT(+);
    EJOPCODE_ISUB: BINARY_INT(-);
    EJOPCODE_IMUL: BINARY_INT(*);
    EJOPCODE_IDIV: BINARY_INT(/);
    EJOPCODE_IREM: BINARY_INT(%);

    // ========== LONG ARITHMETIC ==========
    #define BINARY_LONG(OP) do { \
        uint64_t v2 = STACK_POP_LONG(frame); \
        uint64_t v1 = STACK_POP_LONG(frame); \
        STACK_PUSH_LONG(frame, v1 OP v2); \
        NEXT(); \
    } while (0)

    EJOPCODE_LADD: BINARY_LONG(+);
    EJOPCODE_LSUB: BINARY_LONG(-);
    EJOPCODE_LMUL: BINARY_LONG(*);
    EJOPCODE_LDIV: BINARY_LONG(/);
    EJOPCODE_LREM: BINARY_LONG(%);

    // ========== FLOAT ARITHMETIC ==========
    #define BINARY_FLOAT(OP) do { \
        float v2 = STACK_POP_FLOAT(frame); \
        float v1 = STACK_POP_FLOAT(frame); \
        STACK_PUSH_FLOAT(frame, v1 OP v2); \
        NEXT(); \
    } while (0)

    EJOPCODE_FADD: BINARY_FLOAT(+);
    EJOPCODE_FSUB: BINARY_FLOAT(-);
    EJOPCODE_FMUL: BINARY_FLOAT(*);
    EJOPCODE_FDIV: BINARY_FLOAT(/);
    EJOPCODE_FREM: { float v2 = STACK_POP_FLOAT(frame); float v1 = STACK_POP_FLOAT(frame); STACK_PUSH_FLOAT(frame, fmodf(v1, v2)); NEXT(); }

    // ========== DOUBLE ARITHMETIC ==========
    #define BINARY_DOUBLE(OP) do { \
        double v2 = STACK_POP_DOUBLE(frame); \
        double v1 = STACK_POP_DOUBLE(frame); \
        STACK_PUSH_DOUBLE(frame, v1 OP v2); \
        NEXT(); \
    } while (0)

    EJOPCODE_DADD: BINARY_DOUBLE(+);
    EJOPCODE_DSUB: BINARY_DOUBLE(-);
    EJOPCODE_DMUL: BINARY_DOUBLE(*);
    EJOPCODE_DDIV: BINARY_DOUBLE(/);
    EJOPCODE_DREM: { double v2 = STACK_POP_DOUBLE(frame); double v1 = STACK_POP_DOUBLE(frame); STACK_PUSH_DOUBLE(frame, fmod(v1, v2)); NEXT(); }

    // ========== IINC ==========
    EJOPCODE_IINC: {
        uint8_t idx = *(frame->pc + 1);
        int8_t inc = *(frame->pc + 2);
        int32_t old = LOCAL_LOAD_INT(frame, idx);
        LOCAL_STORE_INT(frame, old + inc, idx);
        NEXT();
    }

    // ========== GOTO & JSR ==========
    EJOPCODE_GOTO: {
        frame->pc += (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_JSR: {
        STACK_PUSH_INT(frame, (int32_t)(uintptr_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc])));
        frame->pc += (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_JSR_W: {
        STACK_PUSH_INT(frame, (int32_t)(uintptr_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc])));
        frame->pc += (int32_t)be32_to_cpu(*(int32_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_RET: {
        uint8_t idx = *(frame->pc + 1);
        frame->pc = (uint8_t*)(uintptr_t)LOCAL_LOAD_INT(frame, idx);
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_DUP: {
        // DUP duplicates the top word on the stack (int, float, or reference)
        if (frame->sp == 0) {
            err = JERR_TYPECHECK_FAILURE;
            goto exit;
        }
        int top_idx = frame->sp - 1;
        int32_t value = frame->stack[top_idx];
        int is_ref = SHADOW_GET_REF(frame->shadow_stack, top_idx);

        if (is_ref) {
            STACK_PUSH_REF(frame, (void*)(uintptr_t)value);
        } else {
            // For int or float, just copy the raw bits
            STACK_PUSH_INT(frame, value);
        }
        NEXT();
    }

    EJOPCODE_LDC:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[*(uint8_t*)(frame->pc + 1)];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);

        switch(sym->type){
            default:
                err = JERR_BADPARAM;
                goto exit;

            case SYMBOL_INT:
            case SYMBOL_FLOAT:
            case SYMBOL_LONG:
            case SYMBOL_DOUBLE:
            case SYMBOL_STRING:
                STACK_PUSH_GENERIC(frame, sym_to_value_type[sym->type], sym->value);
                break;
        }

        NEXT();
    }

    EJOPCODE_LDC2_W:
    EJOPCODE_LDC_W:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);

        switch(sym->type){
            default:
                err = JERR_BADPARAM;
                goto exit;

            case SYMBOL_INT:
            case SYMBOL_FLOAT:
            case SYMBOL_LONG:
            case SYMBOL_DOUBLE:
            case SYMBOL_STRING:
                STACK_PUSH_GENERIC(frame, sym_to_value_type[sym->type], sym->value);
                break;
        }

        NEXT();
    }

    EJOPCODE_NEW:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);

        Object_t* object = 0;
        FAIL_SET_JUMP((err = heap_class_object_alloc(sym->value, &object)) == JERR_OK, err, err, exit); 

        STACK_PUSH_REF(frame, object);
        NEXT();
    }

    EJOPCODE_NEWARRAY:{
        uint8_t type = *(uint8_t*)(frame->pc + 1);
        int32_t length = STACK_POP_INT(frame);

        JavaValueType_t type_mapping[] = {[4] = TYPE_BOOL,
                                          [5] = TYPE_CHAR,
                                          [6] = TYPE_FLOAT,
                                          [7] = TYPE_DOUBLE,
                                          [8] = TYPE_BYTE,
                                          [9] = TYPE_SHORT,
                                          [10] = TYPE_INT,
                                          [11] = TYPE_LONG,};

        Object_t* object = NULL;
        Class_t* array_class = NULL;
        char class_name[3] = {'[',type_mapping[type],'\0'};
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add(class_name), &array_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_alloc(array_class, length, &object)) == JERR_OK, err, err, exit);

        STACK_PUSH_REF(frame, object);

        NEXT();
    }

    EJOPCODE_ANEWARRAY:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);

        Class_t* element_class = sym->value;
        int32_t length = STACK_POP_INT(frame);

        FAIL_SET_JUMP(length >= 0, err, JERR_TYPECHECK_FAILURE, exit);

        //Extensive string fuckery (i hate it)
        char* element_name = stringpool_get(element_class->name_id);
        size_t element_name_length = strlen(element_name);
        size_t array_class_name_length = 2 + element_name_length;
        char array_class_name[array_class_name_length];

        memset(array_class_name, 0, array_class_name_length);

        array_class_name[0] = '[';
        memcpy(&array_class_name[1], element_name, element_name_length);
        //====================================

        Class_t* array_class = NULL;
        Object_t* array = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add(array_class_name), &array_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_alloc(array_class, length, &array)) == JERR_OK, err, err, exit);

        STACK_PUSH_REF(frame, array);

        NEXT();
    }

    EJOPCODE_ARRAYLENGTH:{
        int32_t length = 0;
        Object_t* object = STACK_POP_REF(frame);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);

        STACK_PUSH_INT(frame, length);
        NEXT();
    }

    EJOPCODE_IALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int32_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_INT(frame, array[index]);

        NEXT();
    }

    EJOPCODE_LALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int64_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_LONG(frame, array[index]);

        NEXT();
    }

    EJOPCODE_FALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        float* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_FLOAT(frame, array[index]);

        NEXT();
    }

    EJOPCODE_DALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        double* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_DOUBLE(frame, array[index]);

        NEXT();
    }

    EJOPCODE_AALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        Object_t** array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_REF(frame, array[index]);

        NEXT();
    }

    EJOPCODE_BALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int8_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_CALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        uint16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_SALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_IASTORE:{
        int32_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int32_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_FASTORE:{
        float value = STACK_POP_FLOAT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        float* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_LASTORE:{
        int64_t value = STACK_POP_LONG(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int64_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }
    
    EJOPCODE_DASTORE:{
        double value = STACK_POP_DOUBLE(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        double* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_BASTORE:{
        int8_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int8_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }
    
    EJOPCODE_SASTORE:{
        int16_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }
    EJOPCODE_CASTORE:{
        uint16_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        uint16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_AASTORE:{
        Object_t* value = STACK_POP_REF(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        Object_t** array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);

        //Im starting to hate this fucking string shenanigans
        Class_t* items_class = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add(stringpool_get(object->class->name_id) + 1), &items_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(class_is_compatible(value->class, items_class), err, JERR_TYPECHECK_FAILURE, exit);

        FAIL_SET_JUMP(index < length, err, JERR_TYPECHECK_FAILURE, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_POP:{
        if(frame->sp-- == 0) {err = JERR_TYPECHECK_FAILURE; goto exit;}
        SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp);
        NEXT();
    }

    EJOPCODE_IFNULL:{
        if(STACK_POP_REF(frame) == NULL){
            frame->pc += (int16_t)be16_to_cpu(*(uint16_t*)(frame->pc + 1));
            goto *opcode_labels[*frame->pc];
        } 
        NEXT();
    }

    EJOPCODE_IFNONNULL:{
        if(STACK_POP_REF(frame) != NULL){
            frame->pc += (int16_t)be16_to_cpu(*(uint16_t*)(frame->pc + 1));
            goto *opcode_labels[*frame->pc];
        } 
        NEXT();
    }


exit:
    return err;
}