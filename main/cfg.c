#include "cfg.h"
#include "bumper.h"
#include "class.h"
#include "jerror.h"
#include "list.h"
#include "loader.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "opcodes.h"
#include "lb_endian.h"

/**
 * Array of opcode sizes (total bytes including the opcode).
 * Indexed by opcode value using designated initializers with enum constants.
 * For instructions with variable length (tableswitch, lookupswitch, wide)
 * the size is given as 1, indicating that the actual length must be computed
 * from the operands. Undefined opcodes have size 0 (default).
 */
uint8_t opcode_sizes[256] = {
    // 0x00 - 0x0f
    [EJOPCODE_NOP] = 1, [EJOPCODE_ACONST_NULL] = 1,
    [EJOPCODE_ICONST_M1] = 1, [EJOPCODE_ICONST_0] = 1,
    [EJOPCODE_ICONST_1] = 1, [EJOPCODE_ICONST_2] = 1,
    [EJOPCODE_ICONST_3] = 1, [EJOPCODE_ICONST_4] = 1,
    [EJOPCODE_ICONST_5] = 1, [EJOPCODE_LCONST_0] = 1,
    [EJOPCODE_LCONST_1] = 1, [EJOPCODE_FCONST_0] = 1,
    [EJOPCODE_FCONST_1] = 1, [EJOPCODE_FCONST_2] = 1,
    [EJOPCODE_DCONST_0] = 1, [EJOPCODE_DCONST_1] = 1,

    // 0x10 - 0x14
    [EJOPCODE_BIPUSH] = 2, [EJOPCODE_SIPUSH] = 3,
    [EJOPCODE_LDC] = 2, [EJOPCODE_LDC_W] = 3,
    [EJOPCODE_LDC2_W] = 3,

    // 0x15 - 0x19
    [EJOPCODE_ILOAD] = 2, [EJOPCODE_LLOAD] = 2,
    [EJOPCODE_FLOAD] = 2, [EJOPCODE_DLOAD] = 2,
    [EJOPCODE_ALOAD] = 2,

    // 0x1a - 0x2d (shorthand loads)
    [EJOPCODE_ILOAD_0] = 1, [EJOPCODE_ILOAD_1] = 1,
    [EJOPCODE_ILOAD_2] = 1, [EJOPCODE_ILOAD_3] = 1,
    [EJOPCODE_LLOAD_0] = 1, [EJOPCODE_LLOAD_1] = 1,
    [EJOPCODE_LLOAD_2] = 1, [EJOPCODE_LLOAD_3] = 1,
    [EJOPCODE_FLOAD_0] = 1, [EJOPCODE_FLOAD_1] = 1,
    [EJOPCODE_FLOAD_2] = 1, [EJOPCODE_FLOAD_3] = 1,
    [EJOPCODE_DLOAD_0] = 1, [EJOPCODE_DLOAD_1] = 1,
    [EJOPCODE_DLOAD_2] = 1, [EJOPCODE_DLOAD_3] = 1,
    [EJOPCODE_ALOAD_0] = 1, [EJOPCODE_ALOAD_1] = 1,
    [EJOPCODE_ALOAD_2] = 1, [EJOPCODE_ALOAD_3] = 1,

    // 0x2e - 0x35 (array loads)
    [EJOPCODE_IALOAD] = 1, [EJOPCODE_LALOAD] = 1,
    [EJOPCODE_FALOAD] = 1, [EJOPCODE_DALOAD] = 1,
    [EJOPCODE_AALOAD] = 1, [EJOPCODE_BALOAD] = 1,
    [EJOPCODE_CALOAD] = 1, [EJOPCODE_SALOAD] = 1,

    // 0x36 - 0x3a
    [EJOPCODE_ISTORE] = 2, [EJOPCODE_LSTORE] = 2,
    [EJOPCODE_FSTORE] = 2, [EJOPCODE_DSTORE] = 2,
    [EJOPCODE_ASTORE] = 2,

    // 0x3b - 0x4e (shorthand stores)
    [EJOPCODE_ISTORE_0] = 1, [EJOPCODE_ISTORE_1] = 1,
    [EJOPCODE_ISTORE_2] = 1, [EJOPCODE_ISTORE_3] = 1,
    [EJOPCODE_LSTORE_0] = 1, [EJOPCODE_LSTORE_1] = 1,
    [EJOPCODE_LSTORE_2] = 1, [EJOPCODE_LSTORE_3] = 1,
    [EJOPCODE_FSTORE_0] = 1, [EJOPCODE_FSTORE_1] = 1,
    [EJOPCODE_FSTORE_2] = 1, [EJOPCODE_FSTORE_3] = 1,
    [EJOPCODE_DSTORE_0] = 1, [EJOPCODE_DSTORE_1] = 1,
    [EJOPCODE_DSTORE_2] = 1, [EJOPCODE_DSTORE_3] = 1,
    [EJOPCODE_ASTORE_0] = 1, [EJOPCODE_ASTORE_1] = 1,
    [EJOPCODE_ASTORE_2] = 1, [EJOPCODE_ASTORE_3] = 1,

    // 0x4f - 0x56 (array stores)
    [EJOPCODE_IASTORE] = 1, [EJOPCODE_LASTORE] = 1,
    [EJOPCODE_FASTORE] = 1, [EJOPCODE_DASTORE] = 1,
    [EJOPCODE_AASTORE] = 1, [EJOPCODE_BASTORE] = 1,
    [EJOPCODE_CASTORE] = 1, [EJOPCODE_SASTORE] = 1,

    // 0x57 - 0x5f (stack)
    [EJOPCODE_POP] = 1, [EJOPCODE_POP2] = 1,
    [EJOPCODE_DUP] = 1, [EJOPCODE_DUP_X1] = 1,
    [EJOPCODE_DUP_X2] = 1, [EJOPCODE_DUP2] = 1,
    [EJOPCODE_DUP2_X1] = 1, [EJOPCODE_DUP2_X2] = 1,
    [EJOPCODE_SWAP] = 1,

    // 0x60 - 0x83 (arithmetic, shift, bitwise)
    [EJOPCODE_IADD] = 1, [EJOPCODE_LADD] = 1,
    [EJOPCODE_FADD] = 1, [EJOPCODE_DADD] = 1,
    [EJOPCODE_ISUB] = 1, [EJOPCODE_LSUB] = 1,
    [EJOPCODE_FSUB] = 1, [EJOPCODE_DSUB] = 1,
    [EJOPCODE_IMUL] = 1, [EJOPCODE_LMUL] = 1,
    [EJOPCODE_FMUL] = 1, [EJOPCODE_DMUL] = 1,
    [EJOPCODE_IDIV] = 1, [EJOPCODE_LDIV] = 1,
    [EJOPCODE_FDIV] = 1, [EJOPCODE_DDIV] = 1,
    [EJOPCODE_IREM] = 1, [EJOPCODE_LREM] = 1,
    [EJOPCODE_FREM] = 1, [EJOPCODE_DREM] = 1,
    [EJOPCODE_INEG] = 1, [EJOPCODE_LNEG] = 1,
    [EJOPCODE_FNEG] = 1, [EJOPCODE_DNEG] = 1,
    [EJOPCODE_ISHL] = 1, [EJOPCODE_LSHL] = 1,
    [EJOPCODE_ISHR] = 1, [EJOPCODE_LSHR] = 1,
    [EJOPCODE_IUSHR] = 1, [EJOPCODE_LUSHR] = 1,
    [EJOPCODE_IAND] = 1, [EJOPCODE_LAND] = 1,
    [EJOPCODE_IOR] = 1, [EJOPCODE_LOR] = 1,
    [EJOPCODE_IXOR] = 1, [EJOPCODE_LXOR] = 1,

    // 0x84 iinc
    [EJOPCODE_IINC] = 3,

    // 0x85 - 0x93 (conversions)
    [EJOPCODE_I2L] = 1, [EJOPCODE_I2F] = 1,
    [EJOPCODE_I2D] = 1, [EJOPCODE_L2I] = 1,
    [EJOPCODE_L2F] = 1, [EJOPCODE_L2D] = 1,
    [EJOPCODE_F2I] = 1, [EJOPCODE_F2L] = 1,
    [EJOPCODE_F2D] = 1, [EJOPCODE_D2I] = 1,
    [EJOPCODE_D2L] = 1, [EJOPCODE_D2F] = 1,
    [EJOPCODE_I2B] = 1, [EJOPCODE_I2C] = 1,
    [EJOPCODE_I2S] = 1,

    // 0x94 - 0x98 (comparisons)
    [EJOPCODE_LCMP] = 1, [EJOPCODE_FCMPL] = 1,
    [EJOPCODE_FCMPG] = 1, [EJOPCODE_DCMPL] = 1,
    [EJOPCODE_DCMPG] = 1,

    // 0x99 - 0xa6 (conditional branches)
    [EJOPCODE_IFEQ] = 3, [EJOPCODE_IFNE] = 3,
    [EJOPCODE_IFLT] = 3, [EJOPCODE_IFGE] = 3,
    [EJOPCODE_IFGT] = 3, [EJOPCODE_IFLE] = 3,
    [EJOPCODE_IF_ICMPEQ] = 3, [EJOPCODE_IF_ICMPNE] = 3,
    [EJOPCODE_IF_ICMPLT] = 3, [EJOPCODE_IF_ICMPGE] = 3,
    [EJOPCODE_IF_ICMPGT] = 3, [EJOPCODE_IF_ICMPLE] = 3,
    [EJOPCODE_IF_ACMPEQ] = 3, [EJOPCODE_IF_ACMPNE] = 3,

    // 0xa7 - 0xa9 (unconditional branches)
    [EJOPCODE_GOTO] = 3, [EJOPCODE_JSR] = 3,
    [EJOPCODE_RET] = 2,

    // 0xaa - 0xab (table switches - variable length)
    [EJOPCODE_TABLESWITCH] = 1, [EJOPCODE_LOOKUPSWITCH] = 1,

    // 0xac - 0xb1 (returns)
    [EJOPCODE_IRETURN] = 1, [EJOPCODE_LRETURN] = 1,
    [EJOPCODE_FRETURN] = 1, [EJOPCODE_DRETURN] = 1,
    [EJOPCODE_ARETURN] = 1, [EJOPCODE_RETURN] = 1,

    // 0xb2 - 0xb5 (field access)
    [EJOPCODE_GETSTATIC] = 3, [EJOPCODE_PUTSTATIC] = 3,
    [EJOPCODE_GETFIELD] = 3, [EJOPCODE_PUTFIELD] = 3,

    // 0xb6 - 0xba (method invocation)
    [EJOPCODE_INVOKEVIRTUAL] = 3, [EJOPCODE_INVOKESPECIAL] = 3,
    [EJOPCODE_INVOKESTATIC] = 3, [EJOPCODE_INVOKEINTERFACE] = 5,
    [EJOPCODE_INVOKEDYNAMIC] = 5,

    // 0xbb - 0xbd (object allocation)
    [EJOPCODE_NEW] = 3, [EJOPCODE_NEWARRAY] = 2,
    [EJOPCODE_ANEWARRAY] = 3,

    // 0xbe - 0xbf
    [EJOPCODE_ARRAYLENGTH] = 1, [EJOPCODE_ATHROW] = 1,

    // 0xc0 - 0xc1
    [EJOPCODE_CHECKCAST] = 3, [EJOPCODE_INSTANCEOF] = 3,

    // 0xc2 - 0xc3
    [EJOPCODE_MONITORENTER] = 1, [EJOPCODE_MONITOREXIT] = 1,

    // 0xc4 wide (prefix, variable length)
    [EJOPCODE_WIDE] = 1,

    // 0xc5 multianewarray
    [EJOPCODE_MULTIANEWARRAY] = 4,

    // 0xc6 - 0xc7
    [EJOPCODE_IFNULL] = 3, [EJOPCODE_IFNONNULL] = 3,

    // 0xc8 - 0xc9 (wide branches)
    [EJOPCODE_GOTO_W] = 5, [EJOPCODE_JSR_W] = 5,

    // 0xca breakpoint
    [EJOPCODE_BREAKPOINT] = 1,

    [EJOPCODE_IMPDEP1] = 1, [EJOPCODE_IMPDEP2] = 1
};

