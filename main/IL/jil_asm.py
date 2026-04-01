#!/usr/bin/env python3
"""
JIL Assembler and Disassembler
Converts between assembly text and raw binary bytecode.
"""

import argparse
import sys

# Opcode definitions: mnemonic -> (opcode, operand_count)
OPCODES = {
    'nop':      (0x00, 0),
    'ldr32':    (0xA0, 3),
    'str32':    (0xA1, 3),
    'ldr16':    (0xA2, 3),
    'str16':    (0xA3, 3),
    'ldr8':     (0xA4, 3),
    'str8':     (0xA5, 3),
    'mov':      (0xB0, 2),
    'movi':     (0xB1, 2),
    'add':      (0xC0, 3),
    'sub':      (0xC1, 3),
    'mul':      (0xC2, 3),
    'div':      (0xC3, 3),
    'rem':      (0xC4, 3),
    'and':      (0xC5, 3),
    'xor':      (0xC6, 3),
    'or':       (0xC7, 3),
    'shr':      (0xC8, 3),
    'shl':      (0xC9, 3),
    'neg':      (0xCA, 2),
    'fadd':     (0xCB, 3),
    'fsub':     (0xCC, 3),
    'fmul':     (0xCD, 3),
    'fdiv':     (0xCE, 3),
    'frem':     (0xCF, 3),
    'cmpeq':    (0xD0, 3),
    'cmpneq':   (0xD1, 3),
    'cmpgr':    (0xD2, 3),
    'cmpgreq':  (0xD3, 3),
    'cmpls':    (0xD4, 3),
    'cmplseq':  (0xD5, 3),
    'dadd':     (0xDB, 6),
    'dsub':     (0xDC, 6),
    'dmul':     (0xDD, 6),
    'ddiv':     (0xDE, 6),
    'drem':     (0xDF, 6),
    'jmp':      (0xE0, 1),
    'brz':      (0xE1, 2),
    'brnz':     (0xE2, 2),
    'jmpreg':   (0xE3, 1),
    'call':     (0xF0, 3),
    'callv':     (0xF4, 1),
    'ret':      (0xF1, 2),
    'retv':      (0xF3, 0),
    'syscall':  (0xF2, 3),
    'ladd':     (0xFB, 6),
    'lsub':     (0xFC, 6),
    'lmul':     (0xFD, 6),
    'ldiv':     (0xFE, 6),
    'lrem':     (0xFF, 6),
}

# Reverse mapping: opcode -> mnemonic
OPCODE_TO_MNEMONIC = {v[0]: k for k, v in OPCODES.items()}

def parse_register(token):
    """Parse a register token like 'r5' -> int 5."""
    token = token.strip()
    if token.startswith('r'):
        try:
            return int(token[1:])
        except ValueError:
            pass
    raise ValueError(f"Invalid register: {token}")

def parse_immediate(token, labels, current_offset):
    """Parse immediate value: $123, $0xABC, &label."""
    token = token.strip()
    if token.startswith('$'):
        num_str = token[1:]
        if num_str.startswith('0x'):
            return int(num_str[2:], 16)
        else:
            return int(num_str, 10)
    elif token.startswith('&'):
        label = token[1:]
        if label not in labels:
            raise ValueError(f"Undefined label: {label}")
        return labels[label]
    else:
        raise ValueError(f"Invalid immediate: {token}")

