#include "../class_linker.h"

extern classlinker_class_t java_lang_Object;
static classlinker_normalclass_t Runnable_info = {
    .methods_count = 1,
    .methods = (classlinker_method_t[]){
        {
            .name = "run",
            .raw_description = "()V",
        },
    }
};

classlinker_class_t java_lang_Runnable = {
    .this_name = "java/lang/Runnable",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &Runnable_info,
};