#define ARENA_IMPLEMENTATION

#include "class_loader.h"
#include "reader.h"

#define CLASSLOADER_ARENA_SIZE 1024 * 1024

void* parse_utf8(classloader_instance_t* instance, file_reader_t* reader){
    void* ret = NULL;

    uint16_t strlength = 0;
    FAIL_SET_JUMP(file_read_int(reader, &strlength, sizeof(strlength)),ret,NULL,exit);

    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1, strlength + 1)),ret,NULL,exit);
    FAIL_SET_JUMP(file_read_bytes(reader,ret,strlength),ret,NULL,exit);

    instance->classes_stats.strliteral_count++;
exit:
    return ret;
}

void* parse_int(classloader_instance_t* instance, file_reader_t* reader){
    void* ret = NULL;

    uint32_t java_int = 0;
    FAIL_SET_JUMP(file_read_int(reader, &java_int, sizeof(java_int)),ret,NULL,exit);

    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(java_int))),ret,NULL,exit);
    *(typeof(java_int)*)ret = java_int;

exit:
    return ret;
}

void* parse_float(classloader_instance_t* instance, file_reader_t* reader){
    void* ret = NULL;

    float java_int = 0;
    FAIL_SET_JUMP(file_read_int(reader, &java_int, sizeof(java_int)),ret,NULL,exit);

    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(java_int))),ret,NULL,exit);
    *(typeof(java_int)*)ret = java_int;

exit:
    return ret;
}

void* parse_long(classloader_instance_t* instance, file_reader_t* reader){
    void* ret = NULL;

    uint64_t java_int = 0;
    FAIL_SET_JUMP(file_read_int(reader, &java_int, sizeof(java_int)),ret,NULL,exit);

    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(java_int))),ret,NULL,exit);
    *(typeof(java_int)*)ret = java_int;

exit:
    return ret;
}

void* parse_double(classloader_instance_t* instance, file_reader_t* reader){
    void* ret = NULL;

    uint64_t java_int = 0;
    FAIL_SET_JUMP(file_read_int(reader, &java_int, sizeof(java_int)),ret,NULL,exit);

    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(java_int))),ret,NULL,exit);
    *(typeof(java_int)*)ret = java_int;

exit:
    return ret;
}

void* parse_class(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_class_t* class = NULL;

    uint16_t name_index = 0;
    FAIL_SET_JUMP(file_read_int(reader, &name_index, sizeof(name_index)),class,NULL,exit);

    FAIL_SET_JUMP((class = arena_calloc(instance->loader_arena,1,sizeof(*class))),class,NULL,exit);

    class->name_index = name_index - 1;
    instance->classes_stats.classes_referenced++;

exit:
    return class;
}

void* parse_string(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_string_t* ret = NULL;

    uint16_t name_index = 0;
    FAIL_SET_JUMP(file_read_int(reader, &name_index, sizeof(name_index)),ret,NULL,exit);
    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(*ret))),ret,NULL,exit);

    ret->content_index = name_index - 1;

exit:
    return ret;
}

void* parse_FMIM_refs(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_fmim_t* ret = NULL;

    uint16_t class_index = 0;
    uint16_t nametype_index = 0;

    FAIL_SET_JUMP(file_read_int(reader, &class_index, sizeof(class_index)),ret,NULL,exit);
    FAIL_SET_JUMP(file_read_int(reader, &nametype_index, sizeof(nametype_index)),ret,NULL,exit);
    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(*ret))),ret,NULL,exit);

    ret->class_index = class_index - 1;
    ret->name_and_type_index = nametype_index - 1;

exit:
    return ret;
}

void* parse_nametype(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_name_and_type_t* ret = NULL; //they are basicly the same so use the first one, LOL

    uint16_t name_index = 0;
    uint16_t descriptor_index = 0;

    FAIL_SET_JUMP(file_read_int(reader, &name_index, sizeof(name_index)),ret,NULL,exit);
    FAIL_SET_JUMP(file_read_int(reader, &descriptor_index, sizeof(descriptor_index)),ret,NULL,exit);
    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(*ret))),ret,NULL,exit);

    ret->name_index = name_index - 1;
    ret->descriptor_index = descriptor_index - 1;

exit:
    return ret;
}

void* parse_methodhandle(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_methodhandle_t* ret = NULL;

    uint8_t reference_kind = 0;
    uint16_t reference_index = 0;

    FAIL_SET_JUMP(file_read_int(reader, &reference_kind, sizeof(reference_kind)),ret,NULL,exit);
    FAIL_SET_JUMP(file_read_int(reader, &reference_index, sizeof(reference_index)),ret,NULL,exit);
    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(*ret))),ret,NULL,exit);

    ret->reference_kind = reference_kind - 1;
    ret->reference_index = reference_index - 1;

