#include "hal/gpio_types.h"
#include "linker.h"
#include "vm.h"
#include "driver/gpio.h"
#include <unistd.h>

void app_main(){
    VM_t jvm = {0};
    VM_init(&jvm);
    printf("LAUNCHED!\n");

    

    gpio_set_direction(16,GPIO_MODE_OUTPUT);
    gpio_set_level(16,1);

    JClass_t* dummy = JClass_get(&jvm.linker,"dummy");
    printf("static %p\n",JClass_get_method(dummy, "dummy_method@()V"));
    printf("instance %p\n",JClass_get_method(dummy, "non_static_dummy@()V"));
    printf("ref %p\n",JClass_get_method(dummy, "non_static_dummy@()V")->methodref);
    printf("code %p\n",JClass_get_method(dummy, "non_static_dummy@()V")->method_info);

    printf("invoke code: %d\n",VM_startup(&jvm,"dummy"));
    //sleep(1);
    gpio_set_level(16,0);
}