JError_t JCFG_init(JCFG_t* cfg, JCodeAttribute_t* bytecode,JMethod_t* method, bump_allocator_t* arena){
    memset(cfg,0,sizeof(*cfg));
    cfg->bytecode = bytecode;
    cfg->arena = arena;
    cfg->method = method;
    
    return JERR_OK;
}

typedef struct{
    struct list_head list; //Used when counting them initialy
    uint32_t target;
}JCFGTemporaryLabel_t;

static unsigned remove_duplicates(uint32_t* arr, unsigned n) {
    if (n <= 1)
        return n;

    int idx = 1; 
  
    for (int i = 1; i < n; i++) {
        if (arr[i] != arr[i - 1]) {
            arr[idx++] = arr[i];
        }
    }
    return idx;
}

static int qsort_uint32_cmp(const void* a, const void* b){
    return *(uint32_t*)a - *(uint32_t*)b;
}

static int qsort_cfgblock_cmp(const void* a, const void* b){
    return (*(JCFGBlock_t**)a)->start_pc - (*(JCFGBlock_t**)b)->start_pc;
}

static JError_t count_labels(JCFG_t* cfg){
    JError_t err = JERR_OK;

    uint32_t code_length = cfg->bytecode->code_length;
    uint8_t* code = cfg->bytecode->code;

    struct list_head labels_list = LIST_HEAD_INIT(labels_list);
    size_t labels_count = 0;


    JCFGTemporaryLabel_t* root_label = alloca(sizeof(*root_label)); //Lets hope stack wont overflow
    INIT_LIST_HEAD(&root_label->list);
    list_add_tail(&root_label->list,&labels_list);
    root_label->target = 0;
    labels_count++;

    for(unsigned i = 0; i < cfg->bytecode->exception_table_length; i++){
        typeof(cfg->bytecode->exception_table[0])* exception = &cfg->bytecode->exception_table[i];

        JCFGTemporaryLabel_t* exception_start = alloca(sizeof(*exception_start));
        INIT_LIST_HEAD(&exception_start->list);
        list_add_tail(&exception_start->list,&labels_list);
        exception_start->target = exception->start_pc;
        labels_count++;   
        
        JCFGTemporaryLabel_t* exception_end = alloca(sizeof(*exception_end));
        INIT_LIST_HEAD(&exception_end->list);
        list_add_tail(&exception_end->list,&labels_list);
        exception_end->target = exception->end_pc;
        labels_count++;  
    }
    
    for(uint32_t pc = 0; pc < code_length; pc += opcode_sizes[code[pc]]){
        JOpcode_t opcode = code[pc];
        switch(opcode){
            default: break;

            case EJOPCODE_GOTO:
            case EJOPCODE_GOTO_W:{
                    int32_t offset = opcode == EJOPCODE_GOTO ? (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]) : (int32_t)be32_to_cpu(*(uint32_t*)&code[pc + 1]);
                    int32_t goto_pc = pc + offset;
                    FAIL_SET_JUMP(goto_pc >= 0 && goto_pc < code_length,err,JERR_BADPARAM,exit);

                    JCFGTemporaryLabel_t* label = alloca(sizeof(*label));
                    INIT_LIST_HEAD(&label->list);
                    list_add_tail(&label->list,&labels_list);
                    label->target = goto_pc;
                    labels_count++;
                }
                break;

            case EJOPCODE_JSR:
            case EJOPCODE_JSR_W:{
                    int32_t offset = opcode == EJOPCODE_JSR ? (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]) : (int32_t)be32_to_cpu(*(uint32_t*)&code[pc + 1]);
                    int32_t jump_pc = pc + offset;
                    int32_t fallthrough_pc = pc + opcode_sizes[opcode];

                    FAIL_SET_JUMP(jump_pc >= 0 && jump_pc < code_length,err,JERR_BADPARAM,exit);
                    FAIL_SET_JUMP(fallthrough_pc >= 0 && fallthrough_pc < code_length,err,JERR_BADPARAM,exit);

                    JCFGTemporaryLabel_t* jump_label = alloca(sizeof(*jump_label));
                    INIT_LIST_HEAD(&jump_label->list);
                    list_add_tail(&jump_label->list,&labels_list);
                    jump_label->target = jump_pc;
                    
                    JCFGTemporaryLabel_t* fall_label = alloca(sizeof(*fall_label));
                    INIT_LIST_HEAD(&fall_label->list);
                    list_add_tail(&fall_label->list,&labels_list);
                    fall_label->target = fallthrough_pc;  
                    
                    labels_count += 2;
                }
                break;

            case EJOPCODE_INVOKEINTERFACE:
            case EJOPCODE_INVOKEVIRTUAL:
            case EJOPCODE_INVOKESTATIC:
            case EJOPCODE_INVOKESPECIAL:
            case EJOPCODE_NEW:
            case EJOPCODE_NEWARRAY:
            case EJOPCODE_ANEWARRAY:
            case EJOPCODE_MULTIANEWARRAY:{
                    JCFGTemporaryLabel_t* new_label = alloca(sizeof(*new_label));
                    INIT_LIST_HEAD(&new_label->list);
                    list_add_tail(&new_label->list,&labels_list);
                    new_label->target = pc;
                    
                    labels_count++;
            }
            break;

            case EJOPCODE_IFEQ:
            case EJOPCODE_IFNE:
            case EJOPCODE_IFLT:
            case EJOPCODE_IFGE:
            case EJOPCODE_IFGT:
            case EJOPCODE_IFLE:
            case EJOPCODE_IF_ICMPEQ:
            case EJOPCODE_IF_ICMPNE:
            case EJOPCODE_IF_ICMPLT:
            case EJOPCODE_IF_ICMPGE:
            case EJOPCODE_IF_ICMPGT:
            case EJOPCODE_IF_ICMPLE:
            case EJOPCODE_IF_ACMPEQ:
            case EJOPCODE_IF_ACMPNE:
            case EJOPCODE_IFNULL:
            case EJOPCODE_IFNONNULL:{
                    int32_t offset = (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]);
                    int32_t jump_pc = pc + offset;
                    int32_t fallthrough_pc = pc + opcode_sizes[opcode];

                    FAIL_SET_JUMP(jump_pc >= 0 && jump_pc < code_length,err,JERR_BADPARAM,exit);
                    FAIL_SET_JUMP(fallthrough_pc >= 0 && fallthrough_pc < code_length,err,JERR_BADPARAM,exit);

                    JCFGTemporaryLabel_t* jump_label = alloca(sizeof(*jump_label));
                    INIT_LIST_HEAD(&jump_label->list);
                    list_add_tail(&jump_label->list,&labels_list);
                    jump_label->target = jump_pc;
                    
                    JCFGTemporaryLabel_t* fall_label = alloca(sizeof(*fall_label));
                    INIT_LIST_HEAD(&fall_label->list);
                    list_add_tail(&fall_label->list,&labels_list);
                    fall_label->target = fallthrough_pc;  
                    
                    labels_count += 2;
                }
                break;

            case EJOPCODE_TABLESWITCH:
                assert(0 && "TODO: tableswitch");
                break;

            case EJOPCODE_LOOKUPSWITCH:
                assert(0 && "TODO: lookupswitch");
                break;
            
        }
    }

    uint32_t* all_labels = alloca(sizeof(*all_labels) * labels_count);
    JCFGTemporaryLabel_t* cur_label = NULL;
    unsigned cur_label_i = 0;
    list_for_each_entry(cur_label,&labels_list,list){
        all_labels[cur_label_i++] = cur_label->target;
    }
    assert(cur_label_i == labels_count);

    qsort(all_labels,labels_count,sizeof(*all_labels),qsort_uint32_cmp);
    unsigned dedup_size = remove_duplicates(all_labels,labels_count);

    cfg->labels_count = dedup_size;
    cfg->labels = bumper_calloc(cfg->arena,cfg->labels_count,sizeof(*cfg->labels));
    FAIL_SET_JUMP(cfg->labels,err,JERR_OOM,exit);

    memcpy(cfg->labels,all_labels,dedup_size * sizeof(*all_labels));

    cfg->block_count = cfg->labels_count + cfg->bytecode->exception_table_length;
