#include "thread.h"
#include "bumper.h"
#include "class.h"
#include "config.h"
#include "jerror.h"
#include "list.h"
#include "native_methods_service.h"
#include "parser.h"
#include "opcodes.h"
#include "lb_endian.h"
#include "stringpool.h"


#include <assert.h>
#include <string.h>
#include <math.h>


static struct list_head s_active_threads;
static struct list_head s_free_threads;
static struct list_head s_inactive_threads;

static Thread_t s_threads[THREAD_MAX_COUNT] = {0};
static size_t s_active_threads_count = 0;

CallFrame_t* thread_frame_push(Thread_t* thread, Method_t* method);
CallFrame_t* thread_frame_pop(Thread_t* thread);

void threads_init(){
    s_active_threads_count = 0;

    INIT_LIST_HEAD(&s_active_threads);
    INIT_LIST_HEAD(&s_free_threads);
    INIT_LIST_HEAD(&s_inactive_threads);

    for(unsigned i = 0; i < THREAD_MAX_COUNT; i++){
        Thread_t* thread = &s_threads[i];

        thread->top_frame = NULL;
        bumper_create_from(&thread->frame_allocator, thread->stackbuf, THREAD_STACK_SIZE);

        INIT_LIST_HEAD(&thread->list);
        list_add(&thread->list, &s_free_threads);
    }
}

Thread_t* thread_alloc(){
    Thread_t* inactive = NULL;
    list_for_each_entry(inactive, &s_free_threads, list) break;

    if(inactive){
        INIT_LIST_HEAD(&inactive->joiners);

        list_del_init(&inactive->list);
        list_add(&inactive->list, &s_inactive_threads);

        bumper_reset(&inactive->frame_allocator);
    }

    return inactive;
}

void thread_start(Thread_t* thread, Object_t* this, Method_t* method){
    thread->state = THREAD_ACTIVE;
    thread->opcode_quota = THREAD_LOWEST_QUOTA * THREAD_DEFAULT_PRIORITY;
    
    CallFrame_t* base_frame = thread_frame_push(thread, method);
    assert(base_frame && method->args_slots == 1 && method->flags.is_virtual);

    list_del_init(&thread->list);
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
        frame->frame_size = frame_size;
        frame->locals = (void*)(frame_memory + sizeof(CallFrame_t));
        frame->stack = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + sizeof(CallFrame_t));
        frame->shadow_locals = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + sizeof(CallFrame_t));
        frame->shadow_stack = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + (((bytecode->max_locals + 31) / 32) * sizeof(int32_t)) + sizeof(CallFrame_t));

        frame->pc = bytecode->code;
        frame->method = method;

        frame->prev = thread->top_frame;
        thread->top_frame = frame;

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
#define STACK_PUSH_INT(frame, value) ({ \
    (frame)->stack[(frame)->sp] = (value); \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->sp++; \
})

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
    if ((frame)->sp == 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->stack[(frame)->sp]; \
})

#define STACK_POP_FLOAT(frame) ({ \
    if ((frame)->sp == 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    union { int32_t i; float f; } _u = { .i = (frame)->stack[(frame)->sp] }; \
    _u.f; \
})

#define STACK_POP_REF(frame) ({ \
    if ((frame)->sp == 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 1) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
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
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    uint32_t _low = (frame)->stack[(frame)->sp]; \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
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
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->locals[idx]; \
})

#define LOCAL_LOAD_FLOAT(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 0) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
    union { int32_t i; float f; } _u = { .i = (frame)->locals[idx] }; \
    _u.f; \
})

#define LOCAL_LOAD_REF(frame, idx) ({ \
    if (SHADOW_GET_REF((frame)->shadow_locals, idx) != 1) { err = JERR_TYPECHECK_FAILURE; goto exit; } \
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
            STACK_PUSH_REF(frame, *(int32_t*)value);\
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
        assert(0 && "TODO: exceptions in native methods");
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

    INIT_LIST_HEAD(&thread.list);
    INIT_LIST_HEAD(&thread.joiners);

    thread.opcode_quota = 0;
    bumper_create_from(&thread.frame_allocator,thread.stackbuf, THREAD_STACK_SIZE);


    CallFrame_t* retstub = thread_frame_push(&thread, &(Method_t){.code = 
                                                &(MethodBytecode_t){.code_length = 2,
                                                                    .code = (uint8_t[2]){EJOPCODE_RETURN, EJOPCODE_RETURN},
                                                                    .max_stack = 2,
                                                                   }});
    if(method->flags.is_native){
       assert(0 && "TODO:");
    } else {
        CallFrame_t* frame = thread_frame_push(&thread, method);
        memcpy(frame->shadow_locals, method->args_bitmap, method->args_bitmap_size);
        memcpy(frame->locals, arguments, method->args_slots * sizeof(int32_t));

        FAIL_SET_JUMP((err = interpret_bytecode(&thread)) == JERR_OK, err, err, exit);
    }
    STACK_POP_GENERIC(retstub, method->return_type, return_value);

