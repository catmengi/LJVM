/*
JEspressoVM - project to bring java bytecode execution to esp32 (and others)

Copyright (C) 2026  Vladislav Potrashkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <stdint.h>

/**
 * Java bytecode opcodes as a C enum.
 * Each constant is named EJOPCODE_<uppercase mnemonic> and has the opcode value.
 * The enum covers all standard Java opcodes from 0x00 to 0xff.
 */
typedef enum {
    // Constants
    EJOPCODE_NOP            = 0x00,
    EJOPCODE_ACONST_NULL    = 0x01,
    EJOPCODE_ICONST_M1      = 0x02,
    EJOPCODE_ICONST_0       = 0x03,
    EJOPCODE_ICONST_1       = 0x04,
    EJOPCODE_ICONST_2       = 0x05,
    EJOPCODE_ICONST_3       = 0x06,
    EJOPCODE_ICONST_4       = 0x07,
    EJOPCODE_ICONST_5       = 0x08,
    EJOPCODE_LCONST_0       = 0x09,
    EJOPCODE_LCONST_1       = 0x0a,
    EJOPCODE_FCONST_0       = 0x0b,
    EJOPCODE_FCONST_1       = 0x0c,
    EJOPCODE_FCONST_2       = 0x0d,
    EJOPCODE_DCONST_0       = 0x0e,
    EJOPCODE_DCONST_1       = 0x0f,

    // Pushes
    EJOPCODE_BIPUSH         = 0x10,
    EJOPCODE_SIPUSH         = 0x11,
    EJOPCODE_LDC            = 0x12,
    EJOPCODE_LDC_W          = 0x13,
    EJOPCODE_LDC2_W         = 0x14,

    // Loads
    EJOPCODE_ILOAD          = 0x15,
    EJOPCODE_LLOAD          = 0x16,
    EJOPCODE_FLOAD          = 0x17,
    EJOPCODE_DLOAD          = 0x18,
    EJOPCODE_ALOAD          = 0x19,
    EJOPCODE_ILOAD_0        = 0x1a,
    EJOPCODE_ILOAD_1        = 0x1b,
    EJOPCODE_ILOAD_2        = 0x1c,
    EJOPCODE_ILOAD_3        = 0x1d,
    EJOPCODE_LLOAD_0        = 0x1e,
    EJOPCODE_LLOAD_1        = 0x1f,
    EJOPCODE_LLOAD_2        = 0x20,
    EJOPCODE_LLOAD_3        = 0x21,
    EJOPCODE_FLOAD_0        = 0x22,
    EJOPCODE_FLOAD_1        = 0x23,
    EJOPCODE_FLOAD_2        = 0x24,
    EJOPCODE_FLOAD_3        = 0x25,
    EJOPCODE_DLOAD_0        = 0x26,
    EJOPCODE_DLOAD_1        = 0x27,
    EJOPCODE_DLOAD_2        = 0x28,
    EJOPCODE_DLOAD_3        = 0x29,
    EJOPCODE_ALOAD_0        = 0x2a,
    EJOPCODE_ALOAD_1        = 0x2b,
    EJOPCODE_ALOAD_2        = 0x2c,
    EJOPCODE_ALOAD_3        = 0x2d,

    // Array loads
    EJOPCODE_IALOAD         = 0x2e,
    EJOPCODE_LALOAD         = 0x2f,
    EJOPCODE_FALOAD         = 0x30,
    EJOPCODE_DALOAD         = 0x31,
    EJOPCODE_AALOAD         = 0x32,
    EJOPCODE_BALOAD         = 0x33,
    EJOPCODE_CALOAD         = 0x34,
    EJOPCODE_SALOAD         = 0x35,

    // Stores
    EJOPCODE_ISTORE         = 0x36,
    EJOPCODE_LSTORE         = 0x37,
    EJOPCODE_FSTORE         = 0x38,
    EJOPCODE_DSTORE         = 0x39,
    EJOPCODE_ASTORE         = 0x3a,
    EJOPCODE_ISTORE_0       = 0x3b,
    EJOPCODE_ISTORE_1       = 0x3c,
    EJOPCODE_ISTORE_2       = 0x3d,
    EJOPCODE_ISTORE_3       = 0x3e,
    EJOPCODE_LSTORE_0       = 0x3f,
    EJOPCODE_LSTORE_1       = 0x40,
    EJOPCODE_LSTORE_2       = 0x41,
    EJOPCODE_LSTORE_3       = 0x42,
    EJOPCODE_FSTORE_0       = 0x43,
    EJOPCODE_FSTORE_1       = 0x44,
    EJOPCODE_FSTORE_2       = 0x45,
    EJOPCODE_FSTORE_3       = 0x46,
    EJOPCODE_DSTORE_0       = 0x47,
    EJOPCODE_DSTORE_1       = 0x48,
    EJOPCODE_DSTORE_2       = 0x49,
    EJOPCODE_DSTORE_3       = 0x4a,
    EJOPCODE_ASTORE_0       = 0x4b,
    EJOPCODE_ASTORE_1       = 0x4c,
    EJOPCODE_ASTORE_2       = 0x4d,
    EJOPCODE_ASTORE_3       = 0x4e,

    // Array stores
    EJOPCODE_IASTORE        = 0x4f,
    EJOPCODE_LASTORE        = 0x50,
    EJOPCODE_FASTORE        = 0x51,
    EJOPCODE_DASTORE        = 0x52,
    EJOPCODE_AASTORE        = 0x53,
    EJOPCODE_BASTORE        = 0x54,
    EJOPCODE_CASTORE        = 0x55,
    EJOPCODE_SASTORE        = 0x56,

    // Stack operations
    EJOPCODE_POP            = 0x57,
    EJOPCODE_POP2           = 0x58,
    EJOPCODE_DUP            = 0x59,
    EJOPCODE_DUP_X1         = 0x5a,
    EJOPCODE_DUP_X2         = 0x5b,
    EJOPCODE_DUP2           = 0x5c,
    EJOPCODE_DUP2_X1        = 0x5d,
    EJOPCODE_DUP2_X2        = 0x5e,
    EJOPCODE_SWAP           = 0x5f,

    // Arithmetic
    EJOPCODE_IADD           = 0x60,
    EJOPCODE_LADD           = 0x61,
    EJOPCODE_FADD           = 0x62,
    EJOPCODE_DADD           = 0x63,
    EJOPCODE_ISUB           = 0x64,
    EJOPCODE_LSUB           = 0x65,
    EJOPCODE_FSUB           = 0x66,
    EJOPCODE_DSUB           = 0x67,
    EJOPCODE_IMUL           = 0x68,
    EJOPCODE_LMUL           = 0x69,
    EJOPCODE_FMUL           = 0x6a,
    EJOPCODE_DMUL           = 0x6b,
    EJOPCODE_IDIV           = 0x6c,
    EJOPCODE_LDIV           = 0x6d,
    EJOPCODE_FDIV           = 0x6e,
    EJOPCODE_DDIV           = 0x6f,
    EJOPCODE_IREM           = 0x70,
    EJOPCODE_LREM           = 0x71,
    EJOPCODE_FREM           = 0x72,
    EJOPCODE_DREM           = 0x73,
    EJOPCODE_INEG           = 0x74,
    EJOPCODE_LNEG           = 0x75,
    EJOPCODE_FNEG           = 0x76,
    EJOPCODE_DNEG           = 0x77,

    // Shift
    EJOPCODE_ISHL           = 0x78,
    EJOPCODE_LSHL           = 0x79,
    EJOPCODE_ISHR           = 0x7a,
    EJOPCODE_LSHR           = 0x7b,
    EJOPCODE_IUSHR          = 0x7c,
    EJOPCODE_LUSHR          = 0x7d,

    // Bitwise
    EJOPCODE_IAND           = 0x7e,
    EJOPCODE_LAND           = 0x7f,
    EJOPCODE_IOR            = 0x80,
    EJOPCODE_LOR            = 0x81,
    EJOPCODE_IXOR           = 0x82,
    EJOPCODE_LXOR           = 0x83,

    // Increment
    EJOPCODE_IINC           = 0x84,

    // Conversions
    EJOPCODE_I2L            = 0x85,
    EJOPCODE_I2F            = 0x86,
    EJOPCODE_I2D            = 0x87,
    EJOPCODE_L2I            = 0x88,
    EJOPCODE_L2F            = 0x89,
    EJOPCODE_L2D            = 0x8a,
    EJOPCODE_F2I            = 0x8b,
    EJOPCODE_F2L            = 0x8c,
    EJOPCODE_F2D            = 0x8d,
    EJOPCODE_D2I            = 0x8e,
    EJOPCODE_D2L            = 0x8f,
    EJOPCODE_D2F            = 0x90,
    EJOPCODE_I2B            = 0x91,
    EJOPCODE_I2C            = 0x92,
    EJOPCODE_I2S            = 0x93,

    // Comparisons
    EJOPCODE_LCMP           = 0x94,
    EJOPCODE_FCMPL          = 0x95,
    EJOPCODE_FCMPG          = 0x96,
    EJOPCODE_DCMPL          = 0x97,
    EJOPCODE_DCMPG          = 0x98,

    // Conditional branches
    EJOPCODE_IFEQ           = 0x99,
    EJOPCODE_IFNE           = 0x9a,
    EJOPCODE_IFLT           = 0x9b,
    EJOPCODE_IFGE           = 0x9c,
    EJOPCODE_IFGT           = 0x9d,
    EJOPCODE_IFLE           = 0x9e,
    EJOPCODE_IF_ICMPEQ      = 0x9f,
    EJOPCODE_IF_ICMPNE      = 0xa0,
    EJOPCODE_IF_ICMPLT      = 0xa1,
    EJOPCODE_IF_ICMPGE      = 0xa2,
    EJOPCODE_IF_ICMPGT      = 0xa3,
    EJOPCODE_IF_ICMPLE      = 0xa4,
    EJOPCODE_IF_ACMPEQ      = 0xa5,
    EJOPCODE_IF_ACMPNE      = 0xa6,

    // Unconditional branches
    EJOPCODE_GOTO           = 0xa7,
    EJOPCODE_JSR            = 0xa8,
    EJOPCODE_RET            = 0xa9,

    // Table switches
    EJOPCODE_TABLESWITCH    = 0xaa,
    EJOPCODE_LOOKUPSWITCH   = 0xab,

    // Returns
    EJOPCODE_IRETURN        = 0xac,
    EJOPCODE_LRETURN        = 0xad,
    EJOPCODE_FRETURN        = 0xae,
    EJOPCODE_DRETURN        = 0xaf,
    EJOPCODE_ARETURN        = 0xb0,
    EJOPCODE_RETURN         = 0xb1,

    // Field access
    EJOPCODE_GETSTATIC      = 0xb2,
    EJOPCODE_PUTSTATIC      = 0xb3,
    EJOPCODE_GETFIELD       = 0xb4,
    EJOPCODE_PUTFIELD       = 0xb5,

    // Method invocation
    EJOPCODE_INVOKEVIRTUAL  = 0xb6,
    EJOPCODE_INVOKESPECIAL  = 0xb7,
    EJOPCODE_INVOKESTATIC   = 0xb8,
    EJOPCODE_INVOKEINTERFACE = 0xb9,
    EJOPCODE_INVOKEDYNAMIC  = 0xba,

    // Object allocation
    EJOPCODE_NEW            = 0xbb,
    EJOPCODE_NEWARRAY       = 0xbc,
    EJOPCODE_ANEWARRAY      = 0xbd,
    EJOPCODE_ARRAYLENGTH    = 0xbe,

    // Exceptions
    EJOPCODE_ATHROW         = 0xbf,

    // Object operations
    EJOPCODE_CHECKCAST      = 0xc0,
    EJOPCODE_INSTANCEOF     = 0xc1,

    // Monitors
    EJOPCODE_MONITORENTER   = 0xc2,
    EJOPCODE_MONITOREXIT    = 0xc3,

    // Wide
    EJOPCODE_WIDE           = 0xc4,

    // Multidimensional array
    EJOPCODE_MULTIANEWARRAY = 0xc5,

    // Conditional branches (null)
    EJOPCODE_IFNULL         = 0xc6,
    EJOPCODE_IFNONNULL      = 0xc7,

    // Wide branches
    EJOPCODE_GOTO_W         = 0xc8,
    EJOPCODE_JSR_W          = 0xc9,

    // Reserved
    EJOPCODE_BREAKPOINT     = 0xca,

    // 0xcb-0xfd are unused
    EJOPCODE_IMPDEP1        = 0xfe,
    EJOPCODE_IMPDEP2        = 0xff
}JOpcode_t;