exit:
    return err;
}

//Stackmap generator remnant
//#define IS_REF(bitmap, idx) (((bitmap)[(idx) > 0 ? (idx)/8 : 0] >> ((idx) > 0 ? (idx)%8 : 0)) & 1)
//#define SET_REF(bitmap, idx) (bitmap)[(idx) > 0 ? (idx)/8 : 0] |= (1 << ((idx) > 0 ? (idx)%8 : 0))
//#define CLEAR_REF(bitmap, idx) (bitmap)[(idx) > 0 ? (idx)/8 : 0] &= ~(1 << ((idx) > 0 ? (idx)%8 : 0))
//#define PUSH(bitmap, sp, is_ref) ({if((is_ref))  {SET_REF((bitmap),(*sp));} else {CLEAR_REF((bitmap),(*sp));} (*sp)++;})
//#define POP(bitmap, sp) ({--(*sp); (IS_REF((bitmap),(*sp)));})

//Create blocks, but dont build graph yet
static JError_t create_blocks(JCFG_t* cfg){
    JError_t err = JERR_OK;

    cfg->blocks = bumper_calloc(cfg->arena,cfg->block_count,sizeof(*cfg->blocks));
    FAIL_SET_JUMP(cfg->blocks,err,JERR_OOM,exit);

    
    unsigned block_index = 0;
    for(unsigned i = 0; i < cfg->labels_count; i++){
        uint32_t start_pc = cfg->labels[i];
        uint32_t end_pc = i + 1 < cfg->labels_count ? cfg->labels[i + 1] : cfg->bytecode->code_length;

        JCFGBlock_t* new_block = bumper_calloc(cfg->arena,1,sizeof(*new_block));
        FAIL_SET_JUMP(new_block,err,JERR_OOM,exit);

        new_block->start_pc = start_pc;
        new_block->end_pc = end_pc;

        cfg->blocks[block_index++] = new_block;
    }

    //Temporary solution without dedublication!
    for(unsigned i = 0; i < cfg->bytecode->exception_table_length; i++){
        typeof(cfg->bytecode->exception_table[0])* exception = &cfg->bytecode->exception_table[i];
        JCFGBlock_t* handler = bumper_calloc(cfg->arena,1,sizeof(*handler));
        FAIL_SET_JUMP(handler,err,JERR_OOM,exit);

        handler->start_pc = exception->handler_pc;
        handler->end_pc = cfg->bytecode->code_length;
        handler->flags.is_exception = 1;

        cfg->blocks[block_index++] = handler;
    }

    qsort(cfg->blocks,cfg->block_count,sizeof(*cfg->blocks),qsort_cfgblock_cmp);

exit:
    return err;
}

