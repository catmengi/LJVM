#include "class.h"
#include "parser.h"
#include "stringpool.h"
#include "thread.h"


void JEspresso_init(){
    stringpool_init();
    parser_init();
    classes_init();
    threads_init();
}