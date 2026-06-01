#include "config.h"
#include "stringpool.h"
#include "class.h"

#include <assert.h>
#include "loader.h"

int main(){
    JEspresso_init();

    loader_set_loadpath("java_src");

    Class_t* out = NULL;
    assert(class_load_bynameid(stringpool_add("dummy"), &out) == JERR_OK);

}