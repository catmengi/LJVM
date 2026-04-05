#pragma once
#include <stdint.h>

typedef uint32_t JILInstruction_t;

//opcode is uint8_t command ID
//regpair is uint8_t with first half being register A, second half register B
//imm16 is uint16_t immediate value
//imm24 is 24bit value extended to be uint32_t


#define JIL_INSTR_GET_OPCODE(instr) ((uint32_t)(instr) & (uint32_t)0xFF)
#define JIL_INSTR_GET_REGPAIR_A(instr) (((instr) >> 8) & 0xFF) //Cannot be used with IMM24
#define JIL_INSTR_GET_REGPAIR_B(instr) (((instr) >> 16) & 0xFF) //Cannot be used with IMM16 and IMM24
#define JIL_INSTR_GET_REGPAIR_C(instr) (((instr) >> 24) & 0xFF) //Cannot be used with IMM16 and IMM24
#define JIL_INSTR_GET_IMM16(instr) (((instr) >> 16) & 0xFFFF) //Cannot be used with REGPAIR_C
#define JIL_INSTR_GET_IMM24(instr) (((instr) >> 8) & 0xFFFFFF) //Cannot be used with IMM16 and REGPAIRs
#define JIL_INSTR_GET_IMM24_SIGNED(instr) ((int32_t)(((instr) >> 8) & 0xFFFFFF) | (((((instr) >> 8) & 0x800000) ? 0xFF000000 : 0))) //Cannot be used with IMM16 and REGPAIRs
#define JIL_REGPAIR_GET_REG_A(regpair)((regpair >> 4) & 0xF)
#define JIL_REGPAIR_GET_REG_B(regpair)((regpair) & 0xF)

//DO NOT FUCKING SET THE SAME FIELD TWICE!!!!!
#define JIL_INSTR_SET_OPCODE(instr, opcode) ((instr) |= (opcode))
#define JIL_INSTR_SET_REGPAIR_A(instr, regpair) ((instr) |= ((regpair) << 8)) 
#define JIL_INSTR_SET_REGPAIR_B(instr, regpair) ((instr) |= ((regpair) << 16)) 
#define JIL_INSTR_SET_REGPAIR_C(instr, regpair) ((instr) |= ((regpair) << 24))
#define JIL_INSTR_SET_IMM16(instr, imm16) ((instr) |= ((imm16) << 16))
#define JIL_INSTR_SET_IMM24(instr, imm24) ((instr) |= ((imm24) << 8))
#define JIL_REGPAIR_SET_REG_A(regpair, regnum) ((regpair) |= ((regnum) << 4))
#define JIL_REGPAIR_SET_REG_B(regpair, regnum) ((regpair) |= (regnum))
//========================================================

#define JIL_INSTR_NULLIFY(instr) ((instr) = 0)