exit:
    return err;
}

static Error_t interpret_bytecode(Thread_t* thread) {
    Error_t err = JERR_OK;
    size_t opcodes_executed = 0;
    CallFrame_t* frame = thread_frame_get(thread);

    static const JavaValueType_t sym_to_value_type[] = {
        [SYMBOL_INT] = TYPE_INT,
        [SYMBOL_FLOAT] = TYPE_FLOAT,
        [SYMBOL_LONG] = TYPE_LONG,
        [SYMBOL_DOUBLE] = TYPE_LONG,
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
        [EJOPCODE_LDC] = &&EJOPCODE_LDC,
        [EJOPCODE_LDC2_W] = &&EJOPCODE_LDC2_W,
        [EJOPCODE_LDC_W] = &&EJOPCODE_LDC_W,
    };

    // Helper to advance PC and jump to next opcode
    #define NEXT() ({ \
        if (opcodes_executed++ == thread->opcode_quota && thread->opcode_quota > 0) \
            return JERR_SCHEDULE; \
        frame->pc += 1 + JOpcode_args_sizes[*frame->pc]; \
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
    EJOPCODE_RETURN:
        if (!(frame = thread_frame_pop(thread)))
            goto exit;   // root method exit
        NEXT();

    EJOPCODE_IRETURN: {
        int32_t ret = STACK_POP_INT(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_INT(frame, ret);
        NEXT();
    }

    EJOPCODE_FRETURN: {
        float ret = STACK_POP_FLOAT(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_FLOAT(frame, ret);
        NEXT();
    }

    EJOPCODE_ARETURN: {
        void* ret = STACK_POP_REF(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_REF(frame, ret);
        NEXT();
    }

    EJOPCODE_LRETURN: {
        uint64_t ret = STACK_POP_LONG(frame);
        if (!(frame = thread_frame_pop(thread)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_LONG(frame, ret);
        NEXT();
    }

    EJOPCODE_DRETURN: {
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
        
        STACK_POP_GENERIC(frame, field->type, &frame->method->class->static_fields_storage[field->offset]);
        NEXT();
    }

    EJOPCODE_GETSTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;
        void* value = &frame->method->class->static_fields_storage[field->offset];

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

        int32_t* fields_storage = (int32_t*)(((char*)object) + sizeof(*object));
        STACK_PUSH_GENERIC(frame, field->type, &fields_storage[field->offset]);

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

        int32_t* fields_storage = (int32_t*)(((char*)object) + sizeof(*object));
        memcpy(&fields_storage[field->offset], &value, class_field_sizeof(field->type) * sizeof(int32_t));

        NEXT();
    }

    // ========== METHOD INVOCATION ==========
    EJOPCODE_INVOKESPECIAL:
    EJOPCODE_INVOKESTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* method = sym->value;

        if (!method->flags.is_native) {
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            FAIL_SET_JUMP(interpreter_check_arguments(method, frame->shadow_stack, frame->sp), err, JERR_TYPECHECK_FAILURE, exit);

            memcpy(new_frame->shadow_locals, method->args_bitmap, method->args_bitmap_size);
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
        Object_t* object = (Object_t*)frame->stack[frame->sp - template->args_slots];
        Class_t* object_class = object->class;
        FAIL_SET_JUMP(template->flags.is_virtual, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(class_is_compatible(object_class, template->class), err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(template->vtable_index < object_class->vtable_size, err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = object_class->vtable[template->vtable_index];

        if (!method->flags.is_native) {
            CallFrame_t* new_frame = thread_frame_push(thread, method);
            FAIL_SET_JUMP(new_frame, err, JERR_OOM, exit);

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            FAIL_SET_JUMP(interpreter_check_arguments(method, frame->shadow_stack, frame->sp), err, JERR_TYPECHECK_FAILURE, exit);

            memcpy(new_frame->shadow_locals, method->args_bitmap, method->args_bitmap_size);
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

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            FAIL_SET_JUMP(interpreter_check_arguments(method, frame->shadow_stack, frame->sp), err, JERR_TYPECHECK_FAILURE, exit);

            memcpy(new_frame->shadow_locals, method->args_bitmap, method->args_bitmap_size);
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

        int32_t object = 0;
        FAIL_SET_JUMP((err = object_class_alloc(sym->value, &object)) == JERR_OK, err, err, exit); 

        STACK_PUSH_REF(frame, object);
        NEXT();
    }

exit:
    return err;
}