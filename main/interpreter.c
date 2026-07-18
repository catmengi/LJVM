#include "interpreter.h"
#include "jerror.h"
#include "list.h"
#include "monitor.h"
#include "opcodes.h"
#include "native_methods_service.h"
#include "bumper.h"
#include "class.h"
#include "heap.h"
#include "thread.h"
#include "stringpool.h"
#include "lb_endian.h"

#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int s_exception_nameid[JERR_UNKNOWN - JERR_NOCLASSDEF] = {0};

#define SHADOW_CLEAR_REF(bitmap, idx)  ((bitmap)[(idx) >> 5] &= ~(1U << ((idx) & 31)))
#define SHADOW_SET_REF(bitmap, idx)    ((bitmap)[(idx) >> 5] |= (1U << ((idx) & 31)))
#define SHADOW_GET_REF(bitmap, idx)    (((bitmap)[(idx) >> 5] & (1U << ((idx) & 31))) ? 1 : 0)

#ifdef INTERPRETER_DEBUG
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
    if ((frame)->sp == 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->stack[(frame)->sp]; \
})

#define STACK_POP_FLOAT(frame) ({ \
    if ((frame)->sp == 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    union { int32_t i; float f; } _u = { .i = (frame)->stack[(frame)->sp] }; \
    _u.f; \
})

#define STACK_POP_REF(frame) ({ \
    if ((frame)->sp == 0) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    (frame)->sp--; \
    if (SHADOW_GET_REF((frame)->shadow_stack, (frame)->sp) != 1) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (void*)(uintptr_t)(frame)->stack[(frame)->sp]; \
})

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
    if ((frame)->sp < 2) {err = JERR_TYPECHECK_FAILURE; goto exit; } \
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
#else
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
    (frame)->sp--; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (frame)->stack[(frame)->sp]; \
})

#define STACK_POP_FLOAT(frame) ({ \
    (frame)->sp--; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    union { int32_t i; float f; } _u = { .i = (frame)->stack[(frame)->sp] }; \
    _u.f; \
})

#define STACK_POP_REF(frame) ({ \
    (frame)->sp--; \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    (void*)(uintptr_t)(frame)->stack[(frame)->sp]; \
})

#define STACK_PUSH_LONG(frame, value) ({ \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    *(int64_t*)(&(frame)->stack[frame->sp]) = value;\
    (frame)->sp += 2;\
})

#define STACK_PUSH_DOUBLE(frame, value) ({ \
    union { double d; uint64_t u; } _u = { .d = (value) }; \
    STACK_PUSH_LONG(frame, _u.u); \
})

#define STACK_POP_LONG(frame) ({ \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    SHADOW_CLEAR_REF((frame)->shadow_stack, (frame)->sp); \
    *(int64_t*)(&(frame)->stack[(frame)->sp -= 2]);\
})

#define STACK_POP_DOUBLE(frame) ({ \
    uint64_t _u = STACK_POP_LONG(frame); \
    union { uint64_t u; double d; } _conv = { .u = _u }; \
    _conv.d; \
})

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
    *(int64_t*)(&(frame)->locals[idx]) = value; \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx); \
    SHADOW_CLEAR_REF((frame)->shadow_locals, idx + 1); \
})

#define LOCAL_STORE_DOUBLE(frame, value, idx) ({ \
    union { double d; uint64_t u; } _u = { .d = (value) }; \
    LOCAL_STORE_LONG(frame, _u.u, idx); \
})

#define LOCAL_LOAD_INT(frame, idx) ({ \
    (frame)->locals[idx]; \
})

#define LOCAL_LOAD_FLOAT(frame, idx) ({ \
    union { int32_t i; float f; } _u = { .i = (frame)->locals[idx] }; \
    _u.f; \
})

#define LOCAL_LOAD_REF(frame, idx) ({ \
    (void*)(uintptr_t)(frame)->locals[idx]; \
})

#define LOCAL_LOAD_LONG(frame, idx) ({ \
    (int64_t)(frame)->locals[idx];\
})

#define LOCAL_LOAD_DOUBLE(frame, idx) ({ \
    uint64_t _u = LOCAL_LOAD_LONG(frame, idx); \
    union { uint64_t u; double d; } _conv = { .u = _u }; \
    _conv.d; \
})
#endif

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

static inline int error_to_exception_nameid_index(Error_t err){
    assert(err >= JERR_NOCLASSDEF && err < JERR_UNKNOWN);
    return err - JERR_NOCLASSDEF;
}
static inline uint16_t error_to_exception_nameid(Error_t err){
    return s_exception_nameid[error_to_exception_nameid_index(err)];
}

void interpreter_init(){
    s_exception_nameid[error_to_exception_nameid_index(JERR_NOCLASSDEF)] = stringpool_add("java/lang/NoClassDefFoundError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_INVALIDMONITORSTATE)] = stringpool_add("java/lang/IllegalMonitorStateException");
    s_exception_nameid[error_to_exception_nameid_index(JERR_NULLPOINTER)] = stringpool_add("java/lang/NullPointerException");
    s_exception_nameid[error_to_exception_nameid_index(JERR_NOSUCHFIELD)] = stringpool_add("java/lang/NoSuchFieldError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_NOSUCHMETHOD)] = stringpool_add("java/lang/NoSuchMethodError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_ABSTRACT)] = stringpool_add("java/lang/AbstractMethodError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_ILLEGALACCESS)] = stringpool_add("java/lang/IllegalAccessError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_EXCEPTION)] = stringpool_add("java/lang/Throwable");
    s_exception_nameid[error_to_exception_nameid_index(JERR_INCOMPATIBLECLASSCHANGE)] = stringpool_add("java/lang/IncompatibleClassChangeError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_INSTANTIATION)] = stringpool_add("java/lang/InstantiationError");
    s_exception_nameid[error_to_exception_nameid_index(JERR_NEGATIVESIZE)] = stringpool_add("java/lang/NegativeArraySizeException");
    s_exception_nameid[error_to_exception_nameid_index(JERR_INDEXOOB)] = stringpool_add("java/lang/ArrayIndexOutOfBoundsException");
    s_exception_nameid[error_to_exception_nameid_index(JERR_CAST)] = stringpool_add("java/lang/ClassCastException");
    s_exception_nameid[error_to_exception_nameid_index(JERR_STACKOVERFLOW)] = stringpool_add("java/lang/StackOverflowError");
}

Interpreter_t* interpreter_ctx_init(Thread_t* thread, Interpreter_t* ctx){
    bumper_create_from(&ctx->arena, ctx->stackbuf, sizeof(ctx->stackbuf));
    ctx->frame_count = 0;
    ctx->thread = thread; //Its done like this, so thread can be NULL!
    ctx->frame = NULL;

    return ctx;
}

static inline int int_ceil(int n, int d) {
    return (n + d - 1) / d;
}

