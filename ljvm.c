#include "ljvm.h"


JError_t jvm_init(JVM_t* jvm){
    INIT_LIST_HEAD(&jvm->thread_list);
    TODO("Heap");

    return EJERR_OK;
}