#include "bumper.h"
#include "class.h"
#include "compiler.h"
#include "jerror.h"
#include "loader.h"
#include "lb_endian.h"

typedef enum {
    NOP = 0,
    ACONST_NULL,
    ICONST_M1,
    ICONST_0,
    ICONST_1,
    ICONST_2,
    ICONST_3,
    ICONST_4,
    ICONST_5,
    LCONST_0,
    LCONST_1,
    FCONST_0,
    FCONST_1,
    FCONST_2,
    DCONST_0,
    DCONST_1,
    BIPUSH,
    SIPUSH,
    LDC,
    LDC_W,
    LDC2_W,
    ILOAD,
    LLOAD,
    FLOAD,
    DLOAD,
    ALOAD,
    ILOAD_0,
    ILOAD_1,
    ILOAD_2,
    ILOAD_3,
    LLOAD_0,
    LLOAD_1,
    LLOAD_2,
    LLOAD_3,
    FLOAD_0,
    FLOAD_1,
    FLOAD_2,
    FLOAD_3,
    DLOAD_0,
    DLOAD_1,
    DLOAD_2,
    DLOAD_3,
    ALOAD_0,
    ALOAD_1,
    ALOAD_2,
    ALOAD_3,
    IALOAD,
    LALOAD,
    FALOAD,
    DALOAD,
    AALOAD,
    BALOAD,
    CALOAD,
    SALOAD,
    ISTORE,
    LSTORE,
    FSTORE,
    DSTORE,
    ASTORE,
    ISTORE_0,
    ISTORE_1,
    ISTORE_2,
    ISTORE_3,
    LSTORE_0,
    LSTORE_1,
    LSTORE_2,
    LSTORE_3,
    FSTORE_0,
    FSTORE_1,
    FSTORE_2,
    FSTORE_3,
    DSTORE_0,
    DSTORE_1,
    DSTORE_2,
    DSTORE_3,
    ASTORE_0,
    ASTORE_1,
    ASTORE_2,
    ASTORE_3,
    IASTORE,
    LASTORE,
    FASTORE,
    DASTORE,
    AASTORE,
    BASTORE,
    CASTORE,
    SASTORE,
    POP,
    POP2,
    DUP,
    DUP_X1,
    DUP_X2,
    DUP2,
    DUP2_X1,
    DUP2_X2,
    SWAP,
    IADD,
    LADD,
    FADD,
    DADD,
    ISUB,
    LSUB,
    FSUB,
    DSUB,
    IMUL,
    LMUL,
    FMUL,
    DMUL,
    IDIV,
    LDIV,
    FDIV,
    DDIV,
    IREM,
    LREM,
    FREM,
    DREM,
    INEG,
    LNEG,
    FNEG,
    DNEG,
    ISHL,
    LSHL,
    ISHR,
    LSHR,
    IUSHR,
    LUSHR,
    IAND,
    LAND,
    IOR,
    LOR,
    IXOR,
    LXOR,
    IINC,
    I2L,
    I2F,
    I2D,
    L2I,
    L2F,
    L2D,
    F2I,
    F2L,
    F2D,
    D2I,
    D2L,
    D2F,
    I2B,
    I2C,
    I2S,
    LCMP,
    FCMPL,
    FCMPG,
    DCMPL,
    DCMPG,
    IFEQ,
    IFNE,
    IFLT,
    IFGE,
    IFGT,
    IFLE,
    IF_ICMPEQ,
    IF_ICMPNE,
    IF_ICMPLT,
    IF_ICMPGE,
    IF_ICMPGT,
    IF_ICMPLE,
    IF_ACMPEQ,
    IF_ACMPNE,
    GOTO,
    JSR,
    RET,
    TABLESWITCH,
    LOOKUPSWITCH,
    IRETURN,
    LRETURN,
    FRETURN,
    DRETURN,
    ARETURN,
    RETURN,
    GETSTATIC,
    PUTSTATIC,
    GETFIELD,
    PUTFIELD,
    INVOKEVIRTUAL,
    INVOKESPECIAL,
    INVOKESTATIC,
    INVOKEINTERFACE,
    INVOKEDYNAMIC,
    NEW,
    NEWARRAY,
    ANEWARRAY,
    ARRAYLENGTH,
    ATHROW,
    CHECKCAST,
    INSTANCEOF,
    MONITORENTER,
    MONITOREXIT,
    WIDE,
    MULTIANEWARRAY,
    IFNULL,
    IFNONNULL,
    GOTO_W,
    JSR_W,
    BREAKPOINT = 0xCA,
    IMPDEP1 = 0xFE,
    IMPDEP2 = 0xFF
} JVMOpcode_t;

