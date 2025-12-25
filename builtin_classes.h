#pragma once

#include "class_linker.h"
#include "jvm.h"

#define NUM_BUILTINS 16
void builtin_classes_init(classlinker_instance_t* linker);