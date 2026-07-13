#pragma once

#include "list.h"
#include "config.h"
#include "class.h"

typedef struct{
    struct list_head list;
    Class_t* items[CLASSTABLE_ENTRY_ITEMS_COUNT];
}ClasstableEntry_t;

void classtable_init();
Error_t classtable_put(Class_t* class);
Class_t* classtable_get(int32_t name_id);