InterpreterFrame_t* interpreter_frame_push(Interpreter_t* ctx, Method_t* method){
    MethodBytecode_t* bytecode = method->code; //Assumption that method is bytecode, not native
    size_t size = (bytecode->max_locals + bytecode->max_stack + int_ceil(bytecode->max_stack, 32) + int_ceil(bytecode->max_locals, 32)) * sizeof(int32_t) + sizeof(InterpreterFrame_t);

    void* chunk = bumper_alloc(&ctx->arena, size);
    if(!chunk) return NULL;

    bump_allocator_t sub_arena = {0};
    bumper_create_from(&sub_arena, chunk, size);

    InterpreterFrame_t* frame = bumper_calloc(&sub_arena, 1, sizeof(*frame));
    frame->stack = bumper_calloc(&sub_arena, bytecode->max_stack, sizeof(*frame->stack));
    frame->locals = bumper_calloc(&sub_arena, bytecode->max_locals, sizeof(*frame->locals));
    frame->shadow_stack = bumper_calloc(&sub_arena, int_ceil(bytecode->max_stack, 32), sizeof(*frame->shadow_stack));
    frame->shadow_locals = bumper_calloc(&sub_arena, int_ceil(bytecode->max_locals, 32), sizeof(*frame->shadow_locals));

    assert(frame->stack && frame->locals && frame->shadow_stack && frame->shadow_locals);
    memcpy(frame->shadow_locals, method->args_bitmap, method->args_bitmap_size * sizeof(*frame->shadow_locals));

    INIT_LIST_HEAD(&frame->held_monitors);
    frame->size = size;
    frame->pc = bytecode->code;
    frame->sp = 0;
    frame->method = method;
    frame->prev = ctx->frame;

    ctx->frame = frame;
    ctx->frame_count++;

    return frame;
}

InterpreterFrame_t* interpreter_frame_pop(Interpreter_t* ctx){
    if(ctx->frame){
        InterpreterFrame_t* frame = ctx->frame;
        ctx->frame_count--;
        ctx->frame = frame->prev;
        bumper_unwind(&ctx->arena, frame->size);
    }
    return ctx->frame;
}

InterpreterFrame_t* interpreter_frame_get(Interpreter_t* ctx){
    return ctx->frame;
}

static bool check_arguments(Method_t* method, uint32_t* shadow_stack, uint32_t sp){
    for(unsigned i = 0; i < method->args_slots; i++){
        if(SHADOW_GET_REF(shadow_stack, sp + i) != SHADOW_GET_REF(method->args_bitmap, i)) return false;
    }

    return true;
}

static Error_t native_method_invoke(Interpreter_t* ctx, InterpreterFrame_t* frame, Method_t* method){
    int32_t* args = &frame->stack[frame->sp - method->args_slots];
    NativeMethodReturnValue_t retval = ((NativeMethod_t)method->code)(ctx,method,args);

    switch(retval.err){
        case JERR_OK:
            frame->sp -= method->args_slots;
            STACK_PUSH_GENERIC(frame, method->return_type, retval.value);
            return retval.err;

        case JERR_EXCEPTION: 
            STACK_PUSH_REF(frame, *(Object_t**)retval.value);
            return JERR_EXCEPTION;

        default: return retval.err;
    }
}

Error_t interpreter_method_invoke(Interpreter_t* ctx, Method_t* method, int32_t* arguments, void* return_value){
    Error_t err = JERR_OK;

    FAIL_SET_JUMP(method && (arguments || method->args_slots == 0) && (return_value || method->return_type == TYPE_VOID), err, JERR_BADPARAM, exit);
    InterpreterFrame_t* retstub = interpreter_frame_push(ctx, &(Method_t){.code = 
                                                &(MethodBytecode_t){.code_length = 2,
                                                                    .code = (uint8_t[3]){EJOPCODE_NOP,EJOPCODE_NOP, EJOPCODE_INTERPRETEREXIT},
                                                                    .max_stack = 2 + method->flags.is_native ? method->args_slots : 0,
                                                                   }, .args_bitmap_size = 0});
    FAIL_SET_JUMP(retstub, err, JERR_STACKOVERFLOW, exit);
    if(method->flags.is_native){
        assert(0 && "TODO:");
    } else {
        InterpreterFrame_t* frame = interpreter_frame_push(ctx, method);
        FAIL_SET_JUMP(frame, err, JERR_STACKOVERFLOW, exit);

        memcpy(frame->locals, arguments, method->args_slots * sizeof(int32_t));

        FAIL_SET_JUMP((err = interpreter_execute(ctx)) == JERR_OK, err, err, exit);
    }
    STACK_POP_GENERIC(retstub, method->return_type, return_value);
    interpreter_frame_pop(ctx); //Delete retstub

exit:   
    return err;
}

static void frame_unlock_monitors(InterpreterFrame_t* frame){
    Monitor_t *monitor = NULL, *tmp = NULL;
    list_for_each_entry_safe(monitor, tmp, &frame->held_monitors, list){
        assert(monitor_exit_force(monitor) == JERR_OK);
    }
}

static Error_t throw_exception(Interpreter_t* ctx, Object_t* exception_object){
    size_t unwind_by = 0;
    //TODO: prepare stack trace!

    Interpreter_t* interpreter = ctx;
    for(InterpreterFrame_t* frame = interpreter->frame; frame; frame = frame->prev, interpreter->frame = frame, frame_unlock_monitors(frame)){
        MethodBytecode_t* bytecode = frame->method->code;

        for(unsigned i = 0; i < bytecode->exception_count; i++){
            MethodExceptionHandler_t* exception = &bytecode->exceptions[i];
            if(exception->start_pc + bytecode->code <= frame->pc && exception->end_pc + bytecode->code > frame->pc){
                ClassSymbol_t* exception_type_symbol = exception->type;

                if(exception_type_symbol == NULL || class_resolv_symbol(ctx, exception_type_symbol) == JERR_OK){
                    if(exception_type_symbol == NULL || class_is_compatible(exception_object->class,exception_type_symbol->value)){
                        bumper_unwind(&interpreter->arena, unwind_by);

                        frame->pc = bytecode->code + exception->handler_pc;
                        frame->sp = 0;
                        STACK_PUSH_REF(frame, exception_object);

                        return JERR_OK;
                    }
                }
            }
        }

        unwind_by += frame->size;
    }

    return JERR_UNHANDLED_EXCEPTION;
}

#define SPINLOCK_ENTER(spinlock) ({while(atomic_flag_test_and_set(&(spinlock))){}})
#define SPINLOCK_EXIT(spinlock) atomic_flag_clear(&(spinlock))

static Error_t run_clinit(Interpreter_t* ctx, Class_t* class){
    Error_t err = JERR_OK;

    LIST_HEAD(clinit_list);
    for(Class_t* cur = class; cur; cur = cur->parent){
        int expected = 0;
        if(atomic_compare_exchange_strong(&cur->clinit_stage, &expected, 1)){
            atomic_store(&cur->clinit_trigger, ctx->thread);

            INIT_LIST_HEAD(&cur->clinit_list);
            list_add(&cur->clinit_list, &clinit_list);
        } else {
            while(atomic_load(&cur->clinit_stage) != 2){
                if(atomic_load(&cur->clinit_trigger) == ctx->thread){
                    goto clinit_launch;
                }
                usleep(1000); //To not busy spin CPU
            }

            goto clinit_launch;
        }
    }

clinit_launch:
    if(!list_empty(&clinit_list)){
        int32_t clinit_nameid = stringpool_add("<clinit>@()V");
        assert(clinit_nameid >= 0);

        Class_t* to_init = NULL;
        list_for_each_entry(to_init, &clinit_list, clinit_list){
            Method_t* clinit = class_find_method(to_init, clinit_nameid);
            if(clinit){
                FAIL_SET_JUMP((err = interpreter_method_invoke(ctx, clinit, NULL, NULL)) == JERR_OK, err, JERR_CLINIT_FAILED, exit);
            }
            atomic_store(&to_init->clinit_stage, 2);
        }
    }

exit:
    return err;
}

