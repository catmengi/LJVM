#pragma once

#include "vm_frame.h"
#include "list.h"
#include "freertos/freeRTOS.h"
#include "freertos/task.h"

typedef struct{
    struct list_head list;
    struct list_head wait_list;

    VMCallStack_t call_stack;
    TaskHandle_t task_handle;
}VMThread_t;

extern __thread VMThread_t* VMCurrentThread_t;