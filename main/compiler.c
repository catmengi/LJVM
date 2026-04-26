#include "compiler.h"
#include "cfg.h"
#include "jeex.h"
#include "jeex_builder.h"
#include "jerror.h"
#include "linker.h"
#include "class.h"
#include "loader.h"


static JError_t iterate_cfg(JCFG_t* cfg, void* user_data, JError_t (*cb)(JCFGBlock_t* block, void* user_data)){
    static uint8_t visit_id = 0xFF;
    JError_t err = JERR_OK;

    JCFGTemporaryStack_t worklist = {0};
    INIT_STACK(&worklist, cfg->block_count);

    for(unsigned i = 0; i < cfg->exceptions_count; i++){
        STACK_PUSH(&worklist, cfg->exceptions[i].handler);
    }
    STACK_PUSH(&worklist, cfg->root);

    JCFGBlock_t* block = NULL;
    while((block = (JCFGBlock_t*)STACK_POP(&worklist))){
        block->visit_id = visit_id;
            
        err = cb(block, user_data);
        FAIL_SET_JUMP(err == JERR_OK,err,err,exit);

        for(unsigned i = 0; i < block->children_count; i++){
            JCFGBlock_t* child = block->children[i];
            if(child->visit_id != visit_id)
                STACK_PUSH(&worklist,child);
        }
    }
    
exit:
    visit_id--;
    return err;
}

static JError_t bytecode_size_count(JCFGBlock_t* block, void* user_data){
    JError_t err = JERR_OK;
    JEEXCompilerMethodInfo_t* cmethod_info = user_data;

    JMethod_t* method = cmethod_info->origin_method;
    JCodeAttribute_t* java_bytecode = method->code;

    assert(block->custom_data == NULL);
    
    block->custom_data = bumper_calloc(cmethod_info->builder->arena,1,sizeof(JEEXCompilerCodeBlock_t));
    FAIL_SET_JUMP(block->custom_data,err,JERR_OOM,exit);

    JEEXCompilerCodeBlock_t* block_compinfo = block->custom_data;
    uint8_t* code = java_bytecode->code;
    uint32_t block_length = 0;
    for(uint32_t pc = block->start_pc; pc < block->end_pc; pc += opcode_sizes[code[pc]]){
        uint8_t opcode = java_bytecode->code[pc];
        switch(opcode){
            default: 
                printf("Unknown opcode: %d\n",opcode);
                break;
        }
    }

    block_compinfo->size = block_length;
    cmethod_info->code_length += block_length;

exit:
    return err;
}

static JError_t iterate_classes(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    JLinkerMetadata_t* meta = class->metadata;
    for(unsigned i = 0; i < meta->methods.count; i++){
        JMethod_t* method = (void*)meta->methods.elements[i];

        JEEXCompilerMethodInfo_t cmethod_info = {
            .origin_method = method,
            .origin_class = class,
            .builder = builder,
        };

        if(method->flags.is_native == 0){
            FAIL_SET_JUMP(iterate_cfg(&method->cfg,&cmethod_info,bytecode_size_count) == JERR_OK,err,JERR_UNKNOWN,exit);
        }
    }

    JClass_t* child = NULL;
    list_for_each_entry(child,&class->children,as_child){
        FAIL_SET_JUMP(iterate_classes(child, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

JError_t JEEXCompiler_start(JEEXBuilder_t* jeex_builder){
    JError_t err = JERR_OK;

    JClass_t* root = NULL;
    list_for_each_entry(root,&jeex_builder->linker->root_list,as_child){
        FAIL_SET_JUMP((err = iterate_classes(root, jeex_builder)) == JERR_OK,err,err,exit);
    }
    
exit:
    return err;
}

