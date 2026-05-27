#include "class.h"
#include "parser.h"
#include "stringpool.h"


void JEspresso_init(){
    stringpool_init();
    parser_init();
    classes_init();
}