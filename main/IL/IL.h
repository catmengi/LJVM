#pragma once
#include <stdint.h>

typedef struct{
    uint32_t instruction;
}JILInstruction_t;

typedef struct __attribute__((__packed__)){
    unsigned reg_a:4;
    unsigned reg_b:4;
}JILRegpair_t;


//EJIL_REGPAIR_(A/B) is JILRegpair_t. EJIL_IMMEDIATE_P(1/2/3) is uint8_t, that assemble a bigger integer if in ().
//"none" should be ignored, possible set to 0
typedef enum{
    EJIL_OPCODE_NOP = 0,
    
    
    EJIL_OPCODE_LDR32 = 0xA0, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: a = mem[b + immediate];
    EJIL_OPCODE_STR32 = 0xA1, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3) :: mem[b + immediate] = a;
    EJIL_OPCODE_LDR16 = 0xA2, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: a = mem[b + immediate];
    EJIL_OPCODE_STR16 = 0xA3, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3) :: mem[b + immediate] = a;
    EJIL_OPCODE_LDR8 = 0xA4, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: a = mem[b + immediate];
    EJIL_OPCODE_STR8 = 0xA5, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3) :: mem[b + immediate] = a;

    EJIL_OPCODE_MOV = 0xB0, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), none, none. :: a = b;
    EJIL_OPCODE_MOVI = 0xB1, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3. :: a = immediate

    EJIL_OPCODE_ADD = 0xC0, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b + c;
    EJIL_OPCODE_SUB = 0xC1, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b - c;
    EJIL_OPCODE_MUL = 0xC2, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b * c;
    EJIL_OPCODE_DIV = 0xC3, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b / c;
    EJIL_OPCODE_REM = 0xC4, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b % c;

    EJIL_OPCODE_AND = 0xC5, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b & c;
    EJIL_OPCODE_XOR = 0xC6, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b ^ c;
    EJIL_OPCODE_OR = 0xC7, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b | c;
    EJIL_OPCODE_SHR = 0xC8, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b >> c;
    EJIL_OPCODE_SHL = 0xC9, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b << c;
    EJIL_OPCODE_NEG = 0xCA, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), none, none. :: a = -b;

    EJIL_OPCODE_FADD = 0xCB, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b + c;
    EJIL_OPCODE_FSUB = 0xCC, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b - c;
    EJIL_OPCODE_FMUL = 0xCD, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b * c;
    EJIL_OPCODE_FDIV = 0xCE, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b / c;
    EJIL_OPCODE_FREM = 0xCF, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b % c;    

    EJIL_OPCODE_CMPEQ = 0xD0, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b == c
    EJIL_OPCODE_CMPNEQ = 0xD1, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b != c
    EJIL_OPCODE_CMPGR = 0xD2, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b > c
    EJIL_OPCODE_CMPGREQ = 0xD3, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b >= c
    EJIL_OPCODE_CMPLS = 0xD4, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b < c
    EJIL_OPCODE_CMPLSEQ = 0xD5, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none. :: a = b <= c

    EJIL_OPCODE_DADD = 0xDB, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd + ef;
    EJIL_OPCODE_DSUB = 0xDC, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd - ef;
    EJIL_OPCODE_DMUL = 0xDD, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd * ef;
    EJIL_OPCODE_DDIV = 0xDE, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd / ef;
    EJIL_OPCODE_DREM = 0xDF, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd % ef;   

    EJIL_OPCODE_JMP = 0xE0, //EJIL_OPCODE, (EJIL_IMMEDIATE_P1, EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: set pc to (immediate)
    EJIL_OPCODE_BRZ = 0xE1, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: if a == 0 set pc to (immediate)
    EJIL_OPCODE_BRNZ = 0xE2, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: if a != 0 set pc to (immediate)
    EJIL_OPCODE_JMPREG = 0xE3, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), none, none. :: set pc to reg a.

    EJIL_OPCODE_CALL = 0xF0, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), EJIL_REGPAIR_B(b, c), none.
    EJIL_OPCODE_RET = 0xF1, //EJIL_OPCODE, EJIL_REGPAIR_B(a, b), none, none. 
    EJIL_OPCODE_SYSCALL = 0xF2, //EJIL_OPCODE, EJIL_REGPAIR(a, b), (EJIL_IMMEDIATE_P2, EJIL_IMMEDIATE_P3). :: invoke syscall number from immediate, with argument from a. Return value is written to b
    EJIL_OPCODE_RETV = 0xF3, //EJIL_OPCODE, none, none, none. 
    EJIL_OPCODE_CALLV = 0xF4, //EJIL_OPCODE, EJIL_REGPAIR_A(a, (none)), none, none. 


    EJIL_OPCODE_LADD = 0xFB, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd + ef;
    EJIL_OPCODE_LSUB = 0xFC, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd - ef;
    EJIL_OPCODE_LMUL = 0xFD, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd * ef;
    EJIL_OPCODE_LDIV = 0xFE, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd / ef;
    EJIL_OPCODE_LREM = 0xFF, //EJIL_OPCODE, EJIL_REGPAIR_A(a, b), EJIL_REGPAIR_B(c, d), EJIL_REGPAIR_B(e, f). :: ab = cd % ef;  

    //CALL / RET logic: CALL a, b, c :: a - function address. b - input argument register. c - output argument register. c (in new frame) will be the value of b
    //                  RET a, b ::  a - register with return value, b - where to write return value in previous frame 
    //                  RETV is basically void return 
    //                  CALLV a :: calls function on address a
}JILOpcode_t;

enum{
    EJIL_SYSCALL_NEW = 0xA0,
    EJIL_SYSCALL_GETTHREAD,
    EJIL_SYSCALL_CHECKSAFEPOINT,
    EJIL_SYSCALL_MONITORENTER,
    EJIL_SYSCALL_MONITOREXIT,
    EJIL_SYSCALL_DEBUG_PRINT,
    EJIL_SYSCALL_DEBUG_DUMPREG,
    EJIL_SYSCALL_DEBUG_HALT,
};

typedef enum{
    EJIL_OPCODE = 0, //JILOpcode_t
    EJIL_REGPAIR_A = 1, //JILRegpair_t
    EJIL_REGPAIR_B = 2, //JILRegpair_t
    EJIL_REGPAIR_C = 3, //JILRegpair_t
    EJIL_IMMEDIATE_P1 = 1, //uint8_t
    EJIL_IMMEDIATE_P2 = 2, //uint8_t
    EJIL_IMMEDIATE_P3 = 3, //uint8_t
}JILInstructionElement_t;

/*IL assembly language guide:
    labels are presented like that: "label_name_abcdefg: "

    opcode mnemonics are small letter JILOpcode_t enum entrys without EJIL_OPCODE_* 
    
    registers are addressed from r0 - r15

    Immediate might take either value that is passed like that: "$0xFFFF" or "$12345678"
    Or take label address like that "&label_name_abcdefg"

    Assembler program must produce raw binary with IL bytecodea

    If in a regpair register is written as (none) this means NO register must be encoded in this regpair slot
    Yet assembler must handle this properly, for example EJIL_OPCODE_CALL use this techinique, in assembly this must be:
        
        call r5,r6,r6 ; Call method from reg a5

                                            opcode        a  (none)   b    c
        In binary this something like this |1111 0000| |0101 0000| |0110 0110|
*/
