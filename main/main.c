#include "bumper.h"
#include "cfg.h"
#include "class.h"
#include "compiler.h"
#include "linker.h"
#include "preloader.h"

void app_main(){
    bump_allocator_t arena = {0};
    bumper_create(&arena,2 * 1024 * 1024);

    JLoader_t loader = {0};
    JLoader_init(&loader,&arena);
    JPreloader_load_builtins(&loader);

    JLinker_t linker = {0};
    JLinker_init(&linker,&loader,&arena);
    JLinker_link(&linker);

    void* arena_top = bumper_alloc(&arena,0);

    //JCompiler_t compiler = {0};
    //JCompiler_init(&compiler,&linker,"/sd/abcd_app");
    //JCompiler_start(&compiler);

    printf("used memory: %zu\n",arena_top - bumper_arena_start(&arena));
    printf("%d\n",0 % 8);
}