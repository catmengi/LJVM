#include "config.h"
#include "parser.h"
#include "stringpool.h"
#include "class.h"

#include <stdio.h>
#include <assert.h>

int main(){
    JEspresso_init();

    Class_t* out = NULL;
    assert(class_load_bynameid(stringpool_add("dummy"), &out) == JERR_OK);

}