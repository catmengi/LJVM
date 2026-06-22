#include "class.h"
#include "parser.h"
#include "stringpool.h"
#include "thread.h"
#include "heap.h"
#include "monitor.h"


void JEspresso_init(){
    stringpool_init();
    parser_init();
    monitors_init();
    heap_init();
    classes_init();
    threads_init();
}