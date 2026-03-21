#include "compiler.h"
#include "bumper.h"
#include "class.h"
#include "jerror.h"

static void* ht_arena_alloc(void* userctx, size_t size){
    return bumper_alloc(userctx,size);
}

int JSymbolTable_init(JSymbolTable_t* symtab, bump_allocator_t* arena){
    hashmap_init(&symtab->symmap,32,ht_arena_alloc,hashmap_pointer_hash,hashmap_pointer_cmp,arena);
    symtab->cur_symindex = 0;
    symtab->arena = arena;

    return 0;
}

//return symbol associated with this pointer
JSymbol_t* JSymbolTable_get(JSymbolTable_t* symtab, void* ptr){
    return hashmap_get(&symtab->symmap,ptr);
}

JError_t JSymbolTable_put(JSymbolTable_t* symtab, void* ptr, JSymbol_t symbol){
    JError_t err = JERR_OK;
    JSymbol_t* sym_copy = bumper_calloc(symtab->arena,1,sizeof(*sym_copy));
    FAIL_SET_JUMP(sym_copy,err,JERR_OOM,exit);

    *sym_copy = symbol;
    hashmap_set(&symtab->symmap,ptr,sym_copy);

exit:
    return err;
}

int JCompiler_init(JCompiler_t* compiler, JLinker_t* linker, bump_allocator_t* arena){
    compiler->arena = arena;
    compiler->linker = linker;

    return JSymbolTable_init(&compiler->symtab, arena);
}