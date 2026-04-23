#include "bumper.h"
#include "cfg.h"
#include "class.h"
#include "compiler.h"
#include "jeex_builder.h"
#include "linker.h"
#include "preloader.h"

#include <string.h>

void app_main(){
    bump_allocator_t arena = {0};
    bumper_create(&arena,2 * 1024 * 1024);

    JLoader_t loader = {0};
    JLoader_init(&loader,&arena);
    JPreloader_load_builtins(&loader);

    JLinker_t linker = {0};
    JLinker_init(&linker,&loader,&arena);
    JLinker_link(&linker);


    //JCompiler_t compiler = {0};
    //JCompiler_init(&compiler,&linker,"/sd/abcd_app");
    //JCompiler_start(&compiler);

    JClass_t* dummy = JClass_get(&linker, "dummy");
    assert(dummy);

    JField_t* b = JClass_get_field(dummy, "b@J");
    JField_t* bi = JClass_get_field(dummy, "bi@I");
    JField_t* c = JClass_get_field(dummy, "c@F");

    JMethod_t* init = JClass_get_method(dummy, "<init>@()V");
    JMethod_t* main = JClass_get_method(dummy, "main@([Ljava/lang/String;)V");
    assert(init && main);
    printf("init: %s\n",init->name);
    printf("main: %s\n", main->name);

    assert(b && bi && c);
    assert(strcmp(b->name, "b@J") == 0);
    assert(strcmp(bi->name, "bi@I") == 0);
    assert(strcmp(c->name, "c@F") == 0);

    printf("b: %p\n",b->constvalue);
    printf("bi: %p\n",bi->constvalue);
    printf("c: %f\n",*(float*)c->constvalue);

    JEEXBuilder_t jex_builder = {0};
    JEEX_create_builder(&jex_builder, &linker, &arena); //Debug only
    assert(JEEX_build(&jex_builder) == JERR_OK);

    printf("used memory: %zu\n",bumper_alloc(&arena,0) - bumper_arena_start(&arena));
}