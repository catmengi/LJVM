#pragma once


//Instruction memory is counted in WORDS, any jmp/branch accepts offset in WORDS. call address must be in WORDS
//Offsets for LDR/STR/ALDR/ASTR are in BYTES 
//Each regpair have 2 registers, eg REGPAIR_A have registers A, B
//REGPAIR_B have register C, D, REGPAIR_C have register E,F. Each register represented in regpair as 4bit index into int32_t reg[16]
//sIMM16/sIMM24 - means signed immediate value
//IMM16/IMM24 - means unsigned immediate value

//Память инструкций адресуется в СЛОВАХ, любая инструкция jmp/branch принимает смещение в СЛОВАХ, call инструкции требуют адреса в СЛОВАХ
//Смещения для LDR/STR/ALDR/ASTR указываются в БАЙТАХ
//Каждая регистровая пара адресует 2 регистра, например: REGPAIR_A: регистры A, B. REGPAIR_B: регистры C, D. REGPAIR_C: регистры E,F
//sIMM16/sIMM24 - значит signed immediate число(+-)
//sIMM16/sIMM24 - значит unsigned immediate число(+)
typedef enum{
    EJIL_OPCODE_NOP = 0,
    EJIL_OPCODE_HALT = 10,
    
    EJIL_OPCODE_LDR32,   //REGPAIR_A + sIMM16 :: A = MEM[B + sIMM16]
    EJIL_OPCODE_LDR16,   //REGPAIR_A + sIMM16 :: A = MEM[B + sIMM16]
    EJIL_OPCODE_LDR8,    //REGPAIR_A + sIMM16 :: A = MEM[B + sIMM16]

    EJIL_OPCODE_STR32,   //REGPAIR_A + sIMM16 :: MEM[B + sIMM16] = A
    EJIL_OPCODE_STR16,   //REGPAIR_A + sIMM16 :: MEM[B + sIMM16] = A
    EJIL_OPCODE_STR8,    //REGPAIR_A + sIMM16 :: MEM[B + sIMM16] = A,

    EJIL_OPCODE_ALDR32,   //REGPAIR_A + REGPAIR_B :: A = MEM[B + C]
    EJIL_OPCODE_ALDR16,   //REGPAIR_A + REGPAIR_B :: A = MEM[B + C]
    EJIL_OPCODE_ALDR8,    //REGPAIR_A + REGPAIR_B :: A = MEM[B + C]

    EJIL_OPCODE_ASTR32,   //REGPAIR_A + REGPAIR_B :: MEM[B + C] = A
    EJIL_OPCODE_ASTR16,   //REGPAIR_A + REGPAIR_B :: MEM[B + C] = A
    EJIL_OPCODE_ASTR8,    //REGPAIR_A + REGPAIR_B :: MEM[B + C] = A,

    EJIL_OPCODE_MOV,     //REGPAIR_A :: A = B
    EJIL_OPCODE_MOVI,    //REGPAIR_A + IMM16 :: A = IMM16
    EJIL_OPCODE_MOVZ,    //REGPAIR_A :: A = 0

    EJIL_OPCODE_JMP,     //IMM24 :: PC += IMM24
    EJIL_OPCODE_BRZ,     //REGPAIR_A, sIMM16 :: PC = A == 0 ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRNZ,    //REGPAIR_A, sIMM16 :: PC = A != 0 ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BREQ,    //REGPAIR_A, sIMM16 :: PC = A == B ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRNEQ,   //REGPAIR_A, sIMM16 :: PC = A != B ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRGT,    //REGPAIR_A, sIMM16 :: PC = A > B ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRLT,    //REGPAIR_A, sIMM16 :: PC = A < B ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRGTE,   //REGPAIR_A, sIMM16 :: PC = A >= B ? PC + sIMM16 : PC + 1
    EJIL_OPCODE_BRLTE,   //REGPAIR_A, sIMM16 :: PC = A <= B ? PC + sIMM16 : PC + 1

    EJIL_OPCODE_CMPZ,    //REGPAIR_A :: A = B == 0
    EJIL_OPCODE_CMPNZ,   //REGPAIR_A :: A = B != 0
    EJIL_OPCODE_CMPEQ,   //REGPAIR_A, REGPAIR_B :: A = B == C
    EJIL_OPCODE_CMPNEQ,  //REGPAIR_A, REGPAIR_B :: A = B != C
    EJIL_OPCODE_CMPGT,   //REGPAIR_A, REGPAIR_B :: A = B > C
    EJIL_OPCODE_CMPLT,   //REGPAIR_A, REGPAIR_B :: A = B < C
    EJIL_OPCODE_CMPGTE,  //REGPAIR_A, REGPAIR_B :: A = B >= C
    EJIL_OPCODE_CMPLTE,  //REGPAIR_A, REGPAIR_B :: A = B <= C

    EJIL_OPCODE_SHR,     //REGPAIR_A, REGPAIR_B :: A = B >> C
    EJIL_OPCODE_SHL,     //REGPAIR_A, REGPAIR_B :: A = B << C
    EJIL_OPCODE_AND,     //REGPAIR_A, REGPAIR_B :: A = B & C
    EJIL_OPCODE_OR,      //REGPAIR_A, REGPAIR_B :: A = B | C
    EJIL_OPCODE_XOR,     //REGPAIR_A, REGPAIR_B :: A = B ^ C

    /*
    EJIL_OPCODE_CMPZ64,  //REGPAIR_A :: A = B == 0
    EJIL_OPCODE_CMPNZ64, //REGPAIR_A :: A = B != 0
    EJIL_OPCODE_CMPEQ64, //REGPAIR_A, REGPAIR_B :: A = B == C
    EJIL_OPCODE_CMPNEQ64,//REGPAIR_A, REGPAIR_B :: A = B != C
    EJIL_OPCODE_CMPGT64, //REGPAIR_A, REGPAIR_B :: A = B > C
    EJIL_OPCODE_CMPLT64, //REGPAIR_A, REGPAIR_B :: A = B < C
    EJIL_OPCODE_CMPGTE64,//REGPAIR_A, REGPAIR_B :: A = B >= C
    EJIL_OPCODE_CMPLTE64,//REGPAIR_A, REGPAIR_B :: A = B <= C

    EJIL_OPCODE_SHR64,   //REGPAIR_A, REGPAIR_B :: A = B >> C
    EJIL_OPCODE_SHL64,   //REGPAIR_A, REGPAIR_B :: A = B << C
    EJIL_OPCODE_AND64,   //REGPAIR_A, REGPAIR_B :: A = B & C
    EJIL_OPCODE_OR64,    //REGPAIR_A, REGPAIR_B :: A = B | C
    EJIL_OPCODE_XOR64,   //REGPAIR_A, REGPAIR_B :: A = B ^ C
    */

    EJIL_OPCODE_ADD,     //REGPAIR_A, REGPAIR_B :: A = B + C,
    EJIL_OPCODE_SUB,     //REGPAIR_A, REGPAIR_B :: A = B - C,
    EJIL_OPCODE_MUL,     //REGPAIR_A, REGPAIR_B :: A = B * C,
    EJIL_OPCODE_DIV,     //REGPAIR_A, REGPAIR_B :: A = B / C,
    EJIL_OPCODE_REM,     //REGPAIR_A, REGPAIR_B :: A = B % C,

    EJIL_OPCODE_FADD,    //REGPAIR_A, REGPAIR_B :: A = B + C,
    EJIL_OPCODE_FSUB,    //REGPAIR_A, REGPAIR_B :: A = B - C,
    EJIL_OPCODE_FMUL,    //REGPAIR_A, REGPAIR_B :: A = B * C,
    EJIL_OPCODE_FDIV,    //REGPAIR_A, REGPAIR_B :: A = B / C,
    EJIL_OPCODE_FREM,    //REGPAIR_A, REGPAIR_B :: A = B % C,

    /*
    EJIL_OPCODE_ADD64,   //REGPAIR_A, REGPAIR_B :: A = B + C,
    EJIL_OPCODE_SUB64,   //REGPAIR_A, REGPAIR_B :: A = B - C,
    EJIL_OPCODE_MUL64,   //REGPAIR_A, REGPAIR_B :: A = B * C,
    EJIL_OPCODE_DIV64,   //REGPAIR_A, REGPAIR_B :: A = B / C,
    EJIL_OPCODE_REM64,   //REGPAIR_A, REGPAIR_B :: A = B % C,

    EJIL_OPCODE_FADD64,  //REGPAIR_A, REGPAIR_B :: A = B + C,
    EJIL_OPCODE_FSUB64,  //REGPAIR_A, REGPAIR_B :: A = B - C,
    EJIL_OPCODE_FMUL64,  //REGPAIR_A, REGPAIR_B :: A = B * C,
    EJIL_OPCODE_FDIV64,  //REGPAIR_A, REGPAIR_B :: A = B / C,
    EJIL_OPCODE_FREM64,  //REGPAIR_A, REGPAIR_B :: A = B % C,
    */

    EJIL_OPCODE_SYSCALL, //REGPAIR_A, IMM16:: A = syscalls[IMM16](B)

    EJIL_OPCODE_CALLVOID,//REGPAIR_A :: Call method on address on reg A
    EJIL_OPCODE_RETVOID, // :: Return from method without retval
    EJIL_OPCODE_CALL,    //REGPAIR_A, REGPAIR_B :: Call method on address from reg A, passing argument from reg B(current frame) to reg C(invoked method frame) 
    EJIL_OPCODE_RET,     //REGPAIR_A :: register A - register where to take return value(in current frame), register B - register to write retval in caller frame

    //EJIL_OPCODE_CALLVOID Вызывает метод по адресу из регистра A без передачи аргумента
    //EJIL_OPCODE_RETVOID Возврата из метода БЕЗ возврата значения
    //EJIL_OPCODE_CALL Вызывает метод адресу из регистра A, берет аргумент из регистра B и ложит его в регистр C вызваного метода
    //EJIL_OPCODE_RET Возврата из метода со значением, возвращаемое число берется из регистра A и ложится в регистр B предыдущего(вызывающего) метода

}JILOpcodes_t;