Error_t interpreter_execute(Interpreter_t* ctx){
    Error_t err = JERR_OK;
    InterpreterFrame_t* frame = interpreter_frame_get(ctx);

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

        [EJOPCODE_LLOAD]   = &&EJOPCODE_LLOAD,
        [EJOPCODE_LLOAD_0] = &&EJOPCODE_LLOAD_0,
        [EJOPCODE_LLOAD_1] = &&EJOPCODE_LLOAD_1,
        [EJOPCODE_LLOAD_2] = &&EJOPCODE_LLOAD_2,
        [EJOPCODE_LLOAD_3] = &&EJOPCODE_LLOAD_3,

        [EJOPCODE_DLOAD]   = &&EJOPCODE_DLOAD,
        [EJOPCODE_DLOAD_0] = &&EJOPCODE_DLOAD_0,
        [EJOPCODE_DLOAD_1] = &&EJOPCODE_DLOAD_1,
        [EJOPCODE_DLOAD_2] = &&EJOPCODE_DLOAD_2,
        [EJOPCODE_DLOAD_3] = &&EJOPCODE_DLOAD_3,

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

        [EJOPCODE_LSTORE]   = &&EJOPCODE_LSTORE,
        [EJOPCODE_LSTORE_0] = &&EJOPCODE_LSTORE_0,
        [EJOPCODE_LSTORE_1] = &&EJOPCODE_LSTORE_1,
        [EJOPCODE_LSTORE_2] = &&EJOPCODE_LSTORE_2,
        [EJOPCODE_LSTORE_3] = &&EJOPCODE_LSTORE_3,

        [EJOPCODE_DSTORE]   = &&EJOPCODE_DSTORE,
        [EJOPCODE_DSTORE_0] = &&EJOPCODE_DSTORE_0,
        [EJOPCODE_DSTORE_1] = &&EJOPCODE_DSTORE_1,
        [EJOPCODE_DSTORE_2] = &&EJOPCODE_DSTORE_2,
        [EJOPCODE_DSTORE_3] = &&EJOPCODE_DSTORE_3,

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

        [EJOPCODE_LCONST_0]   = &&EJOPCODE_LCONST_0,
        [EJOPCODE_LCONST_1]   = &&EJOPCODE_LCONST_1,

        [EJOPCODE_FCONST_0] = &&EJOPCODE_FCONST_0,
        [EJOPCODE_FCONST_1] = &&EJOPCODE_FCONST_1,
        [EJOPCODE_FCONST_2] = &&EJOPCODE_FCONST_2,
        [EJOPCODE_DCONST_0] = &&EJOPCODE_DCONST_0,
        [EJOPCODE_DCONST_1] = &&EJOPCODE_DCONST_1,

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
        [EJOPCODE_GOTO_W] = &&EJOPCODE_GOTO_W,
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
        [EJOPCODE_MONITORENTER] = &&EJOPCODE_MONITORENTER,
        [EJOPCODE_MONITOREXIT] = &&EJOPCODE_MONITOREXIT,
        [EJOPCODE_INSTANCEOF] = &&EJOPCODE_INSTANCEOF,
        [EJOPCODE_CHECKCAST] = &&EJOPCODE_CHECKCAST,
        [EJOPCODE_ATHROW] = &&EJOPCODE_ATHROW,

        [EJOPCODE_NOP]          = &&EJOPCODE_NOP,
        [EJOPCODE_POP2]         = &&EJOPCODE_POP2,
        [EJOPCODE_DUP_X1]       = &&EJOPCODE_DUP_X1,
        [EJOPCODE_DUP_X2]       = &&EJOPCODE_DUP_X2,
        [EJOPCODE_DUP2]         = &&EJOPCODE_DUP2,
        [EJOPCODE_DUP2_X1]      = &&EJOPCODE_DUP2_X1,
        [EJOPCODE_DUP2_X2]      = &&EJOPCODE_DUP2_X2,
        [EJOPCODE_SWAP]         = &&EJOPCODE_SWAP,

        [EJOPCODE_INEG]         = &&EJOPCODE_INEG,
        [EJOPCODE_LNEG]         = &&EJOPCODE_LNEG,
        [EJOPCODE_FNEG]         = &&EJOPCODE_FNEG,
        [EJOPCODE_DNEG]         = &&EJOPCODE_DNEG,

        [EJOPCODE_ISHL]         = &&EJOPCODE_ISHL,
        [EJOPCODE_LSHL]         = &&EJOPCODE_LSHL,
        [EJOPCODE_ISHR]         = &&EJOPCODE_ISHR,
        [EJOPCODE_LSHR]         = &&EJOPCODE_LSHR,
        [EJOPCODE_IUSHR]        = &&EJOPCODE_IUSHR,
        [EJOPCODE_LUSHR]        = &&EJOPCODE_LUSHR,

        [EJOPCODE_IAND]         = &&EJOPCODE_IAND,
        [EJOPCODE_LAND]         = &&EJOPCODE_LAND,
        [EJOPCODE_IOR]          = &&EJOPCODE_IOR,
        [EJOPCODE_LOR]          = &&EJOPCODE_LOR,
        [EJOPCODE_IXOR]         = &&EJOPCODE_IXOR,
        [EJOPCODE_LXOR]         = &&EJOPCODE_LXOR,

        [EJOPCODE_I2L]          = &&EJOPCODE_I2L,
        [EJOPCODE_I2F]          = &&EJOPCODE_I2F,
        [EJOPCODE_I2D]          = &&EJOPCODE_I2D,
        [EJOPCODE_L2I]          = &&EJOPCODE_L2I,
        [EJOPCODE_L2F]          = &&EJOPCODE_L2F,
        [EJOPCODE_L2D]          = &&EJOPCODE_L2D,
        [EJOPCODE_F2I]          = &&EJOPCODE_F2I,
        [EJOPCODE_F2L]          = &&EJOPCODE_F2L,
        [EJOPCODE_F2D]          = &&EJOPCODE_F2D,
        [EJOPCODE_D2I]          = &&EJOPCODE_D2I,
        [EJOPCODE_D2L]          = &&EJOPCODE_D2L,
        [EJOPCODE_D2F]          = &&EJOPCODE_D2F,
        [EJOPCODE_I2B]          = &&EJOPCODE_I2B,
        [EJOPCODE_I2C]          = &&EJOPCODE_I2C,
        [EJOPCODE_I2S]          = &&EJOPCODE_I2S,

        [EJOPCODE_LCMP]         = &&EJOPCODE_LCMP,
        [EJOPCODE_FCMPL]        = &&EJOPCODE_FCMPL,
        [EJOPCODE_FCMPG]        = &&EJOPCODE_FCMPG,
        [EJOPCODE_DCMPL]        = &&EJOPCODE_DCMPL,
        [EJOPCODE_DCMPG]        = &&EJOPCODE_DCMPG,

        [EJOPCODE_IF_ACMPEQ]    = &&EJOPCODE_IF_ACMPEQ,
        [EJOPCODE_IF_ACMPNE]    = &&EJOPCODE_IF_ACMPNE,

        [EJOPCODE_IFEQ]         = &&EJOPCODE_IFEQ,
        [EJOPCODE_IFNE]         = &&EJOPCODE_IFNE,
        [EJOPCODE_IFLT]         = &&EJOPCODE_IFLT,
        [EJOPCODE_IFGE]         = &&EJOPCODE_IFGE,
        [EJOPCODE_IFGT]         = &&EJOPCODE_IFGT,
        [EJOPCODE_IFLE]         = &&EJOPCODE_IFLE,

        [EJOPCODE_INTERPRETEREXIT] = &&EJOPCODE_INTERPRETEREXIT,
    };

    // Helper to advance PC and jump to next opcode
    #define NEXT() ({ \
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

    EJOPCODE_LLOAD:
        STACK_PUSH_LONG(frame, LOCAL_LOAD_LONG(frame, *(frame->pc + 1)));
        NEXT();

    EJOPCODE_LLOAD_0:
        STACK_PUSH_LONG(frame, LOCAL_LOAD_LONG(frame, 0));
        NEXT();
    EJOPCODE_LLOAD_1:
        STACK_PUSH_LONG(frame, LOCAL_LOAD_LONG(frame, 1));
        NEXT();
    EJOPCODE_LLOAD_2:
        STACK_PUSH_LONG(frame, LOCAL_LOAD_LONG(frame, 2));
        NEXT();
    EJOPCODE_LLOAD_3:
        STACK_PUSH_LONG(frame, LOCAL_LOAD_LONG(frame, 3));
        NEXT();

    EJOPCODE_DLOAD:
        STACK_PUSH_DOUBLE(frame, LOCAL_LOAD_DOUBLE(frame, *(frame->pc + 1)));
        NEXT();

    EJOPCODE_DLOAD_0:
        STACK_PUSH_DOUBLE(frame, LOCAL_LOAD_DOUBLE(frame, 0));
        NEXT();
    EJOPCODE_DLOAD_1:
        STACK_PUSH_DOUBLE(frame, LOCAL_LOAD_DOUBLE(frame, 1));
        NEXT();
    EJOPCODE_DLOAD_2:
        STACK_PUSH_DOUBLE(frame, LOCAL_LOAD_DOUBLE(frame, 2));
        NEXT();
    EJOPCODE_DLOAD_3:
        STACK_PUSH_DOUBLE(frame, LOCAL_LOAD_DOUBLE(frame, 3));
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

    EJOPCODE_LSTORE:
        LOCAL_STORE_LONG(frame, STACK_POP_LONG(frame), *(frame->pc + 1));
        NEXT();
    EJOPCODE_LSTORE_0:
        LOCAL_STORE_LONG(frame, STACK_POP_LONG(frame), 0);
        NEXT();
    EJOPCODE_LSTORE_1:
        LOCAL_STORE_LONG(frame, STACK_POP_LONG(frame), 1);
        NEXT();
    EJOPCODE_LSTORE_2:
        LOCAL_STORE_LONG(frame, STACK_POP_LONG(frame), 2);
        NEXT();
    EJOPCODE_LSTORE_3:
        LOCAL_STORE_LONG(frame, STACK_POP_LONG(frame), 3);
        NEXT();

    EJOPCODE_DSTORE:
        LOCAL_STORE_DOUBLE(frame, STACK_POP_DOUBLE(frame), *(frame->pc + 1));
        NEXT();
    EJOPCODE_DSTORE_0:
        LOCAL_STORE_DOUBLE(frame, STACK_POP_DOUBLE(frame), 0);
        NEXT();
    EJOPCODE_DSTORE_1:
        LOCAL_STORE_DOUBLE(frame, STACK_POP_DOUBLE(frame), 1);
        NEXT();
    EJOPCODE_DSTORE_2:
        LOCAL_STORE_DOUBLE(frame, STACK_POP_DOUBLE(frame), 2);
        NEXT();
    EJOPCODE_DSTORE_3:
        LOCAL_STORE_DOUBLE(frame, STACK_POP_DOUBLE(frame), 3);
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

    EJOPCODE_LCONST_0:
        STACK_PUSH_LONG(frame, 0);
        NEXT();
    EJOPCODE_LCONST_1:
        STACK_PUSH_LONG(frame, 1);
        NEXT();

    EJOPCODE_FCONST_0:
        STACK_PUSH_FLOAT(frame, 0);
        NEXT();
    EJOPCODE_FCONST_1:
        STACK_PUSH_FLOAT(frame, 1);
        NEXT();
    EJOPCODE_FCONST_2:
        STACK_PUSH_FLOAT(frame, 2);
        NEXT();       

    EJOPCODE_DCONST_0:
        STACK_PUSH_DOUBLE(frame, 0);
        NEXT();
    EJOPCODE_DCONST_1:
        STACK_PUSH_DOUBLE(frame, 1);
        NEXT();

    EJOPCODE_BIPUSH:
        STACK_PUSH_INT(frame, (int32_t)*(int8_t*)(frame->pc + 1));
        NEXT();

    EJOPCODE_SIPUSH:
        STACK_PUSH_INT(frame, (int32_t)(int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1)));
        NEXT();

    // ========== RETURNS ==========
    EJOPCODE_RETURN:{
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        if (!(frame = interpreter_frame_pop(ctx))) return JERR_OK;
        NEXT();
    }

    EJOPCODE_IRETURN: {
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        int32_t ret = STACK_POP_INT(frame);
        if (!(frame = interpreter_frame_pop(ctx)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_INT(frame, ret);
        NEXT();
    }

    EJOPCODE_FRETURN:{
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        float ret = STACK_POP_FLOAT(frame);
        if (!(frame = interpreter_frame_pop(ctx)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_FLOAT(frame, ret);
        NEXT();
    }

    EJOPCODE_ARETURN:{
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        void* ret = STACK_POP_REF(frame);
        if (!(frame = interpreter_frame_pop(ctx)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_REF(frame, ret);
        NEXT();
    }

    EJOPCODE_LRETURN:{
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        uint64_t ret = STACK_POP_LONG(frame);
        if (!(frame = interpreter_frame_pop(ctx)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_LONG(frame, ret);
        NEXT();
    }

    EJOPCODE_DRETURN:{
        thread_safepoint_check();

        if(frame->method->flags.is_syncronized){
            Object_t* sync_object = frame->method->flags.is_static ? frame->method->class->class_object : (Object_t*)frame->locals[0];
            FAIL_SET_JUMP((err = monitor_exit(sync_object->monitor)) == JERR_OK, err, err, exit);
        }

        double ret = STACK_POP_DOUBLE(frame);
        if (!(frame = interpreter_frame_pop(ctx)))
            return JERR_ORPHAN_RETURN;
        STACK_PUSH_DOUBLE(frame, ret);
        NEXT();
    }

    // ========== FIELD ACCESS ==========
    EJOPCODE_PUTSTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_TYPECHECK_FAILURE, exit);


        Field_t* field = sym->value;
        void* value = (field->class->sfields_storage + field->offset);

        FAIL_SET_JUMP(field->flags.is_static, err, JERR_INCOMPATIBLECLASSCHANGE, exit);
        FAIL_SET_JUMP((err = run_clinit(ctx, field->class)) == JERR_OK, err, err, exit);


        field->constantvalue = NULL;
        
        if(field->flags.is_volatile){
            char volatile_buf[8] = {0};
            STACK_POP_GENERIC(frame, field->type, volatile_buf);

            switch(field->size){
                case 1:
                    __atomic_store_n((uint8_t*)value, *(uint8_t*)volatile_buf, __ATOMIC_SEQ_CST);
                    break;
    
                case 2:
                    __atomic_store_n((uint16_t*)value, *(uint16_t*)volatile_buf, __ATOMIC_SEQ_CST);
                    break;

                case 4:
                    __atomic_store_n((uint32_t*)value, *(uint32_t*)volatile_buf, __ATOMIC_SEQ_CST);
                    break;
            
                case 8:
                    __atomic_store_n((uint64_t*)value, *(uint64_t*)volatile_buf, __ATOMIC_SEQ_CST);
                    break;
            }

            NEXT();
        }

        STACK_POP_GENERIC(frame, field->type, value);
        NEXT();
    }

    EJOPCODE_GETSTATIC: {
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_TYPECHECK_FAILURE, exit);

        Field_t* field = sym->value;
        FAIL_SET_JUMP(field->flags.is_static, err, JERR_INCOMPATIBLECLASSCHANGE, exit);
        FAIL_SET_JUMP((err = run_clinit(ctx, field->class)) == JERR_OK, err, err, exit);


        void* value = (field->class->sfields_storage + field->offset);

        if (field->constantvalue) {
            FAIL_SET_JUMP((err = class_resolv_symbol(ctx, field->constantvalue)) == JERR_OK, err, err, exit);
            memcpy(value, (field->constantvalue->type == SYMBOL_STRING) ? &field->constantvalue->value : field->constantvalue->value,
                   ((field->type == TYPE_LONG || field->type == TYPE_DOUBLE) ? 2 : 1) * sizeof(int32_t));
            field->constantvalue = NULL;
        }

        if(field->flags.is_volatile){
            char volatile_buf[8] = {0};
            switch(field->size){
                case 1:
                    *(uint8_t*)volatile_buf = __atomic_load_n((uint8_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 2:
                    *(uint16_t*)volatile_buf = __atomic_load_n((uint16_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 4:
                    *(uint32_t*)volatile_buf = __atomic_load_n((uint32_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 8:
                    *(uint64_t*)volatile_buf = __atomic_load_n((uint64_t*)value, __ATOMIC_ACQUIRE);
                    break;
            }

            STACK_PUSH_GENERIC(frame, field->type, volatile_buf);
            NEXT();
        }

        STACK_PUSH_GENERIC(frame, field->type, value);
        NEXT();
    }
    
    EJOPCODE_GETFIELD:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_TYPECHECK_FAILURE, exit);

        Field_t* field = sym->value;
        FAIL_SET_JUMP(!field->flags.is_static, err, JERR_INCOMPATIBLECLASSCHANGE, exit);

        Object_t* object = STACK_POP_REF(frame);
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP(class_is_compatible(object->class, field->class), err, JERR_TYPECHECK_FAILURE, exit);

        void* fields = NULL;
        FAIL_SET_JUMP((err = heap_class_object_get_fields(object, &fields)) == JERR_OK, err, err, exit);

        void* value = fields + field->offset;

        if(field->flags.is_volatile){
            char volatile_buf[8] = {0};
            switch(field->size){
                case 1:
                    *(uint8_t*)volatile_buf = __atomic_load_n((uint8_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 2:
                    *(uint16_t*)volatile_buf = __atomic_load_n((uint16_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 4:
                    *(uint32_t*)volatile_buf = __atomic_load_n((uint32_t*)value, __ATOMIC_ACQUIRE);
                    break;
                case 8:
                    *(uint64_t*)volatile_buf = __atomic_load_n((uint64_t*)value, __ATOMIC_ACQUIRE);
                    break;
            }

            STACK_PUSH_GENERIC(frame, field->type, volatile_buf);
            NEXT();
        }


        STACK_PUSH_GENERIC(frame, field->type, (fields + field->offset));
        NEXT();
    }

    EJOPCODE_PUTFIELD:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_TYPECHECK_FAILURE, exit);

        Field_t* field = sym->value;
        FAIL_SET_JUMP(!field->flags.is_static, err, JERR_INCOMPATIBLECLASSCHANGE, exit);

        uint64_t value_buf;
        STACK_POP_GENERIC(frame, field->type, &value_buf);

        Object_t* object = STACK_POP_REF(frame);
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP(class_is_compatible(object->class, field->class), err, JERR_TYPECHECK_FAILURE, exit);

        void* fields = NULL;
        FAIL_SET_JUMP((err = heap_class_object_get_fields(object, &fields)) == JERR_OK, err, err, exit);

        void* value = (fields + field->offset);

        if(field->flags.is_volatile){
            switch(field->size){
                case 1:
                    __atomic_store_n((uint8_t*)value, *(uint8_t*)&value_buf, __ATOMIC_SEQ_CST);
                    break;
    
                case 2:
                    __atomic_store_n((uint16_t*)value, *(uint16_t*)&value_buf, __ATOMIC_SEQ_CST);
                    break;

                case 4:
                    __atomic_store_n((uint32_t*)value, *(uint32_t*)&value_buf, __ATOMIC_SEQ_CST);
                    break;
            
                case 8:
                    __atomic_store_n((uint64_t*)value, *(uint64_t*)&value_buf, __ATOMIC_SEQ_CST);
                    break;
            }

            NEXT();
        }

        memcpy(value, &value_buf, field->size);
        NEXT();
    }

    // ========== METHOD INVOCATION ==========
    EJOPCODE_INVOKESPECIAL:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = sym->value;

        FAIL_SET_JUMP(!method->flags.is_abstract, err, JERR_ABSTRACT, exit);

        if (!method->flags.is_native) {
            InterpreterFrame_t* new_frame = interpreter_frame_push(ctx, method);
            FAIL_SET_JUMP(new_frame, err, JERR_STACKOVERFLOW, exit);

            FAIL_SET_JUMP(check_arguments(method, frame->shadow_stack, frame->sp - method->args_slots), err, JERR_TYPECHECK_FAILURE, exit);
            if(method->flags.is_syncronized){
                assert(ctx->thread && "Cannot be run from bootstrap context");
                FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - method->args_slots])) == JERR_OK, err, err, exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(ctx, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKESTATIC:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = sym->value;
        FAIL_SET_JUMP(!method->flags.is_abstract, err, JERR_ABSTRACT, exit);
        FAIL_SET_JUMP((err = run_clinit(ctx, method->class)) == JERR_OK, err, err, exit);

        if (!method->flags.is_native){
            InterpreterFrame_t* new_frame = interpreter_frame_push(ctx, method);
            FAIL_SET_JUMP(new_frame, err, JERR_STACKOVERFLOW, exit);

            FAIL_SET_JUMP(check_arguments(method, frame->shadow_stack, frame->sp - method->args_slots), err, JERR_TYPECHECK_FAILURE, exit);
            if(method->flags.is_syncronized){
                assert(ctx->thread && "Cannot be run from bootstrap context");
                FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - method->args_slots])) == JERR_OK, err, err, exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];

            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(ctx, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKEVIRTUAL:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* template = sym->value;
        FAIL_SET_JUMP(check_arguments(template, frame->shadow_stack, frame->sp - template->args_slots), err, JERR_TYPECHECK_FAILURE, exit);

        Object_t* object = (Object_t*)frame->stack[frame->sp - template->args_slots];
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        Class_t* object_class = object->class;
        FAIL_SET_JUMP(template->flags.is_virtual, err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(class_is_compatible(object_class, template->class), err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP(template->vtable_index < object_class->vtable_size, err, JERR_TYPECHECK_FAILURE, exit);

        Method_t* method = object_class->vtable[template->vtable_index];

        FAIL_SET_JUMP(!method->flags.is_abstract, err, JERR_ABSTRACT, exit);

        if (!method->flags.is_native) {
            InterpreterFrame_t* new_frame = interpreter_frame_push(ctx, method);
            FAIL_SET_JUMP(new_frame, err, JERR_STACKOVERFLOW, exit);

            if(method->flags.is_syncronized){
                assert(ctx->thread && "Cannot be run from bootstrap context");
                FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - method->args_slots])) == JERR_OK, err, err, exit);
            }
            
            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(ctx, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }

    EJOPCODE_INVOKEINTERFACE:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);

        Method_t* template = sym->value;
        FAIL_SET_JUMP(check_arguments(template, frame->shadow_stack, frame->sp - template->args_slots), err, JERR_TYPECHECK_FAILURE, exit);

        Object_t* object = (Object_t*)frame->stack[frame->sp - template->args_slots];
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

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
        FAIL_SET_JUMP(method, err, JERR_NOSUCHMETHOD, exit);
        FAIL_SET_JUMP(!method->flags.is_abstract, err, JERR_ABSTRACT, exit);


        if (!method->flags.is_native) {
            InterpreterFrame_t* new_frame = interpreter_frame_push(ctx, method);
            FAIL_SET_JUMP(new_frame, err, JERR_STACKOVERFLOW, exit);

            if(method->flags.is_syncronized){
                assert(ctx->thread && "Cannot be run from bootstrap context");
                FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - method->args_slots])) == JERR_OK, err, err, exit);
            }

            int32_t* args = &frame->stack[frame->sp -= method->args_slots];
            memcpy(new_frame->locals, args, method->args_slots * sizeof(int32_t));

            frame = new_frame;
            goto *opcode_labels[*frame->pc];
        } else {
            FAIL_SET_JUMP((err = native_method_invoke(ctx, frame, method)) == JERR_OK, err, err, exit);
            NEXT();
        }
    }
    //========================================


    // ========== CONDITIONAL BRANCHES (int) ==========
    #define IF_CMP(OP) ({ \
        thread_safepoint_check();\
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
    #define BINARY_INT(OP) ({ \
        int32_t v2 = STACK_POP_INT(frame); \
        int32_t v1 = STACK_POP_INT(frame); \
        STACK_PUSH_INT(frame, v1 OP v2); \
        NEXT(); \
    })

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
    EJOPCODE_GOTO:{
        thread_safepoint_check();

        frame->pc += (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_GOTO_W:{
        thread_safepoint_check();

        frame->pc += (int32_t)be32_to_cpu(*(int32_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_JSR:{
        thread_safepoint_check();

        STACK_PUSH_INT(frame, (int32_t)(uintptr_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc])));
        frame->pc += (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_JSR_W:{
        thread_safepoint_check();

        STACK_PUSH_INT(frame, (int32_t)(uintptr_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc])));
        frame->pc += (int32_t)be32_to_cpu(*(int32_t*)(frame->pc + 1));
        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_RET:{
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
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);

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

            case SYMBOL_CLASS:
                STACK_PUSH_REF(frame, ((Class_t*)sym->value)->class_object);
                break;
        }

        NEXT();
    }

    EJOPCODE_LDC2_W:
    EJOPCODE_LDC_W:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);

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

            case SYMBOL_CLASS:
                STACK_PUSH_REF(frame, ((Class_t*)sym->value)->class_object);
                break;
        }

        NEXT();
    }

    EJOPCODE_NEW:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);

        Class_t* class = sym->value;
        FAIL_SET_JUMP((err = run_clinit(ctx, class)) == JERR_OK, err, err, exit);

        FAIL_SET_JUMP(!class->flags.is_abstract, err, JERR_INSTANTIATION, exit);

        Object_t* object = 0;
        FAIL_SET_JUMP((err = heap_class_object_alloc(class, &object)) == JERR_OK, err, err, exit); 

        STACK_PUSH_REF(frame, object);
        NEXT();
    }

    EJOPCODE_NEWARRAY:{
        thread_safepoint_check();

        uint8_t type = *(uint8_t*)(frame->pc + 1);
        int32_t length = STACK_POP_INT(frame);

        FAIL_SET_JUMP(length >= 0, err, JERR_NEGATIVESIZE, exit);

        JavaValueType_t type_mapping[] = {[4] = TYPE_BOOL,
                                          [5] = TYPE_CHAR,
                                          [6] = TYPE_FLOAT,
                                          [7] = TYPE_DOUBLE,
                                          [8] = TYPE_BYTE,
                                          [9] = TYPE_SHORT,
                                          [10] = TYPE_INT,
                                          [11] = TYPE_LONG,};

        Object_t* array = NULL;
        Class_t* array_class = NULL;
        char class_name[3] = {'[',type_mapping[type],'\0'};
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add(class_name), &array_class)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_alloc(array_class, length, &array)) == JERR_OK, err, err, exit);

        STACK_PUSH_REF(frame, array);

        NEXT();
    }

    EJOPCODE_ANEWARRAY:{
        thread_safepoint_check();

        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);

        Class_t* element_class = sym->value;
        int32_t length = STACK_POP_INT(frame);

        FAIL_SET_JUMP(length >= 0, err, JERR_NEGATIVESIZE, exit);

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
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);
        
        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);

        STACK_PUSH_INT(frame, length);
        NEXT();
    }

    EJOPCODE_IALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int32_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_INT(frame, array[index]);

        NEXT();
    }

    EJOPCODE_LALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int64_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_LONG(frame, array[index]);

        NEXT();
    }

    EJOPCODE_FALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        float* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_FLOAT(frame, array[index]);

        NEXT();
    }

    EJOPCODE_DALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        double* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_DOUBLE(frame, array[index]);

        NEXT();
    }

    EJOPCODE_AALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        Object_t** array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_REF(frame, array[index]);

        NEXT();
    }

    EJOPCODE_BALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int8_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_CALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        uint16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_SALOAD:{
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        STACK_PUSH_INT(frame, (int32_t)array[index]);

        NEXT();
    }

    EJOPCODE_IASTORE:{
        int32_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int32_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_FASTORE:{
        float value = STACK_POP_FLOAT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        float* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_LASTORE:{
        int64_t value = STACK_POP_LONG(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int64_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }
    
    EJOPCODE_DASTORE:{
        double value = STACK_POP_DOUBLE(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        double* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_BASTORE:{
        int8_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int8_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }
    
    EJOPCODE_SASTORE:{
        int16_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        int16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }
    EJOPCODE_CASTORE:{
        uint16_t value = STACK_POP_INT(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        uint16_t* array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
        array[index] = value;

        NEXT();
    }

    EJOPCODE_AASTORE:{
        Object_t* value = STACK_POP_REF(frame);
        int32_t index = STACK_POP_INT(frame);
        Object_t* object = STACK_POP_REF(frame);

        Object_t** array = NULL;
        int32_t length = 0;
        FAIL_SET_JUMP(object, err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP((err = heap_array_object_get_length(object, &length)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_array_object_get_elements(object, (void**)&array)) == JERR_OK, err, err, exit);
    
        FAIL_SET_JUMP(index < length, err, JERR_INDEXOOB, exit);
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

    EJOPCODE_MONITORENTER:{
        assert(ctx->thread && "Cannot be run from bootstrap context");

        FAIL_SET_JUMP(SHADOW_GET_REF(frame->shadow_stack, frame->sp - 1), err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((err = monitor_enter((Object_t*)frame->stack[frame->sp - 1])) == JERR_OK, err, err, exit);
       
        frame->sp--;
        NEXT();
    }

    EJOPCODE_MONITOREXIT:{
        assert(ctx->thread && "Cannot be run from bootstrap context");
        
        FAIL_SET_JUMP(SHADOW_GET_REF(frame->shadow_stack, frame->sp - 1), err, JERR_TYPECHECK_FAILURE, exit);
        FAIL_SET_JUMP((Object_t*)frame->stack[frame->sp - 1], err, JERR_NULLPOINTER, exit);

        FAIL_SET_JUMP(((err = monitor_exit(((Object_t*)frame->stack[frame->sp - 1])->monitor)) == JERR_OK), err, err, exit);
       
        frame->sp--;
        NEXT();
    }

    EJOPCODE_INSTANCEOF:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);

        Object_t* object = STACK_POP_REF(frame);
        if(!object || (object && !class_is_compatible(object->class, sym->value))) STACK_PUSH_INT(frame, 0);
        else STACK_PUSH_INT(frame, 1);

        NEXT();
    }

    EJOPCODE_CHECKCAST:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*(uint16_t*)(frame->pc + 1))];
        FAIL_SET_JUMP((err = class_resolv_symbol(ctx, sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_CLASS, err, JERR_BADPARAM, exit);
        
        Object_t* object = STACK_POP_REF(frame);
        FAIL_SET_JUMP(object && class_is_compatible(object->class, sym->value), err, JERR_CAST, exit);
        STACK_PUSH_REF(frame, object);

        NEXT();
    }

    EJOPCODE_ATHROW:{
        thread_safepoint_check();

        Object_t* exception = STACK_POP_REF(frame);
        err = exception ? JERR_EXCEPTION : JERR_NULLPOINTER;
    }

    EJOPCODE_NOP:
        NEXT();

    EJOPCODE_POP2: {
        if (frame->sp < 2) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        frame->sp -= 2;
        SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp);
        SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp + 1);
        NEXT();
    }

    EJOPCODE_DUP_X1: {
        // ... v2, v1 -> ... v1, v2, v1
        if (frame->sp < 2) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1; // v1
        int idx2 = frame->sp - 2; // v2
        int32_t v1 = frame->stack[idx1];
        int32_t v2 = frame->stack[idx2];
        int ref1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int ref2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        frame->sp++;
        frame->stack[frame->sp - 3] = v1;
        frame->stack[frame->sp - 2] = v2;
        frame->stack[frame->sp - 1] = v1;
        if (ref1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 3);
        else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 3);
        if (ref2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 2);
        else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 2);
        if (ref1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 1);
        else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 1);
        NEXT();
    }

    EJOPCODE_DUP_X2: {
        if (frame->sp < 3) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1;
        int idx2 = frame->sp - 2;
        int idx3 = frame->sp - 3;
        int32_t v1 = frame->stack[idx1];
        int32_t v2 = frame->stack[idx2];
        int32_t v3 = frame->stack[idx3];
        int r1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int r2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        int r3 = SHADOW_GET_REF(frame->shadow_stack, idx3);
        frame->sp++;
        frame->stack[frame->sp - 4] = v1;
        frame->stack[frame->sp - 3] = v3;
        frame->stack[frame->sp - 2] = v2;
        frame->stack[frame->sp - 1] = v1;
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 4); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 4);
        if (r3) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 3); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 3);
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 2); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 2);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 1); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 1);
        NEXT();
    }

    EJOPCODE_DUP2: {
        // ... v2, v1 -> ... v2, v1, v2, v1
        if (frame->sp < 2) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1;
        int idx2 = frame->sp - 2;
        int32_t v1 = frame->stack[idx1];
        int32_t v2 = frame->stack[idx2];
        int r1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int r2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        frame->sp += 2;
        frame->stack[frame->sp - 4] = v2;
        frame->stack[frame->sp - 3] = v1;
        frame->stack[frame->sp - 2] = v2;
        frame->stack[frame->sp - 1] = v1;
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 4); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 4);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 3); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 3);
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 2); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 2);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 1); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 1);
        NEXT();
    }

    EJOPCODE_DUP2_X1: {
        if (frame->sp < 3) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1;
        int idx2 = frame->sp - 2;
        int idx3 = frame->sp - 3;
        int32_t v1 = frame->stack[idx1];
        int32_t v2 = frame->stack[idx2];
        int32_t v3 = frame->stack[idx3];
        int r1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int r2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        int r3 = SHADOW_GET_REF(frame->shadow_stack, idx3);
        frame->sp += 2;
        frame->stack[frame->sp - 5] = v2;
        frame->stack[frame->sp - 4] = v1;
        frame->stack[frame->sp - 3] = v3;
        frame->stack[frame->sp - 2] = v2;
        frame->stack[frame->sp - 1] = v1;
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 5); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 5);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 4); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 4);
        if (r3) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 3); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 3);
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 2); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 2);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 1); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 1);
        NEXT();
    }

    EJOPCODE_DUP2_X2: {
        if (frame->sp < 4) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1;
        int idx2 = frame->sp - 2;
        int idx3 = frame->sp - 3;
        int idx4 = frame->sp - 4;
        int32_t v1 = frame->stack[idx1];
        int32_t v2 = frame->stack[idx2];
        int32_t v3 = frame->stack[idx3];
        int32_t v4 = frame->stack[idx4];
        int r1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int r2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        int r3 = SHADOW_GET_REF(frame->shadow_stack, idx3);
        int r4 = SHADOW_GET_REF(frame->shadow_stack, idx4);
        frame->sp += 2;
        frame->stack[frame->sp - 6] = v2;
        frame->stack[frame->sp - 5] = v1;
        frame->stack[frame->sp - 4] = v4;
        frame->stack[frame->sp - 3] = v3;
        frame->stack[frame->sp - 2] = v2;
        frame->stack[frame->sp - 1] = v1;
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 6); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 6);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 5); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 5);
        if (r4) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 4); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 4);
        if (r3) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 3); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 3);
        if (r2) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 2); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 2);
        if (r1) SHADOW_SET_REF(frame->shadow_stack, frame->sp - 1); else SHADOW_CLEAR_REF(frame->shadow_stack, frame->sp - 1);
        NEXT();
    }

    EJOPCODE_SWAP: {
        if (frame->sp < 2) { err = JERR_TYPECHECK_FAILURE; goto exit; }
        int idx1 = frame->sp - 1;
        int idx2 = frame->sp - 2;
        int32_t tmp = frame->stack[idx1];
        frame->stack[idx1] = frame->stack[idx2];
        frame->stack[idx2] = tmp;
        int ref1 = SHADOW_GET_REF(frame->shadow_stack, idx1);
        int ref2 = SHADOW_GET_REF(frame->shadow_stack, idx2);
        if (ref2) SHADOW_SET_REF(frame->shadow_stack, idx1); else SHADOW_CLEAR_REF(frame->shadow_stack, idx1);
        if (ref1) SHADOW_SET_REF(frame->shadow_stack, idx2); else SHADOW_CLEAR_REF(frame->shadow_stack, idx2);
        NEXT();
    }

    EJOPCODE_INEG: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, -v);
        NEXT();
    }

    EJOPCODE_LNEG: {
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, -v);
        NEXT();
    }

    EJOPCODE_FNEG: {
        float v = STACK_POP_FLOAT(frame);
        STACK_PUSH_FLOAT(frame, -v);
        NEXT();
    }

    EJOPCODE_DNEG: {
        double v = STACK_POP_DOUBLE(frame);
        STACK_PUSH_DOUBLE(frame, -v);
        NEXT();
    }

    EJOPCODE_ISHL: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, v1 << (v2 & 0x1F));
        NEXT();
    }
    EJOPCODE_ISHR: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, v1 >> (v2 & 0x1F));
        NEXT();
    }
    EJOPCODE_IUSHR: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, (int32_t)((uint32_t)v1 >> (v2 & 0x1F)));
        NEXT();
    }

    EJOPCODE_LSHL: {
        int32_t shift = STACK_POP_INT(frame);
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, v << (shift & 0x3F));
        NEXT();
    }
    EJOPCODE_LSHR: {
        int32_t shift = STACK_POP_INT(frame);
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, v >> (shift & 0x3F));
        NEXT();
    }
    EJOPCODE_LUSHR: {
        int32_t shift = STACK_POP_INT(frame);
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, (int64_t)((uint64_t)v >> (shift & 0x3F)));
        NEXT();
    }

    EJOPCODE_IAND: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, v1 & v2);
        NEXT();
    }
    EJOPCODE_IOR: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, v1 | v2);
        NEXT();
    }
    EJOPCODE_IXOR: {
        int32_t v2 = STACK_POP_INT(frame);
        int32_t v1 = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, v1 ^ v2);
        NEXT();
    }

    EJOPCODE_LAND: {
        int64_t v2 = STACK_POP_LONG(frame);
        int64_t v1 = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, v1 & v2);
        NEXT();
    }
    EJOPCODE_LOR: {
        int64_t v2 = STACK_POP_LONG(frame);
        int64_t v1 = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, v1 | v2);
        NEXT();
    }
    EJOPCODE_LXOR: {
        int64_t v2 = STACK_POP_LONG(frame);
        int64_t v1 = STACK_POP_LONG(frame);
        STACK_PUSH_LONG(frame, v1 ^ v2);
        NEXT();
    }

    // int -> long, float, double
    EJOPCODE_I2L: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_LONG(frame, (int64_t)v);
        NEXT();
    }
    EJOPCODE_I2F: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_FLOAT(frame, (float)v);
        NEXT();
    }
    EJOPCODE_I2D: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_DOUBLE(frame, (double)v);
        NEXT();
    }

    // long -> int, float, double
    EJOPCODE_L2I: {
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_INT(frame, (int32_t)v);
        NEXT();
    }
    EJOPCODE_L2F: {
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_FLOAT(frame, (float)v);
        NEXT();
    }
    EJOPCODE_L2D: {
        int64_t v = STACK_POP_LONG(frame);
        STACK_PUSH_DOUBLE(frame, (double)v);
        NEXT();
    }

    // float -> int, long, double
    EJOPCODE_F2I: {
        float v = STACK_POP_FLOAT(frame);
        STACK_PUSH_INT(frame, (int32_t)v);
        NEXT();
    }
    EJOPCODE_F2L: {
        float v = STACK_POP_FLOAT(frame);
        STACK_PUSH_LONG(frame, (int64_t)v);
        NEXT();
    }
    EJOPCODE_F2D: {
        float v = STACK_POP_FLOAT(frame);
        STACK_PUSH_DOUBLE(frame, (double)v);
        NEXT();
    }

    // double -> int, long, float
    EJOPCODE_D2I: {
        double v = STACK_POP_DOUBLE(frame);
        STACK_PUSH_INT(frame, (int32_t)v);
        NEXT();
    }
    EJOPCODE_D2L: {
        double v = STACK_POP_DOUBLE(frame);
        STACK_PUSH_LONG(frame, (int64_t)v);
        NEXT();
    }
    EJOPCODE_D2F: {
        double v = STACK_POP_DOUBLE(frame);
        STACK_PUSH_FLOAT(frame, (float)v);
        NEXT();
    }

    // int -> byte, char, short (сужающие)
    EJOPCODE_I2B: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, (int32_t)(int8_t)v);
        NEXT();
    }
    EJOPCODE_I2C: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, (int32_t)(uint16_t)v);
        NEXT();
    }
    EJOPCODE_I2S: {
        int32_t v = STACK_POP_INT(frame);
        STACK_PUSH_INT(frame, (int32_t)(int16_t)v);
        NEXT();
    }

    EJOPCODE_LCMP: {
        int64_t v2 = STACK_POP_LONG(frame);
        int64_t v1 = STACK_POP_LONG(frame);
        int32_t result;
        if (v1 > v2) result = 1;
        else if (v1 == v2) result = 0;
        else result = -1;
        STACK_PUSH_INT(frame, result);
        NEXT();
    }

    EJOPCODE_FCMPL: {
        float v2 = STACK_POP_FLOAT(frame);
        float v1 = STACK_POP_FLOAT(frame);
        int32_t result;
        if (isnan(v1) || isnan(v2)) result = -1;
        else if (v1 > v2) result = 1;
        else if (v1 == v2) result = 0;
        else result = -1;
        STACK_PUSH_INT(frame, result);
        NEXT();
    }
    EJOPCODE_FCMPG: {
        float v2 = STACK_POP_FLOAT(frame);
        float v1 = STACK_POP_FLOAT(frame);
        int32_t result;
        if (isnan(v1) || isnan(v2)) result = 1;
        else if (v1 > v2) result = 1;
        else if (v1 == v2) result = 0;
        else result = -1;
        STACK_PUSH_INT(frame, result);
        NEXT();
    }

    EJOPCODE_DCMPL: {
        double v2 = STACK_POP_DOUBLE(frame);
        double v1 = STACK_POP_DOUBLE(frame);
        int32_t result;
        if (isnan(v1) || isnan(v2)) result = -1;
        else if (v1 > v2) result = 1;
        else if (v1 == v2) result = 0;
        else result = -1;
        STACK_PUSH_INT(frame, result);
        NEXT();
    }
    EJOPCODE_DCMPG: {
        double v2 = STACK_POP_DOUBLE(frame);
        double v1 = STACK_POP_DOUBLE(frame);
        int32_t result;
        if (isnan(v1) || isnan(v2)) result = 1;
        else if (v1 > v2) result = 1;
        else if (v1 == v2) result = 0;
        else result = -1;
        STACK_PUSH_INT(frame, result);
        NEXT();
    }

    EJOPCODE_IF_ACMPEQ:{
        thread_safepoint_check();

        void* ref2 = STACK_POP_REF(frame);
        void* ref1 = STACK_POP_REF(frame);
        int16_t offset = (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        if (ref1 == ref2) {
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        }
        NEXT();
    }

    EJOPCODE_IF_ACMPNE:{
        thread_safepoint_check();

        void* ref2 = STACK_POP_REF(frame);
        void* ref1 = STACK_POP_REF(frame);
        int16_t offset = (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1));
        if (ref1 != ref2) {
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        }
        NEXT();
    }

    #define IF_ZERO(OP) ({ \
        thread_safepoint_check();\
        int32_t v = STACK_POP_INT(frame); \
        int16_t offset = (int16_t)be16_to_cpu(*(int16_t*)(frame->pc + 1)); \
        if (v OP 0) { \
            frame->pc += offset; \
            goto *opcode_labels[*frame->pc]; \
        } \
        NEXT(); \
    })

    EJOPCODE_IFEQ:
        IF_ZERO(==);

    EJOPCODE_IFNE:
        IF_ZERO(!=);

    EJOPCODE_IFLT:
        IF_ZERO(<);

    EJOPCODE_IFGE:
        IF_ZERO(>=);

    EJOPCODE_IFGT:
        IF_ZERO(>);

    EJOPCODE_IFLE:
        IF_ZERO(<=);

    EJOPCODE_INTERPRETEREXIT:
        return err;

exit:
    if(err >= JERR_NOCLASSDEF && err < JERR_UNKNOWN && err != JERR_OOM){
        Object_t* exception = err == JERR_EXCEPTION ? STACK_POP_REF(frame) : NULL;
        if(!exception){
            Class_t* exception_class = NULL;
            FAIL_SET_JUMP((err = class_load_bynameid(error_to_exception_nameid(err), &exception_class)) == JERR_OK, err, err, exit); //Oh, this is cursed
            FAIL_SET_JUMP((err = heap_class_object_alloc(exception_class, &exception)) == JERR_OK, err, err, exit);
            FAIL_SET_JUMP((err = interpreter_method_invoke(ctx, class_find_method(exception_class, stringpool_add("<init>@()V")), NULL, NULL)) == JERR_OK, err, err, exit);            
        }

        FAIL_SET_JUMP((err = throw_exception(ctx, exception)) == JERR_OK, err, err, exit);
        frame = interpreter_frame_get(ctx);

        goto *opcode_labels[*frame->pc];
    }

    //Unlock all monitors in case of diyng
    for(InterpreterFrame_t* cur = frame; cur; cur = cur->prev){
        frame_unlock_monitors(cur);
    }
    return err;
}