#pragma once
#include "linker.h"
#include "list.h"

enum{
    EJERR_SYSTEM_EXIT = -2,
    EJERR_UNKNOWN = -1,
    EJERR_OK = 0,
    EJERR_OOM,
    EJERR_SYSTEM_OOM,  //whole system is fucked up
    EJERR_NOT_FOUND,
    EJERR_INVALID_CLASS,
    EJERR_INVALID_FRAME_STATE,
    EJERR_ARITHMETIC
};

typedef struct JHeap_t JHeap_t;
typedef struct JVM_t{
    struct list_head thread_list;
    JHeap_t* object_heap;
    linker_t* linker;
}JVM_t;

JError_t jvm_init(JVM_t* jvm);