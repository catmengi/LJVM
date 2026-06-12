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

void thread_start(Thread_t* thread, Method_t* method, int32_t* args){
    thread->state = THREAD_ACTIVE;
    thread->opcode_quota = THREAD_LOWEST_QUOTA * THREAD_DEFAULT_PRIORITY;
    
    CallFrame_t* base_frame = thread_frame_push(thread, method);
    assert(base_frame);

    memcpy(base_frame->locals, args, method->arguments_size * sizeof(int32_t));

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
    size_t frame_size = (bytecode->max_locals * sizeof(int32_t)) + (bytecode->max_stack * sizeof(int32_t)) + sizeof(CallFrame_t);

    char* frame_memory = bumper_calloc(&thread->frame_allocator, 1, frame_size);
    if(frame_memory){
        CallFrame_t* frame = (void*)frame_memory;
        frame->frame_size = frame_size;
        frame->locals = (void*)(frame_memory + sizeof(CallFrame_t));
        frame->stack = (void*)(frame_memory + (bytecode->max_locals * sizeof(int32_t)) + sizeof(CallFrame_t));
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

static Error_t native_method_invoke(Thread_t* thread, CallFrame_t* frame, Method_t* method){
    Error_t err = JERR_OK;
    
    int32_t* params = frame->stack -= method->arguments_size;

    NativeMethod_t native = natives_find(stringpool_get(method->class->name_id), stringpool_get(method->name_id));
    FAIL_SET_JUMP(native, err, JERR_NOTFOUND, exit);

    NativeMethodReturnValue_t retval = native(thread,method,params);
    FAIL_SET_JUMP(retval.err == JERR_OK, err, retval.err, exit);

    memcpy(frame->stack, retval.value, method->return_size * sizeof(int32_t));
    frame->stack += method->return_size;

exit:
    if(err == JERR_EXCEPTION){
        assert(0 && "TODO: exceptions in native methods");
    }
    return err;
}

Error_t java_method_invoke(Method_t* method, int32_t* arguments, void* return_value){
    Error_t err = JERR_OK;
    Thread_t thread = {0};
    
    INIT_LIST_HEAD(&thread.list);
    INIT_LIST_HEAD(&thread.joiners);

    thread.opcode_quota = 0;
    bumper_create_from(&thread.frame_allocator,thread.stackbuf, THREAD_STACK_SIZE);


    CallFrame_t* retstub = thread_frame_push(&thread, &(Method_t){.code = 
                                                &(MethodBytecode_t){.code_length = 2,
                                                                    .code = (uint8_t[2]){EJOPCODE_RETURN, EJOPCODE_RETURN},
                                                                    .max_stack = 2,
                                                                   }});

    CallFrame_t* frame = thread_frame_push(&thread, method);
    FAIL_SET_JUMP(frame, err, JERR_OOM, exit);
    memcpy(frame->locals, arguments, method->arguments_size * sizeof(int32_t));

    FAIL_SET_JUMP((err = interpret_bytecode(&thread)) == JERR_OK, err, err, exit);
    memcpy(return_value, (retstub->stack -= method->return_size), method->return_size * sizeof(int32_t));

exit:
    return err;
}

static Error_t interpret_bytecode(Thread_t* thread){
    Error_t err = JERR_OK;
    size_t opcodes_executed = 0;
    register CallFrame_t* frame = thread_frame_get(thread);

    void* opcode_labels[256] = {
        [EJOPCODE_ALOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_ALOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_ALOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_ALOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_ALOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_ILOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_ILOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_ILOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_ILOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_ILOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_FLOAD] = &&EJOPCODE_LOAD32,
        [EJOPCODE_FLOAD_0] = &&EJOPCODE_LOAD32_0,
        [EJOPCODE_FLOAD_1] = &&EJOPCODE_LOAD32_1,
        [EJOPCODE_FLOAD_2] = &&EJOPCODE_LOAD32_2,
        [EJOPCODE_FLOAD_3] = &&EJOPCODE_LOAD32_3,

        [EJOPCODE_ASTORE] = &&EJOPCODE_STORE32,
        [EJOPCODE_ASTORE_0] = &&EJOPCODE_STORE32_0,
        [EJOPCODE_ASTORE_1] = &&EJOPCODE_STORE32_1,
        [EJOPCODE_ASTORE_2] = &&EJOPCODE_STORE32_2,
        [EJOPCODE_ASTORE_3] = &&EJOPCODE_STORE32_3,

        [EJOPCODE_ISTORE] = &&EJOPCODE_STORE32,
        [EJOPCODE_ISTORE_0] = &&EJOPCODE_STORE32_0,
        [EJOPCODE_ISTORE_1] = &&EJOPCODE_STORE32_1,
        [EJOPCODE_ISTORE_2] = &&EJOPCODE_STORE32_2,
        [EJOPCODE_ISTORE_3] = &&EJOPCODE_STORE32_3,

        [EJOPCODE_FSTORE] = &&EJOPCODE_STORE32,
        [EJOPCODE_FSTORE_0] = &&EJOPCODE_STORE32_0,
        [EJOPCODE_FSTORE_1] = &&EJOPCODE_STORE32_1,
        [EJOPCODE_FSTORE_2] = &&EJOPCODE_STORE32_2,
        [EJOPCODE_FSTORE_3] = &&EJOPCODE_STORE32_3,

        [EJOPCODE_ICONST_0] = &&EJOPCODE_ICONST_0,
        [EJOPCODE_ICONST_1] = &&EJOPCODE_ICONST_1,
        [EJOPCODE_ICONST_2] = &&EJOPCODE_ICONST_2,
        [EJOPCODE_ICONST_3] = &&EJOPCODE_ICONST_3,
        [EJOPCODE_ICONST_4] = &&EJOPCODE_ICONST_4,
        [EJOPCODE_ICONST_5] = &&EJOPCODE_ICONST_5,
        [EJOPCODE_ICONST_M1] = &&EJOPCODE_ICONST_M1,

        [EJOPCODE_PUTSTATIC] = &&EJOPCODE_PUTSTATIC,
        [EJOPCODE_GETSTATIC] = &&EJOPCODE_GETSTATIC,

        [EJOPCODE_BIPUSH] = &&EJOPCODE_BIPUSH,
        [EJOPCODE_SIPUSH] = &&EJOPCODE_SIPUSH,

        [EJOPCODE_RETURN] = &&EJOPCODE_RETURN,
        [EJOPCODE_IRETURN] = &&EJOPCODE_IRETURN,
        [EJOPCODE_FRETURN] = &&EJOPCODE_FRETURN,
        [EJOPCODE_LRETURN] = &&EJOPCODE_LRETURN,
        [EJOPCODE_DRETURN] = &&EJOPCODE_DRETURN,
        [EJOPCODE_ARETURN] = &&EJOPCODE_ARETURN,

        [EJOPCODE_IF_ICMPEQ] = &&EJOPCODE_IF_ICMPEQ,
        [EJOPCODE_IF_ICMPNE] = &&EJOPCODE_IF_ICMPNE,
        [EJOPCODE_IF_ICMPGE] = &&EJOPCODE_IF_ICMPGE,
        [EJOPCODE_IF_ICMPGT] = &&EJOPCODE_IF_ICMPGT,
        [EJOPCODE_IF_ICMPLE] = &&EJOPCODE_IF_ICMPLE,
        [EJOPCODE_IF_ICMPLT] = &&EJOPCODE_IF_ICMPLT,

        [EJOPCODE_IADD] = &&EJOPCODE_IADD,
        [EJOPCODE_ISUB] = &&EJOPCODE_ISUB,
        [EJOPCODE_IMUL] = &&EJOPCODE_IMUL,
        [EJOPCODE_IDIV] = &&EJOPCODE_IDIV,
        [EJOPCODE_IREM] = &&EJOPCODE_IREM,

        [EJOPCODE_LADD] = &&EJOPCODE_LADD,
        [EJOPCODE_LSUB] = &&EJOPCODE_LSUB,
        [EJOPCODE_LMUL] = &&EJOPCODE_LMUL,
        [EJOPCODE_LDIV] = &&EJOPCODE_LDIV,
        [EJOPCODE_LREM] = &&EJOPCODE_LREM,

        [EJOPCODE_FADD] = &&EJOPCODE_FADD,
        [EJOPCODE_FSUB] = &&EJOPCODE_FSUB,
        [EJOPCODE_FMUL] = &&EJOPCODE_FMUL,
        [EJOPCODE_FDIV] = &&EJOPCODE_FDIV,
        [EJOPCODE_FREM] = &&EJOPCODE_FREM,

        [EJOPCODE_DADD] = &&EJOPCODE_DADD,
        [EJOPCODE_DSUB] = &&EJOPCODE_DSUB,
        [EJOPCODE_DMUL] = &&EJOPCODE_DMUL,
        [EJOPCODE_DDIV] = &&EJOPCODE_DDIV,
        [EJOPCODE_DREM] = &&EJOPCODE_DREM,

        [EJOPCODE_IINC] = &&EJOPCODE_IINC,
        [EJOPCODE_GOTO] = &&EJOPCODE_GOTO,
        [EJOPCODE_JSR] = &&EJOPCODE_JSR,
        [EJOPCODE_JSR_W] = &&EJOPCODE_JSR_W,
        [EJOPCODE_RET] = &&EJOPCODE_RET,

        [EJOPCODE_INVOKESTATIC] = &&EJOPCODE_INVOKESTATIC,
        [EJOPCODE_INVOKESPECIAL] = &&EJOPCODE_INVOKESPECIAL,
    };

    #define NEXT() ({if(opcodes_executed++ == thread->opcode_quota && thread->opcode_quota > 0) return JERR_SCHEDULE; goto *opcode_labels[*(frame->pc += (1 + JOpcode_args_sizes[*frame->pc]))];})


    goto *opcode_labels[*frame->pc];
    EJOPCODE_LOAD32:
        memcpy(frame->stack++, &frame->locals[*(frame->pc + 1)], sizeof(uint32_t));
        NEXT();

    
    EJOPCODE_LOAD32_0:
        memcpy(frame->stack++, &frame->locals[0], sizeof(uint32_t));
        NEXT();
    EJOPCODE_LOAD32_1:
        memcpy(frame->stack++, &frame->locals[1], sizeof(uint32_t));
        NEXT();
    EJOPCODE_LOAD32_2:
        memcpy(frame->stack++, &frame->locals[2], sizeof(uint32_t));
        NEXT();
    EJOPCODE_LOAD32_3:
        memcpy(frame->stack++, &frame->locals[3], sizeof(uint32_t));
        NEXT();

    EJOPCODE_STORE32:
        memcpy(&frame->locals[*(frame->pc + 1)], --frame->stack, sizeof(uint32_t));
        NEXT();

    EJOPCODE_STORE32_0:
        memcpy(&frame->locals[0], --frame->stack, sizeof(uint32_t));
        NEXT();
    EJOPCODE_STORE32_1:
        memcpy(&frame->locals[1], --frame->stack, sizeof(uint32_t));
        NEXT();
    EJOPCODE_STORE32_2:
        memcpy(&frame->locals[2], --frame->stack, sizeof(uint32_t));
        NEXT();
    EJOPCODE_STORE32_3:
        memcpy(&frame->locals[3], --frame->stack, sizeof(uint32_t));
        NEXT();


    EJOPCODE_ICONST_0:
        *(frame->stack++) = 0;
        NEXT();
    EJOPCODE_ICONST_1:
        *(frame->stack++) = 1;
        NEXT();
    EJOPCODE_ICONST_2:
        *(frame->stack++) = 2;
        NEXT();
    EJOPCODE_ICONST_3:
        *(frame->stack++) = 3;
        NEXT();
    EJOPCODE_ICONST_4:
        *(frame->stack++) = 4;
        NEXT();
    EJOPCODE_ICONST_5:
        *(frame->stack++) = 5;
        NEXT();

    EJOPCODE_ICONST_M1:
        *(frame->stack++) = -1;
        NEXT();



    EJOPCODE_BIPUSH:
        *(frame->stack++) = *(int8_t*)(frame->pc + 1);
        NEXT();
    
    EJOPCODE_SIPUSH:
        *(frame->stack++) = be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        NEXT();
    

    EJOPCODE_RETURN:
        if(!(frame = thread_frame_pop(thread))) goto exit; //Root method exit!
        NEXT();

    
    EJOPCODE_IRETURN:
    EJOPCODE_FRETURN:
    EJOPCODE_ARETURN:{
        uint32_t retval = 0;
        int32_t* dest = --frame->stack;
        memcpy(&retval, dest, sizeof(uint32_t));

        if((frame = thread_frame_pop(thread))){
            int32_t* target = frame->stack;

            memcpy(target, &retval, sizeof(uint32_t));
            frame->stack++;
            NEXT();
        } else return JERR_ORPHAN_RETURN;
    }

    EJOPCODE_LRETURN:
    EJOPCODE_DRETURN:{
        uint64_t retval = 0;
        int64_t* dest = (int64_t*)(frame->stack -= 2);
        memcpy(&retval, dest, sizeof(uint64_t));

        if((frame = thread_frame_pop(thread))){
            int64_t* target = (int64_t*)frame->stack;

            memcpy(target, &retval, sizeof(uint64_t));
            frame->stack += 2;

            NEXT();
        } else return JERR_ORPHAN_RETURN;
    }

    EJOPCODE_PUTSTATIC:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*((uint16_t*)(frame->pc + 1)))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;

        unsigned sz = field->type == TYPE_LONG || field->type == TYPE_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
        void* mem = (frame->method->class->static_fields_storage + field->offset);

        field->constantvalue = NULL;
        frame->stack -= (sz >> 2);
        memcpy(mem, frame->stack, sz);

        NEXT();
    }

    EJOPCODE_GETSTATIC:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*((uint16_t*)(frame->pc + 1)))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_FIELD, err, JERR_BADPARAM, exit);

        Field_t* field = sym->value;

        unsigned sz = field->type == TYPE_LONG || field->type == TYPE_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
        void* mem = (frame->method->class->static_fields_storage + field->offset);
        
        if(field->constantvalue){
            FAIL_SET_JUMP((err = class_resolv_symbol(field->constantvalue)) == JERR_OK, err, err, exit);
            memcpy(mem, field->constantvalue->type == SYMBOL_STRING ? &field->constantvalue->value : field->constantvalue->value, sz);

            field->constantvalue = NULL;
        }
        memcpy(frame->stack, mem, sz);
        frame->stack += (sz >> 2);

        NEXT();
    }

    EJOPCODE_INVOKESPECIAL:
    EJOPCODE_INVOKESTATIC:{
        ClassSymbol_t* sym = &frame->method->class->symtab.symbols[be16_to_cpu(*((uint16_t*)(frame->pc + 1)))];
        FAIL_SET_JUMP((err = class_resolv_symbol(sym)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP(sym->type == SYMBOL_METHOD, err, JERR_BADPARAM, exit);
        
        Method_t* method = sym->value;

        if(!method->flags.is_native){
            FAIL_SET_JUMP((frame = thread_frame_push(thread, method)), err, JERR_OOM, exit);
            memcpy(frame->locals, (frame->prev->stack -= method->arguments_size), method->arguments_size * sizeof(int32_t));

            goto *opcode_labels[*frame->pc];
        } else FAIL_SET_JUMP((err = native_method_invoke(thread, frame, method)) == JERR_OK, err, err, exit);

        NEXT();
    }

    EJOPCODE_IF_ICMPEQ:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 == value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPNE:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 != value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPLT:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 < value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPLE:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 <= value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPGT:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 > value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IF_ICMPGE:{
        int16_t offset = (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        if(value1 >= value2){
            frame->pc += offset;
            goto *opcode_labels[*frame->pc];
        } else NEXT();
    }

    EJOPCODE_IADD:{
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        *(frame->stack++) = value1 + value2;
        NEXT();
    }

    EJOPCODE_ISUB:{
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        *(frame->stack++) = value1 - value2;
        NEXT();
    }

    EJOPCODE_IMUL:{
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        *(frame->stack++) = value1 * value2;
        NEXT();
    }

    EJOPCODE_IDIV:{
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        *(frame->stack++) = value1 / value2;
        NEXT();
    }

    EJOPCODE_IREM:{
        int32_t value2 = *(--frame->stack);
        int32_t value1 = *(--frame->stack);

        *(frame->stack++) = value1 % value2;
        NEXT();
    }

    EJOPCODE_LADD:{
        int64_t value2 = *(int64_t*)(frame->stack -= 2);
        int64_t value1 = *(int64_t*)(frame->stack -= 2);

        *(int64_t*)(frame->stack) = value1 + value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_LSUB:{
        int64_t value2 = *(int64_t*)(frame->stack -= 2);
        int64_t value1 = *(int64_t*)(frame->stack -= 2);

        *(int64_t*)(frame->stack) = value1 - value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_LMUL:{
        int64_t value2 = *(int64_t*)(frame->stack -= 2);
        int64_t value1 = *(int64_t*)(frame->stack -= 2);

        *(int64_t*)(frame->stack) = value1 * value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_LDIV:{
        int64_t value2 = *(int64_t*)(frame->stack -= 2);
        int64_t value1 = *(int64_t*)(frame->stack -= 2);

        *(int64_t*)(frame->stack) = value1 / value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_LREM:{
        int64_t value2 = *(int64_t*)(frame->stack -= 2);
        int64_t value1 = *(int64_t*)(frame->stack -= 2);

        *(int64_t*)(frame->stack) = value1 % value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_FADD:{
        float value2 = *(float*)(--frame->stack);
        float value1 = *(float*)(--frame->stack);

        *(float*)(frame->stack++) = value1 + value2;
        NEXT();
    }

    EJOPCODE_FSUB:{
        float value2 = *(float*)(--frame->stack);
        float value1 = *(float*)(--frame->stack);

        *(float*)(frame->stack++) = value1 - value2;
        NEXT();
    }

    EJOPCODE_FMUL:{
        float value2 = *(float*)(--frame->stack);
        float value1 = *(float*)(--frame->stack);

        *(float*)(frame->stack++) = value1 * value2;
        NEXT();
    }

    EJOPCODE_FDIV:{
        float value2 = *(float*)(--frame->stack);
        float value1 = *(float*)(--frame->stack);

        *(float*)(frame->stack++) = value1 / value2;
        NEXT();
    }

    EJOPCODE_FREM:{
        float value2 = *(float*)(--frame->stack);
        float value1 = *(float*)(--frame->stack);

        *(float*)(frame->stack++) = fmod(value1, value2);
        NEXT();
    }

    EJOPCODE_DADD:{
        double value2 = *(double*)(frame->stack -= 2);
        double value1 = *(double*)(frame->stack -= 2);

        *(double*)(frame->stack) = value1 + value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_DSUB:{
        double value2 = *(double*)(frame->stack -= 2);
        double value1 = *(double*)(frame->stack -= 2);

        *(double*)(frame->stack) = value1 - value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_DMUL:{
        double value2 = *(double*)(frame->stack -= 2);
        double value1 = *(double*)(frame->stack -= 2);

        *(double*)(frame->stack) = value1 * value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_DDIV:{
        double value2 = *(double*)(frame->stack -= 2);
        double value1 = *(double*)(frame->stack -= 2);

        *(double*)(frame->stack) = value1 / value2;
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_DREM:{
        double value2 = *(double*)(frame->stack -= 2);
        double value1 = *(double*)(frame->stack -= 2);

        *(double*)(frame->stack) = fmod(value1,value2);
        
        frame->stack += 2;
        NEXT();
    }

    EJOPCODE_IINC:{
        uint8_t index = *(frame->pc + 1);
        int8_t constant = *(frame->pc + 2);

        frame->locals[index] += constant;
        NEXT();
    }

    EJOPCODE_GOTO:{
        frame->pc += (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));
        goto *opcode_labels[*frame->pc];
    }

    //Will not work on 64bit platforms
    EJOPCODE_JSR:{
        *(uint32_t*)(frame->stack++) = (uint32_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc]));
        frame->pc += (int16_t)be16_to_cpu(*((int16_t*)(frame->pc + 1)));

        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_JSR_W:{
        *(uint32_t*)(frame->stack++) = (uint32_t)(frame->pc + (1 + JOpcode_args_sizes[*frame->pc]));
        frame->pc += (int16_t)be32_to_cpu(*((int32_t*)(frame->pc + 1)));

        goto *opcode_labels[*frame->pc];
    }

    EJOPCODE_RET:{
        frame->pc = (uint8_t*)frame->locals[*(frame->pc + 1)];
        goto *opcode_labels[*frame->pc];
    }

    //==================================

exit:
    thread_kill(thread);
    return err;
}