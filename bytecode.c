#include "bytecode.h"
#include "class.h"
#include "class_loader.h"
#include "lb_endian.h"
#include "linker.h"
#include "ljvm.h"
#include "thread.h"
#include <assert.h>
#include <stdint.h>

#include <math.h>

#define COGO_DEBUG

#ifndef COGO_DEBUG
#define NEXT_INSTRUCTION(instruction, jump_table) goto *(jump_table[++instruction]);
#else
#define NEXT_INSTRUCTION(instruction, jump_table) ({int new_instruction = instruction; assert(new_instruction < sizeof(jump_table)); assert(jump_table[new_instruction]); goto *jump_table[new_instruction];})
#endif



JError_t cogo_interpreter() {
    JError_t err = EJERR_OK;
    const void* jump_table[] = {
        [OP_NOP] = &&OP_NOP, 
        [OP_RETURN] = &&OP_VRETURN, [OP_ARETURN] = &&OP_VRETURN, [OP_IRETURN] = &&OP_VRETURN, 
        [OP_LRETURN] = &&OP_VRETURN, [OP_FRETURN] = &&OP_VRETURN, [OP_DRETURN] = &&OP_VRETURN,
        [OP_LDC] = &&OP_LDC, [OP_LDCw] = &&OP_LDCw, [OP_LDC2w] = &&OP_LDC2w,
        [OP_PUTSTATIC] = &&OP_PUTSTATIC, [OP_GETSTATIC] = &&OP_GETSTATIC,
        [OP_IADD] = &&OP_IADD, [OP_ISUB] = &&OP_ISUB, [OP_IMUL] = &&OP_IMUL,
        [OP_IDIV] = &&OP_IDIV, [OP_IREM] = &&OP_IREM, [OP_INEG] = &&OP_INEG,
        [OP_ISHL] = &&OP_ISHL, [OP_ISHR] = &&OP_ISHR, [OP_IUSHR] = &&OP_IUSHR,
        [OP_IAND] = &&OP_IAND, [OP_IOR] = &&OP_IOR, [OP_IXOR] = &&OP_IXOR,
        [OP_IINC] = &&OP_IINC,
        [OP_LADD] = &&OP_LADD, [OP_LSUB] = &&OP_LSUB, [OP_LMUL] = &&OP_LMUL,
        [OP_LDIV] = &&OP_LDIV, [OP_LREM] = &&OP_LREM, [OP_LNEG] = &&OP_LNEG,
        [OP_LSHL] = &&OP_LSHL, [OP_LSHR] = &&OP_LSHR, [OP_LUSHR] = &&OP_LUSHR,
        [OP_LAND] = &&OP_LAND, [OP_LOR] = &&OP_LOR, [OP_LXOR] = &&OP_LXOR,
        [OP_FADD] = &&OP_FADD, [OP_FSUB] = &&OP_FSUB, [OP_FMUL] = &&OP_FMUL,
        [OP_FDIV] = &&OP_FDIV, [OP_FREM] = &&OP_FREM, [OP_FNEG] = &&OP_FNEG,
        [OP_DADD] = &&OP_DADD, [OP_DSUB] = &&OP_DSUB, [OP_DMUL] = &&OP_DMUL,
        [OP_DDIV] = &&OP_DDIV, [OP_DREM] = &&OP_DREM, [OP_DNEG] = &&OP_DNEG,
        [OP_LCMP] = &&OP_LCMP, [OP_FCMPL] = &&OP_FCMPL, [OP_FCMPG] = &&OP_FCMPG,
        [OP_DCMPL] = &&OP_DCMPL, [OP_DCMPG] = &&OP_DCMPG,
        [OP_I2L] = &&OP_I2L, [OP_I2F] = &&OP_I2F, [OP_I2D] = &&OP_I2D,
        [OP_L2I] = &&OP_L2I, [OP_L2F] = &&OP_L2F, [OP_L2D] = &&OP_L2D,
        [OP_F2I] = &&OP_F2I, [OP_F2L] = &&OP_F2L, [OP_F2D] = &&OP_F2D,
        [OP_D2I] = &&OP_D2I, [OP_D2L] = &&OP_D2L, [OP_D2F] = &&OP_D2F,
    };

    JFrame_t* frame = thread_frame_get();
    if (!frame) return EJERR_OK; // No methods to execute
    
    JBytecode_t* bytecode = frame->method->userctx;

    // Initial jump into the loop
    NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);

    OP_NOP: {
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    OP_VRETURN: {
        uint64_t retval_buffer = 0; // Use a u64 buffer for simplicity
        
        switch (bytecode->code[frame->pc]) {
            case OP_IRETURN:
                thread_frame_stack_pop_u32(&retval_buffer);
                break;
            case OP_FRETURN:
                thread_frame_stack_pop_u32(&retval_buffer);
                break;
            case OP_LRETURN:
                thread_frame_stack_pop_u64(&retval_buffer);
                break;
            case OP_DRETURN:
                thread_frame_stack_pop_u64(&retval_buffer);
                break;
            case OP_ARETURN:
                // Use the dedicated, safe function for references
                thread_frame_stack_pop_reference(&retval_buffer);
                break;
            case OP_RETURN:
                // No value to pop for void return
                break;
        }
        // The new API automatically gets the return type from the method info
        return thread_method_return(&retval_buffer);
    }

    OP_LDC: {
        uint8_t cpu_index = bytecode->code[frame->pc + 1];
        JClass_constant_t* constant = &frame->method->owner->info->constant_pool.constants[cpu_index - 1];
        
        // Use the correct push function based on constant type
        if (constant->type == EJCT_string) {
            thread_frame_stack_push_reference(constant->value);
        } else {
            thread_frame_stack_push_u32(constant->value);
        }
        
        NEXT_INSTRUCTION(bytecode->code[(frame->pc += 2)], jump_table);
    }

    OP_LDCw: {
        uint16_t be_index = *(uint16_t*)&bytecode->code[frame->pc + 1];
        uint16_t cpu_index = be16_to_cpu(be_index);
        JClass_constant_t* constant = &frame->method->owner->info->constant_pool.constants[cpu_index - 1];

        // Use the correct push function based on constant type
        if (constant->type == EJCT_string) {
            thread_frame_stack_push_reference(constant->value);
        } else {
            thread_frame_stack_push_u32(constant->value);
        }

        NEXT_INSTRUCTION(bytecode->code[(frame->pc += 3)], jump_table);
    }

    OP_LDC2w: {
        uint16_t be_index = *(uint16_t*)&bytecode->code[frame->pc + 1];
        uint16_t cpu_index = be16_to_cpu(be_index);
        JClass_constant_t* constant = &frame->method->owner->info->constant_pool.constants[cpu_index - 1];
        
        thread_frame_stack_push_u64(constant->value);
        
        NEXT_INSTRUCTION(bytecode->code[(frame->pc += 3)], jump_table);
    }

    OP_PUTSTATIC: {
        uint16_t be_index = *(uint16_t*)&bytecode->code[frame->pc + 1];
        uint16_t cpu_index = be16_to_cpu(be_index);
        JField_t* field = frame->method->owner->info->constant_pool.constants[cpu_index - 1].value;
        FAIL_SET_JUMP(field, err, EJERR_NOT_FOUND, exit);
        
        void* static_field_ptr = class_get_staticfield(field);
        
        // Use the correct stack pop function based on the field's type
        if (field->type == EJVT_REFERENCE) {
            thread_frame_stack_pop_reference(static_field_ptr);
        } else if (field->type == EJVT_LONG || field->type == EJVT_DOUBLE) {
            thread_frame_stack_pop_u64(static_field_ptr);
        } else {
            thread_frame_stack_pop_u32(static_field_ptr);
        }
        
        frame->pc += 3;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    OP_GETSTATIC: {
        uint16_t be_index = *(uint16_t*)&bytecode->code[frame->pc + 1];
        uint16_t cpu_index = be16_to_cpu(be_index);
        JField_t* field = frame->method->owner->info->constant_pool.constants[cpu_index - 1].value;
        FAIL_SET_JUMP(field, err, EJERR_NOT_FOUND, exit);

        void* static_field_ptr = class_get_staticfield(field);
        
        // Use the correct stack push function based on the field's type
        if (field->type == EJVT_REFERENCE) {
            thread_frame_stack_push_reference(static_field_ptr);
        } else if (field->type == EJVT_LONG || field->type == EJVT_DOUBLE) {
            thread_frame_stack_push_u64(static_field_ptr);
        } else {
            thread_frame_stack_push_u32(static_field_ptr);
        }
        
        frame->pc += 3;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === INTEGER MATH (u32) ===
    OP_IADD: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a + b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_ISUB: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a - b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IMUL: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a * b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IDIV: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        FAIL_SET_JUMP(b != 0, err, EJERR_ARITHMETIC, exit);
        result = a / b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IREM: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        FAIL_SET_JUMP(b != 0, err, EJERR_ARITHMETIC, exit);
        result = a % b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_INEG: {
        int32_t a, result;
        thread_frame_stack_pop_u32(&a);
        result = -a;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_ISHL: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a << (b & 0x1F);
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_ISHR: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a >> (b & 0x1F);
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IUSHR: {
        uint32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a >> (b & 0x1F);
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IAND: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a & b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IOR: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a | b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IXOR: {
        int32_t a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a ^ b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_IINC: {
        uint8_t index = bytecode->code[frame->pc + 1];
        int8_t const_val = (int8_t)bytecode->code[frame->pc + 2];
        frame->locals[index] += const_val;
        frame->pc += 3;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === LONG MATH (u64) ===
    OP_LADD: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a + b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LSUB: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a - b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LMUL: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a * b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LDIV: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        FAIL_SET_JUMP(b != 0, err, EJERR_ARITHMETIC, exit);
        result = a / b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LREM: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        FAIL_SET_JUMP(b != 0, err, EJERR_ARITHMETIC, exit);
        result = a % b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LNEG: {
        int64_t a, result;
        thread_frame_stack_pop_u64(&a);
        result = -a;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LSHL: {
        int64_t a;
        int32_t b; // Shift amount is int (u32)
        int64_t result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u64(&a);
        result = a << (b & 0x3F);
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LSHR: {
        int64_t a;
        int32_t b;
        int64_t result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u64(&a);
        result = a >> (b & 0x3F);
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LUSHR: {
        uint64_t a;
        int32_t b;
        uint64_t result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u64(&a);
        result = a >> (b & 0x3F);
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LAND: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a & b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LOR: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a | b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_LXOR: {
        int64_t a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a ^ b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === FLOAT MATH (u32) ===
    OP_FADD: {
        float a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a + b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FSUB: {
        float a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a - b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FMUL: {
        float a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a * b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FDIV: {
        float a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = a / b;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FREM: {
        float a, b, result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        result = fmodf(a, b);
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FNEG: {
        float a, result;
        thread_frame_stack_pop_u32(&a);
        result = -a;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === DOUBLE MATH (u64) ===
    OP_DADD: {
        double a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a + b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DSUB: {
        double a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a - b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DMUL: {
        double a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a * b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DDIV: {
        double a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = a / b;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DREM: {
        double a, b, result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        result = fmod(a, b);
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DNEG: {
        double a, result;
        thread_frame_stack_pop_u64(&a);
        result = -a;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === COMPARISONS ===
    OP_LCMP: {
        int64_t a, b;
        int32_t result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        if (a > b) result = 1;
        else if (a < b) result = -1;
        else result = 0;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FCMPL: {
        float a, b;
        int32_t result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        if (isnan(a) || isnan(b)) result = -1;
        else if (a > b) result = 1;
        else if (a < b) result = -1;
        else result = 0;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_FCMPG: {
        float a, b;
        int32_t result;
        thread_frame_stack_pop_u32(&b);
        thread_frame_stack_pop_u32(&a);
        if (isnan(a) || isnan(b)) result = 1;
        else if (a > b) result = 1;
        else if (a < b) result = -1;
        else result = 0;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DCMPL: {
        double a, b;
        int32_t result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        if (isnan(a) || isnan(b)) result = -1;
        else if (a > b) result = 1;
        else if (a < b) result = -1;
        else result = 0;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_DCMPG: {
        double a, b;
        int32_t result;
        thread_frame_stack_pop_u64(&b);
        thread_frame_stack_pop_u64(&a);
        if (isnan(a) || isnan(b)) result = 1;
        else if (a > b) result = 1;
        else if (a < b) result = -1;
        else result = 0;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

    // === TYPE CONVERSIONS ===
    OP_I2L: {
        int32_t val;
        int64_t result;
        thread_frame_stack_pop_u32(&val);
        result = (int64_t)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_I2F: {
        int32_t val;
        float result;
        thread_frame_stack_pop_u32(&val);
        result = (float)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_I2D: {
        int32_t val;
        double result;
        thread_frame_stack_pop_u32(&val);
        result = (double)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_L2I: {
        int64_t val;
        int32_t result;
        thread_frame_stack_pop_u64(&val);
        result = (int32_t)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_L2F: {
        int64_t val;
        float result;
        thread_frame_stack_pop_u64(&val);
        result = (float)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_L2D: {
        int64_t val;
        double result;
        thread_frame_stack_pop_u64(&val);
        result = (double)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_F2I: {
        float val;
        int32_t result;
        thread_frame_stack_pop_u32(&val);
        if (isnan(val)) result = 0;
        else if (val >= 2147483647.0f) result = 2147483647;
        else if (val <= -2147483648.0f) result = -2147483648;
        else result = (int32_t)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_F2L: {
        float val;
        int64_t result;
        thread_frame_stack_pop_u32(&val);
        if (isnan(val)) result = 0;
        else if (val >= 9223372036854775807.0f) result = 9223372036854775807LL;
        else if (val <= -9223372036854775808.0f) result = -9223372036854775808LL;
        else result = (int64_t)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_F2D: {
        float val;
        double result;
        thread_frame_stack_pop_u32(&val);
        result = (double)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_D2I: {
        double val;
        int32_t result;
        thread_frame_stack_pop_u64(&val);
        if (isnan(val)) result = 0;
        else if (val >= 2147483647.0) result = 2147483647;
        else if (val <= -2147483648.0) result = -2147483648;
        else result = (int32_t)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_D2L: {
        double val;
        int64_t result;
        thread_frame_stack_pop_u64(&val);
        if (isnan(val)) result = 0;
        else if (val >= 9223372036854775807.0) result = 9223372036854775807LL;
        else if (val <= -9223372036854775808.0) result = -9223372036854775808LL;
        else result = (int64_t)val;
        thread_frame_stack_push_u64(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }
    OP_D2F: {
        double val;
        float result;
        thread_frame_stack_pop_u64(&val);
        result = (float)val;
        thread_frame_stack_push_u32(&result);
        frame->pc++;
        NEXT_INSTRUCTION(bytecode->code[frame->pc], jump_table);
    }

exit:
    return err;
}