def encode_instruction(opcode, operands, labels, current_offset):
    """Encode a single instruction into 4 bytes."""
    if opcode == 0x00:  # nop
        return bytes([0, 0, 0, 0])

    # LDR/STR family (3 operands: dest, base, imm16)
    if 0xA0 <= opcode <= 0xA5:
        dest, base, imm = operands
        imm_val = parse_immediate(imm, labels, current_offset)
        if not (0 <= imm_val <= 0xFFFF):
            raise ValueError(f"Immediate out of range (16-bit): {imm_val}")
        regpair_a = (dest << 4) | base
        imm_high = (imm_val >> 8) & 0xFF
        imm_low = imm_val & 0xFF
        return bytes([opcode, regpair_a, imm_high, imm_low])

    # MOV (2 operands: dest, src)
    if opcode == 0xB0:
        dest, src = operands
        regpair = (dest << 4) | src
        return bytes([opcode, regpair, 0, 0])

    # MOVI (2 operands: dest, imm16)
    if opcode == 0xB1:
        dest, imm = operands
        imm_val = parse_immediate(imm, labels, current_offset)
        if not (0 <= imm_val <= 0xFFFF):
            raise ValueError(f"Immediate out of range (16-bit): {imm_val}")
        regpair = (dest << 4) | 0  # second register is none
        imm_high = (imm_val >> 8) & 0xFF
        imm_low = imm_val & 0xFF
        return bytes([opcode, regpair, imm_high, imm_low])

    # ALU ops (3 operands: dest, src1, src2)
    if (0xC0 <= opcode <= 0xC9) or (0xCB <= opcode <= 0xCF) or (0xD0 <= opcode <= 0xD5):
        dest, src1, src2 = operands
        regpair_a = (dest << 4) | 0
        regpair_b = (src1 << 4) | src2
        return bytes([opcode, regpair_a, regpair_b, 0])

    # NEG (2 operands: dest, src)
    if opcode == 0xCA:
        dest, src = operands
        regpair = (dest << 4) | src
        return bytes([opcode, regpair, 0, 0])

    # Double/Long ops (6 operands: dest_hi, dest_lo, src1_hi, src1_lo, src2_hi, src2_lo)
    if opcode in [0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF]:
        dest_hi, dest_lo, src1_hi, src1_lo, src2_hi, src2_lo = operands
        regpair_a = (dest_hi << 4) | dest_lo
        regpair_b = (src1_hi << 4) | src1_lo
        regpair_c = (src2_hi << 4) | src2_lo
        return bytes([opcode, regpair_a, regpair_b, regpair_c])

    # JMP (1 operand: imm24)
    if opcode == 0xE0:
        imm = operands[0]
        imm_val = parse_immediate(imm, labels, current_offset)
        if not (0 <= imm_val <= 0xFFFFFF):
            raise ValueError(f"Immediate out of range (24-bit): {imm_val}")
        imm_p1 = (imm_val >> 16) & 0xFF
        imm_p2 = (imm_val >> 8) & 0xFF
        imm_p3 = imm_val & 0xFF
        return bytes([opcode, imm_p1, imm_p2, imm_p3])

    # BRZ/BRNZ (2 operands: reg, imm16)
    if opcode in [0xE1, 0xE2]:
        reg, imm = operands
        imm_val = parse_immediate(imm, labels, current_offset)
        if not (0 <= imm_val <= 0xFFFF):
            raise ValueError(f"Immediate out of range (16-bit): {imm_val}")
        regpair = (reg << 4) | 0
        imm_high = (imm_val >> 8) & 0xFF
        imm_low = imm_val & 0xFF
        return bytes([opcode, regpair, imm_high, imm_low])

    # JMPREG (1 operand: reg)
    if opcode == 0xE3:
        reg = operands[0]
        regpair = (reg << 4) | 0
        return bytes([opcode, regpair, 0, 0])

    # CALL (3 operands: func, in, out)
    if opcode == 0xF0:
        func, in_reg, out_reg = operands
        regpair_a = (func << 4) | 0
        regpair_b = (in_reg << 4) | out_reg
        return bytes([opcode, regpair_a, regpair_b, 0])

    # CALLV
    if opcode == 0xF4:
        func = operands
        regpair_a = (func << 4) | 0
        return bytes([opcode, regpair_a, 0, 0])

    # RET (2 operands: ret_reg, store_reg)
    if opcode == 0xF1:
        ret, store = operands
        regpair = (ret << 4) | store
        return bytes([opcode, regpair, 0, 0])

    # RETV
    if opcode == 0xF3:
        return bytes([opcode, 0, 0, 0])

    # SYSCALL (3 operands: a, b, imm16)
    if opcode == 0xF2:
        a, b, imm = operands
        imm_val = parse_immediate(imm, labels, current_offset)
        if not (0 <= imm_val <= 0xFFFF):
            raise ValueError(f"Immediate out of range (16-bit): {imm_val}")
        regpair = (a << 4) | b
        imm_high = (imm_val >> 8) & 0xFF
        imm_low = imm_val & 0xFF
        return bytes([opcode, regpair, imm_high, imm_low])

    raise ValueError(f"Unsupported opcode: {opcode:#04x}")

def assemble(input_file, output_file):
    """Assemble JIL assembly to binary."""
    with open(input_file, 'r') as f:
        lines = f.readlines()

    labels = {}
    offset = 0
    instructions = []  # list of (line_num, raw_line, mnemonic, opcode, operands)

    for line_num, raw_line in enumerate(lines, 1):
        # Strip comments
        if ';' in raw_line:
            raw_line = raw_line.split(';')[0]
        line = raw_line.strip()
        if not line:
            continue

        # Handle label on same line
        if ':' in line:
            parts = line.split(':', 1)
            label = parts[0].strip()
            if label:
                if label in labels:
                    raise ValueError(f"Duplicate label {label} at line {line_num}")
                labels[label] = offset
            line = parts[1].strip() if len(parts) > 1 else ''
            if not line:
                continue

        # Replace commas with spaces for consistent tokenization
        line = line.replace(',', ' ')
        tokens = line.split()
        if not tokens:
            continue

        mnemonic = tokens[0].lower()
        if mnemonic not in OPCODES:
            raise ValueError(f"Unknown mnemonic {mnemonic} at line {line_num}")
        opcode, operand_count = OPCODES[mnemonic]
        operands = tokens[1:]
        if len(operands) != operand_count:
            raise ValueError(f"Wrong number of operands for {mnemonic} at line {line_num}")
        instructions.append((line_num, raw_line, mnemonic, opcode, operands))
        offset += 4

    with open(output_file, 'wb') as out:
        for line_num, raw_line, mnemonic, opcode, operands in instructions:
            try:
                parsed_operands = []
                for op in operands:
                    if op.startswith('r'):
                        parsed_operands.append(parse_register(op))
                    else:
                        # immediate (starts with $ or &)
                        parsed_operands.append(op)
                encoded = encode_instruction(opcode, parsed_operands, labels, offset)
                out.write(encoded)
            except Exception as e:
                raise ValueError(f"Error at line {line_num}: {raw_line}\n{e}")

    print(f"Assembled {len(instructions)} instructions to {output_file}")