exit:
    return ret;
}

void* parse_methodtype(classloader_instance_t* instance, file_reader_t* reader){
    classloader_constant_methodtype_t* ret = NULL;

    uint16_t descriptor_index = 0;
    FAIL_SET_JUMP(file_read_int(reader, &descriptor_index, sizeof(descriptor_index)),ret,NULL,exit);
    FAIL_SET_JUMP((ret = arena_calloc(instance->loader_arena,1,sizeof(*ret))),ret,NULL,exit);

    ret->descriptor_index = descriptor_index - 1;

exit:
    return ret;
}

void* parse_nothing(classloader_instance_t* instance, file_reader_t* reader){
    return NULL;
}

typedef void* (*parse_constant)(classloader_instance_t* instance, file_reader_t* reader);
static parse_constant classloader_constant_parsers[19] = {
    [0] = parse_nothing,
    [1] = parse_utf8,
    [3] = parse_int, 
    [4] = parse_float,
    [5] = parse_long,
    [6] = parse_double,
    [7] = parse_class,
    [8] = parse_string,
    [9] = parse_FMIM_refs,
    [10] = parse_FMIM_refs,
    [11] = parse_FMIM_refs,
    [12] = parse_nametype,
    [15] = parse_methodhandle,
    [16] = parse_methodtype,
    //[18] = parse_invokedynamic 
};

