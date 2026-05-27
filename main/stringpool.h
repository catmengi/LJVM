#pragma once

#include <stdint.h>

#define STRINGPOOL_SIZE 32768

void stringpool_init();
int stringpool_add(char* string);
char* stringpool_get(int index);