#include "interpreter.h"
#include "encode.h"
#include "opcode.h"

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int JIL_init(JILInterpreter_t* interpreter, uint32_t* code){
    interpreter->code = code;

    interpreter->call_stack.sp = 0;
    interpreter->call_stack.size = JIL_CALLSTACK_FRAMES;
    interpreter->call_stack.call_stack = JIL_ALLOC_CALLSTACK_ENTRIES(interpreter->call_stack.size);
    
    return interpreter->call_stack.call_stack ? 0 : 1;
}

int JIL_execute(JILInterpreter_t* interpreter){
    uint32_t* code = interpreter->code;
    JILCallFrame_t frame = {0};

    uint32_t instr = 0;
    void* opcode_executors[] = {
        [EJIL_OPCODE_NOP] = &&EJIL_OPCODE_NOP,
        [EJIL_OPCODE_HALT] = &&EJIL_OPCODE_HALT,
        [EJIL_OPCODE_MOVZ] = &&EJIL_OPCODE_MOVZ,
        [EJIL_OPCODE_MOVI] = &&EJIL_OPCODE_MOVI,
        [EJIL_OPCODE_MOV] = &&EJIL_OPCODE_MOV, 
        [EJIL_OPCODE_LDR32] = &&EJIL_OPCODE_LDR32,
        [EJIL_OPCODE_LDR16] = &&EJIL_OPCODE_LDR16,
        [EJIL_OPCODE_LDR8] = &&EJIL_OPCODE_LDR8,
        [EJIL_OPCODE_STR32] = &&EJIL_OPCODE_STR32,
        [EJIL_OPCODE_STR16] = &&EJIL_OPCODE_STR16,
        [EJIL_OPCODE_STR8] = &&EJIL_OPCODE_STR8,
        [EJIL_OPCODE_ALDR32] = &&EJIL_OPCODE_ALDR32,
        [EJIL_OPCODE_ALDR16] = &&EJIL_OPCODE_ALDR16,
        [EJIL_OPCODE_ALDR8] = &&EJIL_OPCODE_ALDR8,
        [EJIL_OPCODE_ASTR32] = &&EJIL_OPCODE_ASTR32,
        [EJIL_OPCODE_ASTR16] = &&EJIL_OPCODE_ASTR16,
        [EJIL_OPCODE_ASTR8] = &&EJIL_OPCODE_ASTR8,
        [EJIL_OPCODE_SYSCALL] = &&EJIL_OPCODE_SYSCALL,
        [EJIL_OPCODE_JMP] = &&EJIL_OPCODE_JMP,
        [EJIL_OPCODE_BRZ] = &&EJIL_OPCODE_BRZ,
        [EJIL_OPCODE_BRNZ] = &&EJIL_OPCODE_BRNZ,
        [EJIL_OPCODE_BREQ] = &&EJIL_OPCODE_BREQ,
        [EJIL_OPCODE_BRNEQ] = &&EJIL_OPCODE_BRNEQ,
        [EJIL_OPCODE_BRGT] = &&EJIL_OPCODE_BRGT,
        [EJIL_OPCODE_BRLT] = &&EJIL_OPCODE_BRLT,
        [EJIL_OPCODE_BRGTE] = &&EJIL_OPCODE_BRGTE,
        [EJIL_OPCODE_BRLTE] = &&EJIL_OPCODE_BRLTE,
        [EJIL_OPCODE_ADD] = &&EJIL_OPCODE_ADD,
        [EJIL_OPCODE_SUB] = &&EJIL_OPCODE_SUB,
        [EJIL_OPCODE_MUL] = &&EJIL_OPCODE_MUL,
        [EJIL_OPCODE_DIV] = &&EJIL_OPCODE_DIV,
        [EJIL_OPCODE_REM] = &&EJIL_OPCODE_REM,
        [EJIL_OPCODE_FADD] = &&EJIL_OPCODE_FADD,
        [EJIL_OPCODE_FSUB] = &&EJIL_OPCODE_FSUB,
        [EJIL_OPCODE_FMUL] = &&EJIL_OPCODE_FMUL,
        [EJIL_OPCODE_FDIV] = &&EJIL_OPCODE_FDIV,
        [EJIL_OPCODE_FREM] = &&EJIL_OPCODE_FREM,
        [EJIL_OPCODE_SHR] = &&EJIL_OPCODE_SHR,
        [EJIL_OPCODE_SHL] = &&EJIL_OPCODE_SHL,
        [EJIL_OPCODE_AND] = &&EJIL_OPCODE_AND,
        [EJIL_OPCODE_OR] = &&EJIL_OPCODE_OR,
        [EJIL_OPCODE_XOR] = &&EJIL_OPCODE_XOR,
        [EJIL_OPCODE_CMPZ] = &&EJIL_OPCODE_CMPZ,
        [EJIL_OPCODE_CMPNZ] = &&EJIL_OPCODE_CMPNZ,
        [EJIL_OPCODE_CMPEQ] = &&EJIL_OPCODE_CMPEQ,
        [EJIL_OPCODE_CMPNEQ] = &&EJIL_OPCODE_CMPNEQ,
        [EJIL_OPCODE_CMPGT] = &&EJIL_OPCODE_CMPGT,
        [EJIL_OPCODE_CMPLT] = &&EJIL_OPCODE_CMPLT,
        [EJIL_OPCODE_CMPGTE] = &&EJIL_OPCODE_CMPGTE,
        [EJIL_OPCODE_CMPLTE] = &&EJIL_OPCODE_CMPLTE,
        [EJIL_OPCODE_CALLVOID] = &&EJIL_OPCODE_CALLVOID,
        [EJIL_OPCODE_CALL] = &&EJIL_OPCODE_CALL,
        [EJIL_OPCODE_RETVOID] = &&EJIL_OPCODE_RETVOID,
        [EJIL_OPCODE_RET] = &&EJIL_OPCODE_RET,
    };

    #define NEXT_OPCODE ({instr = code[frame.pc++]; goto *opcode_executors[JIL_INSTR_GET_OPCODE(instr)];})

    NEXT_OPCODE;

    EJIL_OPCODE_NOP:
        NEXT_OPCODE;

    EJIL_OPCODE_HALT:{
        printf("PC = %ld\n",frame.pc);
        for(unsigned i = 0; i < 16; i++){
            if(i % 4 == 0) printf("\n");

            printf("R%d = %ld, ",i, frame.regs[i]);
        }
        printf("\n");
    }
    //NEXT_OPCODE; //TODO: remove this
    return 0;

    EJIL_OPCODE_MOVZ:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        frame.regs[reg_a] = 0;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_MOVI:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        int16_t imm16 = JIL_INSTR_GET_IMM16(instr);

        frame.regs[reg_a] = imm16;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_MOV:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        frame.regs[reg_a] = frame.regs[reg_b];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_LDR32:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        void* addr = (void*)frame.regs[reg_b];

        frame.regs[reg_a] = *(int32_t*)(addr + imm16);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_LDR16:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        void* addr = (void*)frame.regs[reg_b];

        frame.regs[reg_a] = *(int16_t*)(addr + imm16);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_LDR8:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        void* addr = (void*)frame.regs[reg_b];

        frame.regs[reg_a] = *(int8_t*)(addr + imm16);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_STR32:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        void* addr = (void*)frame.regs[reg_b];

        *(int32_t*)(addr + imm16) = frame.regs[reg_a];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_STR16:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        void* addr = (void*)frame.regs[reg_b];

        *(int16_t*)(addr + imm16) = frame.regs[reg_a];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_STR8:{
        uint8_t regpair = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair);

        int16_t imm16 = (int16_t)JIL_INSTR_GET_IMM16(instr);

        int8_t input = frame.regs[reg_a];
        void* addr = (void*)frame.regs[reg_b];

        *(int8_t*)(addr + imm16) = input;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ALDR32:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        int32_t* output = &frame.regs[reg_a];
        void* addr = (void*)frame.regs[reg_b];

        *output = *(int32_t*)(addr + frame.regs[reg_c]);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ALDR16:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        void* addr = (void*)frame.regs[reg_b];

        frame.regs[reg_a] = *(int16_t*)(addr + frame.regs[reg_c]);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ALDR8:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        void* addr = (void*)frame.regs[reg_b];

        frame.regs[reg_a] = *(int8_t*)(addr + frame.regs[reg_c]);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ASTR32:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        void* addr = (void*)frame.regs[reg_b];

        *(int32_t*)(addr + frame.regs[reg_c]) = frame.regs[reg_a];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ASTR16:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        void* addr = (void*)frame.regs[reg_b];

        *(int16_t*)(addr + frame.regs[reg_c]) = frame.regs[reg_a];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ASTR8:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        void* addr = (void*)frame.regs[reg_b];

        *(int8_t*)(addr + frame.regs[reg_c]) = frame.regs[reg_a];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_SHR:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] >> frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_SHL:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] << frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_AND:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] & frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_OR:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] | frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_XOR:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] ^ frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_ADD:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] + frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_SUB:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] - frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_MUL:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] * frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_DIV:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] / frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_REM:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] % frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_FADD:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        *(float*)&frame.regs[reg_a] = *(float*)&frame.regs[reg_b] + *(float*)&frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_FSUB:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        *(float*)&frame.regs[reg_a] = *(float*)&frame.regs[reg_b] - *(float*)&frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_FMUL:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        *(float*)&frame.regs[reg_a] = *(float*)&frame.regs[reg_b] * *(float*)&frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_FDIV:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        *(float*)&frame.regs[reg_a] = *(float*)&frame.regs[reg_b] / *(float*)&frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_FREM:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        *(float*)&frame.regs[reg_a] = fmod(*(float*)&frame.regs[reg_b],*(float*)&frame.regs[reg_c]);
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPZ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);

        frame.regs[reg_a] = frame.regs[reg_b] == 0;
    }
    NEXT_OPCODE;
 
    EJIL_OPCODE_CMPNZ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);

        frame.regs[reg_a] = frame.regs[reg_b] != 0;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPEQ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] == frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPNEQ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] != frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPGT:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] > frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPLT:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] < frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPGTE:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] >= frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CMPLTE:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        frame.regs[reg_a] = frame.regs[reg_b] <= frame.regs[reg_c];
    }
    NEXT_OPCODE;

    EJIL_OPCODE_JMP:{
        int32_t offset = JIL_INSTR_GET_IMM24_SIGNED(instr);
        frame.pc += offset;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRZ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        
        frame.pc = frame.regs[reg_a] == 0 ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRNZ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        
        frame.pc = frame.regs[reg_a] != 0 ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BREQ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] == frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRNEQ:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] != frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRGT:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] > frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRLT:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] < frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRGTE:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] >= frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_BRLTE:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        int32_t offset = (int16_t)JIL_INSTR_GET_IMM16(instr);
        
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        
        frame.pc = frame.regs[reg_a] <= frame.regs[reg_b] ? frame.pc + offset : frame.pc;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_SYSCALL:{
        uint16_t syscall = JIL_INSTR_GET_IMM16(instr);

        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);

        frame.regs[reg_a] = interpreter->syscall[syscall](frame.regs[reg_b]);
        
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CALLVOID:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);

        uint32_t invoke_addr = frame.regs[reg_a];

        if(interpreter->call_stack.sp >= interpreter->call_stack.size){
            printf("DEBUG ERROR: call stack overflow!\n");
            goto EJIL_OPCODE_HALT;
        }
        JILCallFrame_t* store_location = &interpreter->call_stack.call_stack[interpreter->call_stack.sp++];
        memcpy(store_location, &frame, sizeof(*store_location));

        memset(&frame,0,sizeof(frame));
        frame.pc = invoke_addr;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_RETVOID:{
        if(interpreter->call_stack.sp == 0){
            printf("DEBUG ERROR: call stack underflow!\n");
            goto EJIL_OPCODE_HALT;
        }
        JILCallFrame_t* store_location = &interpreter->call_stack.call_stack[--interpreter->call_stack.sp];
        memcpy(&frame, store_location, sizeof(frame));
    }
    NEXT_OPCODE;

    EJIL_OPCODE_CALL:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t regpair_b = JIL_INSTR_GET_REGPAIR_B(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);
        uint8_t reg_c = JIL_REGPAIR_GET_REG_A(regpair_b);

        uint32_t invoke_addr = frame.regs[reg_a];

        if(interpreter->call_stack.sp >= interpreter->call_stack.size){
            printf("DEBUG ERROR: call stack overflow!\n");
            goto EJIL_OPCODE_HALT;
        }

        uint32_t arg = frame.regs[reg_b];

        JILCallFrame_t* store_location = &interpreter->call_stack.call_stack[interpreter->call_stack.sp++];
        memcpy(store_location, &frame, sizeof(*store_location));

        memset(&frame,0,sizeof(frame));
        frame.regs[reg_c] = arg;
        frame.pc = invoke_addr;
    }
    NEXT_OPCODE;

    EJIL_OPCODE_RET:{
        uint8_t regpair_a = JIL_INSTR_GET_REGPAIR_A(instr);
        uint8_t reg_a = JIL_REGPAIR_GET_REG_A(regpair_a);
        uint8_t reg_b = JIL_REGPAIR_GET_REG_B(regpair_a);

        if(interpreter->call_stack.sp == 0){
            printf("DEBUG ERROR: call stack underflow!\n");
            goto EJIL_OPCODE_HALT;
        }
        uint32_t retval = frame.regs[reg_a];

        JILCallFrame_t* store_location = &interpreter->call_stack.call_stack[--interpreter->call_stack.sp];
        memcpy(&frame, store_location, sizeof(frame));

        frame.regs[reg_b] = retval;
    }
    NEXT_OPCODE;
}