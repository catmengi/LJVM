#include "bumper.h"
#include "hal/gpio_types.h"
#include "linker.h"
#include "driver/gpio.h"
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
    printf("used memory: %zu\n",arena_top - bumper_arena_start(&arena));
}