//Tableswitch, lookupswitch, wide is 0, TODO: parse them properly
static const unsigned char jvm_insn_size[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x00-0x0F
    2,3,2,3,3,2,2,2,2,2,1,1,1,1,1,1, // 0x10-0x1F
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x20-0x2F
    1,1,1,1,1,1,2,2,2,2,2,1,1,1,1,1, // 0x30-0x3F
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x40-0x4F
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x50-0x5F
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x60-0x6F
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x70-0x7F
    1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1, // 0x80-0x8F
    1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3, // 0x90-0x9F
    3,3,3,3,3,3,3,3,3,2,0,0,1,1,1,1, // 0xA0-0xAF
    1,1,3,3,3,3,3,3,3,5,5,3,2,3,1,1, // 0xB0-0xBF
    3,3,1,1,0,4,3,3,4,4,1,1,1,1,1,1, // 0xC0-0xCF
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xD0-0xDF
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xE0-0xEF
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  // 0xF0-0xFF
};

JCFGBlock_t* JCFG_lookup(JCFGBlock_t* block, unsigned visit_id, unsigned pc){

    if(block->jbpc_start <= pc && pc < block->jbpc_end){
        return block;
    } else {
        if(block->visit_id == visit_id) return NULL;

        block->visit_id = visit_id;
        for(unsigned i = 0; i < block->children_count; i++){
            JCFGBlock_t* found = JCFG_lookup(block->children[i],visit_id,pc);
            if(found)
                return found;
        }
        return NULL;
    }
}
JError_t JCFG_generate(JMethodCompiler_t* compiler, JCFGBlock_t** block_output, unsigned start_pc){
    JCodeAttribute_t* bytecode = compiler->bytecode;
    JError_t err = JERR_OK;

    JCFGBlock_t* new_block = bumper_calloc(compiler->arena,1,sizeof(*new_block));
    FAIL_SET_JUMP(new_block,err,JERR_OOM,exit);
    *block_output = new_block;
    new_block->block_id = compiler->last_block_id++;

    new_block->jbpc_start = start_pc;
    unsigned pc = new_block->jbpc_start;
    for(; pc < bytecode->code_length; pc += jvm_insn_size[bytecode->code[pc]]){
        uint8_t opcode = bytecode->code[pc];
        switch(opcode){
            default: assert(opcode != WIDE && opcode != TABLESWITCH && opcode != LOOKUPSWITCH && "IMPLEMENT THOOSE BASTARDS"); break;

            case GOTO_W:
            case GOTO:{
                int32_t offset = opcode == GOTO ? (int16_t)be16_to_cpu(*(uint16_t*)&bytecode->code[pc + 1]) : (int32_t)be32_to_cpu(*(uint32_t*)&bytecode->code[pc + 1]);                
                int32_t goto_pc = pc + offset;
                FAIL_SET_JUMP(goto_pc >= 0 && goto_pc < bytecode->code_length,err,JERR_BADPARAM,exit);

                new_block->block_type = EJCFGBT_GOTO;
                new_block->jbpc_end = pc + jvm_insn_size[opcode];

                JCFGBlock_t* goto_block = JCFG_lookup(compiler->root,compiler->last_visit_id++,goto_pc);
                if(!goto_block){
                    FAIL_SET_JUMP(JCFG_generate(compiler,&goto_block,goto_pc) == JERR_OK,err,JERR_UNKNOWN,exit);
                }

                new_block->children_count = 1;
                new_block->children = bumper_calloc(compiler->arena,new_block->children_count,sizeof(*new_block->children));
                FAIL_SET_JUMP(new_block->children,err,JERR_OOM,exit);

                new_block->children[0] = goto_block;
                return JERR_OK; //Block end
            }
            break;

            case JSR_W:
            case JSR:{
                printf("JSR!!!!!!!\n");
                int32_t offset = opcode == JSR ? be16_to_cpu(*(uint16_t*)&bytecode->code[pc + 1]) : be32_to_cpu(*(uint32_t*)&bytecode->code[pc + 1]);
                int32_t jump_pc = pc + offset;
                int32_t ret_pc = pc + jvm_insn_size[opcode];

                FAIL_SET_JUMP(jump_pc >= 0 && jump_pc < bytecode->code_length,err,JERR_BADPARAM,exit);
                FAIL_SET_JUMP(ret_pc >= 0 && ret_pc < bytecode->code_length,err,JERR_BADPARAM,exit);

                new_block->block_type = EJCFGBT_JSR;
                new_block->jbpc_end = ret_pc;

                JCFGBlock_t* jump_block = JCFG_lookup(compiler->root,compiler->last_visit_id++,jump_pc);
                JCFGBlock_t* ret_block = JCFG_lookup(compiler->root,compiler->last_visit_id++,ret_pc);

                if(!ret_block){
                    JError_t childerr = JCFG_generate(compiler, &ret_block, ret_pc);
                    FAIL_SET_JUMP(childerr == JERR_OK,err,childerr,exit);
                }
                if(!jump_block){
                    JError_t childerr = JCFG_generate(compiler, &jump_block, jump_pc);
                    FAIL_SET_JUMP(childerr == JERR_OK,err,childerr,exit);
                }

                new_block->children_count = 2;
                new_block->children = bumper_calloc(compiler->arena,new_block->children_count,sizeof(*new_block->children));
                FAIL_SET_JUMP(new_block->children,err,JERR_OOM,exit);

                new_block->children[0] = ret_block;
                new_block->children[1] = jump_block;

                return JERR_OK;
            }
            break;

            case IFEQ:
            case IFNE:
            case IFLT:
            case IFGE:
            case IFGT:
            case IFLE:
            case IF_ICMPEQ:
            case IF_ICMPNE:
            case IF_ICMPLT:
            case IF_ICMPGE:
            case IF_ICMPGT:
            case IF_ICMPLE:
            case IF_ACMPEQ:
            case IF_ACMPNE:
            case IFNULL:
            case IFNONNULL:{
                int16_t offset = be16_to_cpu(*(uint16_t*)&bytecode->code[pc + 1]);
                int32_t jump_pc = pc + offset;
                int32_t fall_pc = pc + jvm_insn_size[opcode]; 

                new_block->block_type = EJCFGBT_IF;
                new_block->jbpc_end = fall_pc;

                JCFGBlock_t* jump_block = JCFG_lookup(compiler->root,compiler->last_visit_id++,jump_pc);
                JCFGBlock_t* fall_block = JCFG_lookup(compiler->root,compiler->last_visit_id++,fall_pc);

                if(!fall_block){
                    JError_t childerr = JCFG_generate(compiler, &fall_block, fall_pc);
                    FAIL_SET_JUMP(childerr == JERR_OK,err,childerr,exit);
                }
                if(!jump_block){
                    JError_t childerr = JCFG_generate(compiler, &jump_block, jump_pc);
                    FAIL_SET_JUMP(childerr == JERR_OK,err,childerr,exit);
                }

                new_block->children_count = 2;
                new_block->children = bumper_calloc(compiler->arena,new_block->children_count,sizeof(*new_block->children));
                FAIL_SET_JUMP(new_block->children,err,JERR_OOM,exit);

                new_block->children[0] = fall_block;
                new_block->children[1] = jump_block;

                return JERR_OK;
            }
            break;

            //TODO: tableswitch, lookupswitch and other shit

            case ATHROW:
            case IRETURN:
            case LRETURN:
            case FRETURN:
            case DRETURN:
            case ARETURN:
            case RETURN:
            case RET:
                new_block->jbpc_end = pc + jvm_insn_size[opcode];
                return JERR_OK;
        }
    }

    new_block->jbpc_end = pc;
exit:
    return err;
}

void JCFG_test(JCFGBlock_t* root){
    printf("[%d] -> type: %d, jbpc: (%ld:%ld), subblocks count: %d, {",root->block_id,root->block_type,root->jbpc_start,root->jbpc_end,root->children_count);
    root->visit_id = -1;
    for(unsigned i = 0; i < root->children_count; i++){
        printf("%d:",root->children[i]->block_id);
    }
    printf("}\n");
    for(unsigned i = 0; i < root->children_count; i++){
        if(root->children[i]->visit_id != -1){
            JCFG_test(root->children[i]);
        }
    }
}