static const uint8_t JOpcode_args_sizes[256] = {
    [EJOPCODE_BIPUSH]         = 1,
    [EJOPCODE_SIPUSH]         = 2,
    [EJOPCODE_LDC]            = 1,
    [EJOPCODE_LDC_W]          = 2,
    [EJOPCODE_LDC2_W]         = 2,

    [EJOPCODE_ILOAD]          = 1,
    [EJOPCODE_LLOAD]          = 1,
    [EJOPCODE_FLOAD]          = 1,
    [EJOPCODE_DLOAD]          = 1,
    [EJOPCODE_ALOAD]          = 1,

    [EJOPCODE_ISTORE]         = 1,
    [EJOPCODE_LSTORE]         = 1,
    [EJOPCODE_FSTORE]         = 1,
    [EJOPCODE_DSTORE]         = 1,
    [EJOPCODE_ASTORE]         = 1,

    [EJOPCODE_IINC]           = 2,

    [EJOPCODE_IFEQ]           = 2,
    [EJOPCODE_IFNE]           = 2,
    [EJOPCODE_IFLT]           = 2,
    [EJOPCODE_IFGE]           = 2,
    [EJOPCODE_IFGT]           = 2,
    [EJOPCODE_IFLE]           = 2,
    [EJOPCODE_IF_ICMPEQ]      = 2,
    [EJOPCODE_IF_ICMPNE]      = 2,
    [EJOPCODE_IF_ICMPLT]      = 2,
    [EJOPCODE_IF_ICMPGE]      = 2,
    [EJOPCODE_IF_ICMPGT]      = 2,
    [EJOPCODE_IF_ICMPLE]      = 2,
    [EJOPCODE_IF_ACMPEQ]      = 2,
    [EJOPCODE_IF_ACMPNE]      = 2,

    [EJOPCODE_GOTO]           = 2,
    [EJOPCODE_JSR]            = 2,
    [EJOPCODE_RET]            = 1,

    [EJOPCODE_GETSTATIC]      = 2,
    [EJOPCODE_PUTSTATIC]      = 2,
    [EJOPCODE_GETFIELD]       = 2,
    [EJOPCODE_PUTFIELD]       = 2,

    [EJOPCODE_INVOKEVIRTUAL]  = 2,
    [EJOPCODE_INVOKESPECIAL]  = 2,
    [EJOPCODE_INVOKESTATIC]   = 2,
    [EJOPCODE_INVOKEINTERFACE]= 4,
    [EJOPCODE_INVOKEDYNAMIC]  = 4,

    [EJOPCODE_NEW]            = 2,
    [EJOPCODE_NEWARRAY]       = 1,
    [EJOPCODE_ANEWARRAY]      = 2,

    [EJOPCODE_CHECKCAST]      = 2,
    [EJOPCODE_INSTANCEOF]     = 2,

    [EJOPCODE_MULTIANEWARRAY] = 3,

    [EJOPCODE_IFNULL]         = 2,
    [EJOPCODE_IFNONNULL]      = 2,

    [EJOPCODE_GOTO_W]         = 4,
    [EJOPCODE_JSR_W]          = 4,

    // All other opcodes (including NOP, constants, loads/stores without index,
    // arithmetic, returns, etc.) have 0 argument bytes.
    // Unused opcodes (0xCB–0xFD) are also 0.
};