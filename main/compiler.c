#include "compiler.h"
#include "cfg.h"
#include "jeex_builder.h"
#include "jerror.h"
#include "linker.h"
#include "class.h"


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

JError_t JEEXCompiler_start(JEEXBuilder_t* jeex_builder){
    return JERR_UNKNOWN;
}