def disassemble(input_file, output_file):
    """Disassemble binary to JIL assembly."""
    with open(input_file, 'rb') as f:
        data = f.read()

    if len(data) % 4 != 0:
        raise ValueError("Binary size not multiple of 4")

    with open(output_file, 'w') as out:
        for i in range(0, len(data), 4):
            instr = data[i:i+4]
            opcode = instr[0]
            if opcode not in OPCODE_TO_MNEMONIC:
                out.write(f"; Unknown opcode {opcode:#04x} at offset {i:#04x}\n")
                continue
            mnemonic = OPCODE_TO_MNEMONIC[opcode]

            # Decode based on opcode
            if opcode == 0x00:  # nop
                out.write("nop\n")
            elif 0xA0 <= opcode <= 0xA5:  # LDR/STR
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                b = regpair & 0xF
                imm = (instr[2] << 8) | instr[3]
                out.write(f"{mnemonic} r{a}, r{b}, ${imm}\n")
            elif opcode == 0xB0:  # mov
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                b = regpair & 0xF
                out.write(f"mov r{a}, r{b}\n")
            elif opcode == 0xB1:  # movi
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                imm = (instr[2] << 8) | instr[3]
                out.write(f"movi r{a}, ${imm}\n")
            elif (0xC0 <= opcode <= 0xC9) or (0xCB <= opcode <= 0xCF) or (0xD0 <= opcode <= 0xD5):
                regpair_a = instr[1]
                a = (regpair_a >> 4) & 0xF
                regpair_b = instr[2]
                b = (regpair_b >> 4) & 0xF
                c = regpair_b & 0xF
                out.write(f"{mnemonic} r{a}, r{b}, r{c}\n")
            elif opcode == 0xCA:  # neg
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                b = regpair & 0xF
                out.write(f"neg r{a}, r{b}\n")
            elif opcode in [0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF]:
                regpair_a = instr[1]
                a = (regpair_a >> 4) & 0xF
                b = regpair_a & 0xF
                regpair_b = instr[2]
                c = (regpair_b >> 4) & 0xF
                d = regpair_b & 0xF
                regpair_c = instr[3]
                e = (regpair_c >> 4) & 0xF
                f = regpair_c & 0xF
                out.write(f"{mnemonic} r{a}, r{b}, r{c}, r{d}, r{e}, r{f}\n")
            elif opcode == 0xE0:  # jmp
                imm = (instr[1] << 16) | (instr[2] << 8) | instr[3]
                out.write(f"jmp ${imm}\n")
            elif opcode in [0xE1, 0xE2]:  # brz, brnz
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                imm = (instr[2] << 8) | instr[3]
                out.write(f"{mnemonic} r{a}, ${imm}\n")
            elif opcode == 0xE3:  # jmpreg
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                out.write(f"jmpreg r{a}\n")
            elif opcode == 0xF0:  # call
                regpair_a = instr[1]
                a = (regpair_a >> 4) & 0xF
                regpair_b = instr[2]
                b = (regpair_b >> 4) & 0xF
                c = regpair_b & 0xF
                out.write(f"call r{a}, r{b}, r{c}\n")
            elif opcode == 0xF4:  # callv
                regpair_a = instr[1]
                a = (regpair_a >> 4) & 0xF
                out.write(f"callv r{a}\n")
            elif opcode == 0xF1:  # ret
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                b = regpair & 0xF
                out.write(f"ret r{a}, r{b}\n")
            elif opcode == 0xF3:  # retv
                out.write(f"retv\n")
            elif opcode == 0xF2:  # syscall
                regpair = instr[1]
                a = (regpair >> 4) & 0xF
                b = regpair & 0xF
                imm = (instr[2] << 8) | instr[3]
                out.write(f"syscall r{a}, r{b}, ${imm}\n")
            else:
                out.write(f"; Unknown opcode {opcode:#04x} at offset {i:#04x}\n")

    print(f"Disassembled {len(data)//4} instructions to {output_file}")

def main():
    parser = argparse.ArgumentParser(description="JIL Assembler/Disassembler")
    subparsers = parser.add_subparsers(dest='command', required=True)

    asm_parser = subparsers.add_parser('asm', help='Assemble')
    asm_parser.add_argument('input', help='Input assembly file')
    asm_parser.add_argument('output', help='Output binary file')

    dis_parser = subparsers.add_parser('disasm', help='Disassemble')
    dis_parser.add_argument('input', help='Input binary file')
    dis_parser.add_argument('output', help='Output assembly file')

    args = parser.parse_args()

    if args.command == 'asm':
        assemble(args.input, args.output)
    elif args.command == 'disasm':
        disassemble(args.input, args.output)

if __name__ == "__main__":
    main()