static JCFGBlock_t* find_cfg_block(JCFG_t* cfg, uint32_t start_pc){
    JCFGBlock_t template = {
        .start_pc = start_pc,
    };
    JCFGBlock_t* template_ptr = &template;
    JCFGBlock_t** found = bsearch(&template_ptr,cfg->blocks,cfg->block_count,sizeof(*cfg->blocks),qsort_cfgblock_cmp);

    return found ? *found : NULL;
}

static JError_t build_graph(JCFG_t* cfg){
    JError_t err = JERR_OK;

    JCFGTemporaryStack_t stack = {0};
    INIT_STACK(&stack, cfg->block_count);

    cfg->exceptions_count = cfg->bytecode->exception_table_length;
    cfg->exceptions = bumper_calloc(cfg->arena,cfg->exceptions_count,sizeof(*cfg->exceptions));
    FAIL_SET_JUMP(cfg->exceptions,err,JERR_OOM,exit);

    uint8_t exception_dedup_id = 0xEE;
    for(unsigned i = 0; i < cfg->exceptions_count; i++){
        JCFGException_t* cfg_exception = &cfg->exceptions[i];
        typeof(cfg->bytecode->exception_table[0])* exception = &cfg->bytecode->exception_table[i];

        cfg_exception->catch_type = exception->catch_type;
        cfg_exception->start_pc = exception->start_pc;
        cfg_exception->end_pc = exception->end_pc;
        FAIL_SET_JUMP((cfg_exception->handler = find_cfg_block(cfg,exception->handler_pc)),err,JERR_OOM,exit);
        
        JCFGBlock_t* handler = cfg_exception->handler;
        //JCFGBlock_t* trigger = find_cfg_block(cfg, cfg_exception->start_pc);
        //FAIL_SET_JUMP(trigger,err,JERR_NOTFOUND,exit);

        if(handler->visit_id != exception_dedup_id){ //No need to add it to stack twice
            handler->visit_id = exception_dedup_id;
            STACK_PUSH(&stack,handler); //Build this block
        }
    }

    JCFGBlock_t* root = find_cfg_block(cfg,0);
    FAIL_SET_JUMP(root,err,JERR_UNKNOWN,exit);

    cfg->root = root;
    STACK_PUSH(&stack, root); 


    uint8_t visit_id = 0xCE;
    uint8_t* code = cfg->bytecode->code;
    JCFGBlock_t* cur_block = NULL;
    while((cur_block = (void*)STACK_POP(&stack))){
        cur_block->visit_id = visit_id;
        
        for(uint32_t pc = cur_block->start_pc; pc < cur_block->end_pc; pc += opcode_sizes[code[pc]]){
            JOpcode_t opcode = code[pc];
            uint32_t next_pc = pc + opcode_sizes[opcode];
            cur_block->last_opcode_pc = pc; //Just required to patch IF/GOTOs and other shit

            switch(opcode){
                default: break;

                case EJOPCODE_GOTO:
                case EJOPCODE_GOTO_W:{
                    int32_t offset = opcode == EJOPCODE_GOTO ? (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]) : (int32_t)be32_to_cpu(*(uint32_t*)&code[pc + 1]);
                    int32_t goto_pc = pc + offset;

                    cur_block->end_pc = next_pc;
                    cur_block->type = EJCFGBT_GOTO;
                    cur_block->children_count = 1;
                    cur_block->children = bumper_calloc(cfg->arena,cur_block->children_count,sizeof(*cur_block->children));
                    FAIL_SET_JUMP(cur_block->children,err,JERR_OOM,exit);

                    JCFGBlock_t* target = find_cfg_block(cfg,goto_pc);
                    FAIL_SET_JUMP(target,err,JERR_NOTFOUND,exit);

                    cur_block->children[0] = target;
 
                    if(target->visit_id != visit_id)
                        STACK_PUSH(&stack,target);

                    goto loop_end;
                }
                break;

                #if 1
                case EJOPCODE_JSR:
                case EJOPCODE_JSR_W:{
                    int32_t offset = opcode == EJOPCODE_JSR ? (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]) : (int32_t)be32_to_cpu(*(uint32_t*)&code[pc + 1]);
                    int32_t goto_pc = pc + offset;
                    int32_t fallthrough_pc = opcode_sizes[opcode] + pc;

                    cur_block->end_pc = next_pc;
                    cur_block->type = EJCFGBT_JSR;
                    cur_block->children_count = 2;
                    cur_block->children = bumper_calloc(cfg->arena,cur_block->children_count,sizeof(*cur_block->children));
                    FAIL_SET_JUMP(cur_block->children,err,JERR_OOM,exit);

                    JCFGBlock_t* jump_target = find_cfg_block(cfg,goto_pc);
                    FAIL_SET_JUMP(jump_target,err,JERR_NOTFOUND,exit);
                
                    JCFGBlock_t* fall_target = find_cfg_block(cfg,fallthrough_pc);
                    FAIL_SET_JUMP(fall_target,err,JERR_NOTFOUND,exit);

                    cur_block->children[0] = fall_target;
                    cur_block->children[1] = jump_target;
 
                    if(fall_target->visit_id != visit_id)
                        STACK_PUSH(&stack,fall_target);

                    if(jump_target->visit_id != visit_id)
                        STACK_PUSH(&stack,jump_target);

                    goto loop_end;
                }
                break;
                #else
                #define RET_DISABLED
                case EJOPCODE_JSR:
                case EJOPCODE_JSR_W:
                case EJOPCODE_RET:
                    assert(0 && "JSR ARE NOT SUPPORT! DO INLINING LATER!");
                #endif

                case EJOPCODE_IFEQ:
                case EJOPCODE_IFNE:
                case EJOPCODE_IFLT:
                case EJOPCODE_IFGE:
                case EJOPCODE_IFGT:
                case EJOPCODE_IFLE:
                case EJOPCODE_IF_ICMPEQ:
                case EJOPCODE_IF_ICMPNE:
                case EJOPCODE_IF_ICMPLT:
                case EJOPCODE_IF_ICMPGE:
                case EJOPCODE_IF_ICMPGT:
                case EJOPCODE_IF_ICMPLE:
                case EJOPCODE_IF_ACMPEQ:
                case EJOPCODE_IF_ACMPNE:
                case EJOPCODE_IFNULL:
                case EJOPCODE_IFNONNULL:{
                    int32_t offset = (int16_t)be16_to_cpu(*(uint16_t*)&code[pc + 1]);
                    int32_t goto_pc = pc + offset;
                    int32_t fallthrough_pc = opcode_sizes[opcode] + pc;

                    cur_block->end_pc = next_pc;
                    cur_block->type = EJCFGBT_IF;
                    cur_block->children_count = 2;
                    cur_block->children = bumper_calloc(cfg->arena,cur_block->children_count,sizeof(*cur_block->children));
                    FAIL_SET_JUMP(cur_block->children,err,JERR_OOM,exit);

                    JCFGBlock_t* jump_target = find_cfg_block(cfg,goto_pc);
                    FAIL_SET_JUMP(jump_target,err,JERR_NOTFOUND,exit);
                
                    JCFGBlock_t* fall_target = find_cfg_block(cfg,fallthrough_pc);
                    FAIL_SET_JUMP(fall_target,err,JERR_NOTFOUND,exit);

                    cur_block->children[0] = fall_target;
                    cur_block->children[1] = jump_target;
 
                    if(fall_target->visit_id != visit_id)
                        STACK_PUSH(&stack,fall_target);

                    if(jump_target->visit_id != visit_id)
                        STACK_PUSH(&stack,jump_target);

                    goto loop_end;
                }
                break;   
                
                case EJOPCODE_LOOKUPSWITCH:
                case EJOPCODE_TABLESWITCH:
                    assert(0 && "TODO: switches");
                    break;

                case EJOPCODE_ATHROW:
                #ifndef RET_DISABLED
                case EJOPCODE_RET:
                #endif
                case EJOPCODE_RETURN:
                case EJOPCODE_IRETURN:
                case EJOPCODE_LRETURN:
                case EJOPCODE_FRETURN:
                case EJOPCODE_DRETURN:
                case EJOPCODE_ARETURN:{
                    cur_block->end_pc = next_pc;
                    cur_block->type = EJCFGBT_END;
                    goto loop_end;
                }
                break;
            }
        }

        loop_end:
        if(cur_block->type == EJCFGBT_CODE){ //Handle block end like this only if there are no ending opcodes was found
            JCFGBlock_t* possible_continue = find_cfg_block(cfg,cur_block->end_pc);
            if(possible_continue){
                cur_block->children_count = 1;
                cur_block->children = bumper_calloc(cfg->arena,1,sizeof(*cur_block->children));
                FAIL_SET_JUMP(cur_block->children,err,JERR_OOM,exit);

                cur_block->children[0] = possible_continue;
                if(possible_continue->visit_id != visit_id)
                    STACK_PUSH(&stack,possible_continue);
            }
        }

    }
exit:
    return err;
}


JError_t JCFG_build(JCFG_t* cfg){
    JError_t err = JERR_OK;

    FAIL_SET_JUMP((err = count_labels(cfg)) == JERR_OK,err,err,exit);
    FAIL_SET_JUMP((err = create_blocks(cfg)) == JERR_OK,err,err,exit);
    FAIL_SET_JUMP((err = build_graph(cfg)) == JERR_OK,err,err,exit);

exit:
    return err;
}