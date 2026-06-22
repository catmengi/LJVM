#pragma once

#define KB * 1024
#define MB * 1024 * 1024

#define CLASS_PERMAMENT_ARENA 1 MB
#define CLASS_TEMPOPARY_ARENA 256 KB
#define PARSER_ARENA 256 KB
#define STRINGPOOL_ARENA 512 KB

#define THREAD_STACK_SIZE 2 KB
#define THREAD_MAX_COUNT 16
#define THREAD_LOWEST_QUOTA 64
#define THREAD_DEFAULT_PRIORITY 5

#define OBJECT_HEAP_SIZE 2 MB
#define MAX_MONITORS 512

void JEspresso_init();