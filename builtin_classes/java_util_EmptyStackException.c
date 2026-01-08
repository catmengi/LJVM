#include "../jvm.h"
#include "../class_linker.h"

extern classlinker_class_t java_lang_RuntimeException;

classlinker_normalclass_t java_util_EmptyStackException_info = {
};

classlinker_class_t java_util_EmptyStackException = {
    .this_name = "java/util/EmptyStackException",
    .parent = &java_lang_RuntimeException,
    .generation = 4,
    .info = &java_util_EmptyStackException_info, 
};