static classloader_error_t attribute_parse(classloader_instance_t* instance, file_reader_t* reader, classloader_attribute_t* output){
    classloader_error_t err = CLASSLOADER_OK;

    FAIL_SET_JUMP(file_read_int(reader,&output->name_index, sizeof(output->name_index)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(file_read_int(reader,&output->length, sizeof(output->length)),err,CLASSLOADER_FILE_ERROR,exit);

    output->name_index--;

    FAIL_SET_JUMP((output->classloader_attribute = arena_alloc(instance->loader_arena,output->length)),err,CLASSLOADER_OOM,exit);

    FAIL_SET_JUMP(file_read_bytes(reader,output->classloader_attribute,output->length),err,CLASSLOADER_FILE_ERROR,exit);

exit:
    return err;
}

static classloader_error_t field_parse(classloader_instance_t* instance, file_reader_t* reader, classloader_field_t* output){
    classloader_error_t err = CLASSLOADER_OK;

    FAIL_SET_JUMP(file_read_int(reader,&output->access_flags, sizeof(output->access_flags)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(file_read_int(reader,&output->name_index, sizeof(output->name_index)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(file_read_int(reader,&output->descriptor_index, sizeof(output->descriptor_index)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(file_read_int(reader,&output->attributes_count, sizeof(output->attributes_count)),err,CLASSLOADER_FILE_ERROR,exit);

    output->name_index--;
    output->descriptor_index--;

    FAIL_SET_JUMP(output->attributes_count == 0 || (output->attributes = arena_calloc(instance->loader_arena,output->attributes_count, sizeof(*output->attributes))),err,CLASSLOADER_OOM,exit);
    for(unsigned i = 0; i < output->attributes_count; i++){
        classloader_error_t attribute_err = attribute_parse(instance, reader, &output->attributes[i]);
        FAIL_SET_JUMP(attribute_err == CLASSLOADER_OK, err, attribute_err, exit);
    }

exit:
    return err;
}

classloader_instance_t* classloader_new(){
    Arena* classloader_arena = arena_new_dynamic(CLASSLOADER_ARENA_SIZE);
    classloader_instance_t* instance = NULL;

    if(classloader_arena){
        instance = arena_alloc(classloader_arena, sizeof(*instance));
        assert(instance);

        INIT_LIST_HEAD(&instance->loaded_classes);
        instance->classes_stats = (typeof(instance->classes_stats)){0};
        instance->loader_arena = classloader_arena;
    }
    return instance;
}

int classloader_load_jar(classloader_instance_t* instance, const char* jar){
}

classloader_error_t classloader_load_class(classloader_instance_t* instance, file_reader_t* reader){
    classloader_error_t  err = CLASSLOADER_OK;
    FAIL_SET_JUMP(instance && reader,err,CLASSLOADER_UNKNOWN,exit);

    uint32_t magic = 0;
    FAIL_SET_JUMP(file_read_int(reader,&magic,sizeof(magic)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP((magic == 0xcafebabe),err,CLASSLOADER_FILE_ERROR,exit);

    uint32_t empty = 0;
    FAIL_SET_JUMP(file_read_int(reader,&empty,sizeof(uint32_t)),err,CLASSLOADER_FILE_ERROR,exit);

    classloader_class_t* java_class = arena_calloc(instance->loader_arena,1,sizeof(*java_class));
    FAIL_SET_JUMP(java_class,err,CLASSLOADER_OOM,exit);

    INIT_LIST_HEAD(&java_class->list);
    list_add(&java_class->list,&instance->loaded_classes);

    FAIL_SET_JUMP(file_read_int(reader,&java_class->constants_count,sizeof(java_class->constants_count)),err,CLASSLOADER_FILE_ERROR,exit);

    FAIL_SET_JUMP((java_class->constants = arena_calloc(instance->loader_arena,java_class->constants_count - 1,
                                        sizeof(*java_class->constants))),err,CLASSLOADER_OOM,exit);
    java_class->constants_count--;
    instance->classes_stats.constants_count_summary += java_class->constants_count;

    for(unsigned i = 0; i < java_class->constants_count; i++){
        classloader_constant_type_t type = 0;
        FAIL_SET_JUMP(file_read_int(reader,&type,sizeof(uint8_t)), err, CLASSLOADER_FILE_ERROR, exit);

        FAIL_SET_JUMP(classloader_constant_parsers[type],err,CLASSLOADER_FILE_ERROR,exit);

        java_class->constants[i].type = type;
        java_class->constants[i].data = classloader_constant_parsers[type](instance,reader);

        switch(type){
            default: break;
            case EJCT_utf8:
                java_class->class_stats.utf8_count++;
                break;

            case EJCT_long:
            case EJCT_double:
                i++;
                break;
        }
    }

    FAIL_SET_JUMP(file_read_int(reader,&java_class->access_flags,sizeof(java_class->access_flags)),err,CLASSLOADER_FILE_ERROR,exit);

    FAIL_SET_JUMP(file_read_int(reader,&java_class->this_class,sizeof(java_class->this_class)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(file_read_int(reader,&java_class->super_class,sizeof(java_class->super_class)),err,CLASSLOADER_FILE_ERROR,exit);

    java_class->this_class -= java_class->this_class > 0 ? 1 : 0;
    java_class->super_class -= java_class->super_class > 0 ? 1 : 0;

    FAIL_SET_JUMP(file_read_int(reader,&java_class->interfaces_count,sizeof(java_class->interfaces_count)),err,CLASSLOADER_FILE_ERROR,exit);

    FAIL_SET_JUMP(java_class->interfaces_count == 0 || (java_class->interfaces = arena_calloc(instance->loader_arena,java_class->interfaces_count,
                                        sizeof(*java_class->interfaces))),err,CLASSLOADER_OOM,exit);
    for(unsigned i = 0; i < java_class->interfaces_count; i++){
        FAIL_SET_JUMP(file_read_int(reader,&java_class->interfaces[i],sizeof(java_class->interfaces[i])), err, CLASSLOADER_FILE_ERROR, exit);
        java_class->interfaces[i] -= java_class->interfaces[i] > 0 ? 1 : 0;
    }

//field parse code
    FAIL_SET_JUMP(file_read_int(reader,&java_class->fields_count,sizeof(java_class->fields_count)),err,CLASSLOADER_FILE_ERROR,exit);

    FAIL_SET_JUMP(java_class->fields_count == 0 || (java_class->fields = arena_calloc(instance->loader_arena,java_class->fields_count,
                                        sizeof(*java_class->fields))),err,CLASSLOADER_OOM,exit);
    for(unsigned i = 0; i < java_class->fields_count; i++){
        classloader_error_t field_err = field_parse(instance,reader,&java_class->fields[i]);
        FAIL_SET_JUMP(field_err == CLASSLOADER_OK, err, field_err, exit);
    }

//method parse is the same, because field and method structures are same (at least in class loading stage)

    FAIL_SET_JUMP(file_read_int(reader,&java_class->methods_count,sizeof(java_class->methods_count)),err,CLASSLOADER_FILE_ERROR,exit);
    FAIL_SET_JUMP(java_class->methods_count == 0 || (java_class->methods = arena_calloc(instance->loader_arena,java_class->methods_count,
                                        sizeof(*java_class->methods))),err,CLASSLOADER_OOM,exit);
    for(unsigned i = 0; i < java_class->methods_count; i++){
        classloader_error_t field_err = field_parse(instance,reader,(classloader_field_t*)&java_class->methods[i]);
        FAIL_SET_JUMP(field_err == CLASSLOADER_OK, err, field_err, exit);
    }

//attribute parse
    FAIL_SET_JUMP(file_read_int(reader,&java_class->attributes,sizeof(java_class->attributes_count)),err,CLASSLOADER_FILE_ERROR,exit);
    for(unsigned i = 0; i < java_class->attributes_count; i++){
        classloader_error_t attribute_err = attribute_parse(instance, reader, &java_class->attributes[i]);
        FAIL_SET_JUMP(attribute_err == CLASSLOADER_OK, err, attribute_err, exit);
    }
    
    instance->classes_stats.classes_loaded++;
exit:
    return err;
}
