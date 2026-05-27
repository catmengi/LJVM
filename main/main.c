#include "config.h"
#include "parser.h"
#include "stringpool.h"
#include "class.h"

#include <stdio.h>
#include <assert.h>

extern Class_t* class_convert_from_raw(JRawClass_t* parsed_class);
extern int class_link(Class_t* class);

int main(){
    JEspresso_init();

    FILE* class = fopen("java_src/dummy.class", "rb");
    assert(class);

    JRawClass_t* parsed_class = parse_class(class);
    Class_t* jeclass = class_convert_from_raw(parsed_class);
    assert(jeclass);

    assert(class_link(jeclass) == 0);
}