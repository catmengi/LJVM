#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "stream.h"
#include "loader.h"
#include "builtin_classes.h"


static char* system_prefixes[] = {"java/"};
static BuiltinClassEntry_t* builtin_classes[] = {
    &java_lang_Object,
};

static char s_path[256] = {0};
static char s_app_classpath[256] = "java_src";
static memstream_t s_memstream = {0};

static bool is_system_class(char* class_name){

    for(unsigned i = 0; i < sizeof(system_prefixes) / sizeof(system_prefixes[0]); i++){
        if(strncmp(class_name, system_prefixes[i], strlen(system_prefixes[i])) == 0) return true;
    }

    return false;
}

static int init_classtream(char* class_name, ClassStream_t* classstream){
    if(is_system_class(class_name)){
        for(unsigned i = 0; i < sizeof(builtin_classes) / sizeof(builtin_classes[0]); i++){
            BuiltinClassEntry_t* builtin = builtin_classes[i];
            if(strcmp(builtin->name, class_name) == 0){
                memstream_init(&s_memstream, builtin->classfile, builtin->classfile_len);
                return classstream_init_memstream(classstream, &s_memstream);
            }
        }
    } else {
        memset(s_path, 0, sizeof(s_path));
        snprintf(s_path, sizeof(s_path), "%s/%s.class", s_app_classpath, class_name);

        return classstream_init_file(classstream, fopen(s_path, "rb"));
    }
}

JRawClass_t* loader_load_class(char* name){
    ClassStream_t stream = {0};
    if(init_classtream(name, &stream)) return NULL;

    return parser_parse_class(&stream);
}

