#pragma once

#include "jeex.h"
#include "list.h"
#include "vm_thread.h"


typedef enum{
    EVMERR_OK,
    EVMERR_BADPARAM,
    EVMERR_UNKNOWN,
    EVMERR_OOM,
}VMError_t;


typedef struct{
    struct list_head thread_list;
    JEEXHeader_t* jeex_image;

    uint8_t* static_variables;
}VM_t;