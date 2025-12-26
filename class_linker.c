#include "class_linker.h"
#include "arena.h"
#include "class_loader.h"
#include "jvm_internal.h"
#include "jvm.h"
#include "lb_endian.h"
#include "list.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "builtin_classes.h"

#define CLASSLINKER_ARENA_SIZE 1024 * 1024

classlinker_instance_t* classlinker_new(){
    Arena* classlinker_arena = arena_new_dynamic(CLASSLINKER_ARENA_SIZE);
    classlinker_instance_t* instance = NULL;

    if(classlinker_arena){
        instance = arena_alloc(classlinker_arena, sizeof(*instance));
        assert(instance);

        INIT_LIST_HEAD(&instance->loaded_classes);

        instance->arena = classlinker_arena;
    }
    return instance;
}

classlinker_method_t* find_jni_method(classlinker_instance_t* linker, char* class_name, char* fn_name, char* fn_description, void** userctx_out){
    for(unsigned i = 0; i < linker->jni_list->fn_count; i++){
        if(strcmp(linker->jni_list->fns[i].class_name,class_name) == 0){
            if(fn_description == NULL || strcmp(linker->jni_list->fns[i].method->raw_description, fn_description) == 0){
                *userctx_out = linker->jni_list->fns[i].method->userctx;
                return linker->jni_list->fns[i].method;
            }
        }
    }
    return NULL;
}

typedef struct{
    char* symbol;
    void* value;
}classlinker_symbol_map_t;

static void* get_in_symmap(classlinker_symbol_map_t* symmap, unsigned symmap_size, char* symbol){
    for(unsigned i = 0; i < symmap_size; i++){
        if(symmap[i].symbol){
            if(strcmp(symmap[i].symbol,symbol) == 0){
                return symmap[i].value;
            }
        }
    }
    return NULL;
}

static bool is_array(const char* name){
    if(name[0] == '[')
        return true;
    return false;
}

typedef struct{
    char* name;
    classlinker_attributetype_t type;
}classlinker_attribute_strtype_table_element_t; //big ass name, this will be used in code like once?

static classlinker_attribute_strtype_table_element_t attribute_strtotype[] = {
    {"ConstantValue",ATTRIBUTE_CONSTVALUE},
    {"Code",ATTRIBUTE_CODE},
};

classlinker_attributetype_t attribute_gettype(char* attribute_name){
    classlinker_attributetype_t ret = ATTRIBUTE_UNKNOWN;

    for(unsigned i = 0; i < sizeof(attribute_strtotype) / sizeof(attribute_strtotype[0]); i++){
        if(strcmp(attribute_name,attribute_strtotype[i].name) == 0){
            ret = attribute_strtotype[i].type;
            break;
        }
    }

    return ret;
}



classlinker_bytecode_t* parse_code(char* attr_data,classlinker_instance_t* linker,classlinker_method_t* method){
    void* fail_set_jump_dummy; //Dummy variable for FAIL_SET_JUMP
    classlinker_bytecode_t* code = arena_calloc(linker->arena,1,sizeof(*code));
    if(code){
        code->stack_size = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);
        code->locals_count = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);
        code->code_length = be32_to_cpu(*(uint32_t*)attr_data); attr_data += sizeof(uint32_t);

        code->code = arena_alloc(linker->arena,code->code_length);
        if(code->code == NULL) return NULL;

        memcpy(code->code,attr_data,code->code_length); attr_data += code->code_length;

        code->exceptiontable_size = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);

        code->exception_table = arena_calloc(linker->arena,code->exceptiontable_size,sizeof(*code->exception_table));
        if(code->exception_table || code->exceptiontable_size == 0){
            for(unsigned i = 0; i < code->exceptiontable_size; i++){
                code->exception_table[i].start_pc = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);
                code->exception_table[i].end_pc = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);
                code->exception_table[i].handler_pc = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);

                uint16_t catch_index = be16_to_cpu(*(uint16_t*)attr_data); attr_data += sizeof(uint16_t);
                classlinker_normalclass_t* class_info = method->class->info;

                if(catch_index > 0){
                    code->exception_table[i].exception_class = class_info->constant_pool.constants[catch_index - 1].constant_value;
                } else code->exception_table[i].exception_class = NULL;
            }
        }

    }

    return code;
}

void parse_description(classlinker_methoddescription_t* output, char* description, classlinker_instance_t* linker) {
    // Initialize output
    output->arguments_count = 0;
    output->args = NULL;
    output->return_type = '\0';
    
    // Skip the opening '('
    description++;
    
    // First pass: count arguments
    char* ptr = description;
    while (*ptr != ')') {
        switch (*ptr) {
            case 'B': // byte
            case 'C': // char
            case 'D': // double
            case 'F': // float
            case 'I': // int
            case 'J': // long
            case 'S': // short
            case 'Z': // boolean
                output->arguments_count++;
                ptr++;
                break;
                
            case 'L': // object type
                output->arguments_count++;
                // Skip to the next ';'
                while (*ptr != ';' && *ptr != '\0') {
                    ptr++;
                }
                if (*ptr == ';') ptr++;
                break;
                
            case '[': // array type
                output->arguments_count++;
                // Skip array dimensions
                while (*ptr == '[') {
                    ptr++;
                }
                // Handle the array element type
                if (*ptr == 'L') {
                    while (*ptr != ';' && *ptr != '\0') {
                        ptr++;
                    }
                    if (*ptr == ';') ptr++;
                } else {
                    ptr++;
                }
                break;
                
            default:
                // Invalid descriptor
                ptr++;
                break;
        }
    }
    
    // Allocate memory for arguments
    if (output->arguments_count > 0) {
        output->args = arena_calloc(linker->arena, output->arguments_count, sizeof(char*));
    }
    
    // Second pass: extract argument types
    ptr = description;
    unsigned arg_index = 0;
    while (*ptr != ')') {
        switch (*ptr) {
            case 'B': // byte
                output->args[arg_index++] = arena_strdup(linker->arena, "B");
                ptr++;
                break;
                
            case 'C': // char
                output->args[arg_index++] = arena_strdup(linker->arena, "C");
                ptr++;
                break;
                
            case 'D': // double
                output->args[arg_index++] = arena_strdup(linker->arena, "D");
                ptr++;
                break;
                
            case 'F': // float
                output->args[arg_index++] = arena_strdup(linker->arena, "F");
                ptr++;
                break;
                
            case 'I': // int
                output->args[arg_index++] = arena_strdup(linker->arena, "I");
                ptr++;
                break;
                
            case 'J': // long
                output->args[arg_index++] = arena_strdup(linker->arena, "J");
                ptr++;
                break;
                
            case 'S': // short
                output->args[arg_index++] = arena_strdup(linker->arena, "S");
                ptr++;
                break;
                
            case 'Z': // boolean
                output->args[arg_index++] = arena_strdup(linker->arena, "Z");
                ptr++;
                break;
                
            case 'L': // object type
            {
                char* start = ptr + 1;
                while (*ptr != ';' && *ptr != '\0') {
                    ptr++;
                }
                if (*ptr == ';') {
                    size_t length = ptr - start;
                    char* type_name = arena_alloc(linker->arena, length + 1);
                    strncpy(type_name, start, length);
                    type_name[length] = '\0';
                    output->args[arg_index++] = type_name;
                    ptr++;
                }
                break;
            }
            
            case '[': // array type
            {
                char* array_start = ptr;
                // Count array dimensions
                int dimensions = 0;
                while (*ptr == '[') {
                    dimensions++;
                    ptr++;
                }
                
                // Get the base type
                char base_type[2] = {0};
                if (*ptr == 'L') {
                    char* start = ptr + 1;
                    while (*ptr != ';' && *ptr != '\0') {
                        ptr++;
                    }
                    if (*ptr == ';') {
                        size_t length = ptr - start;
                        char* type_name = arena_alloc(linker->arena, length + dimensions + 2);
                        // Create array descriptor with dimensions
                        for (int i = 0; i < dimensions; i++) {
                            type_name[i] = '[';
                        }
                        strncpy(type_name + dimensions, start, length);
                        type_name[dimensions + length] = '\0';
                        output->args[arg_index++] = type_name;
                        ptr++;
                    }
                } else {
                    switch (*ptr) {
                        case 'B': base_type[0] = 'B'; break;
                        case 'C': base_type[0] = 'C'; break;
                        case 'D': base_type[0] = 'D'; break;
                        case 'F': base_type[0] = 'F'; break;
                        case 'I': base_type[0] = 'I'; break;
                        case 'J': base_type[0] = 'J'; break;
                        case 'S': base_type[0] = 'S'; break;
                        case 'Z': base_type[0] = 'Z'; break;
                    }
                    if (base_type[0] != '\0') {
                        char* array_type = arena_alloc(linker->arena, dimensions + 2);
                        for (int i = 0; i < dimensions; i++) {
                            array_type[i] = '[';
                        }
                        array_type[dimensions] = base_type[0];
                        array_type[dimensions + 1] = '\0';
                        output->args[arg_index++] = array_type;
                        ptr++;
                    }
                }
                break;
            }
            
            default:
                ptr++;
                break;
        }
    }
    
    // Skip the closing ')'
    ptr++;
    
    // Parse return type
    if (*ptr == 'V') {
        output->return_type = 'V'; // void
    } else {
        // For non-void return types, we need to handle them properly
        // For simplicity, we'll just store the first character
        // You might want to expand this to handle object and array return types
        output->return_type = *ptr;
    }
}



classlinker_error_t classlinker_link(classlinker_instance_t* linker, classloader_instance_t* loader){
    builtin_classes_init(linker);

    classlinker_error_t err = CLASSLINKER_OK;

    unsigned symmap_size = loader->classes_stats.classes_referenced + NUM_BUILTINS;
    classlinker_symbol_map_t symbol_map[symmap_size];
    memset(&symbol_map,0,symmap_size * sizeof(symbol_map[0]));

    classlinker_class_t* linkable_class = NULL;
    unsigned symmap_i = 0;
    //Step -1: adding preloaded classes
    list_for_each_entry(linkable_class,&linker->loaded_classes,list){
        unsigned csymmap_i = symmap_i++;

        symbol_map[csymmap_i].value = linkable_class;
        symbol_map[csymmap_i].symbol = linkable_class->this_name;
    }

    //Step 0: creating and loading NON array classes
    classloader_class_t* class_inlink = NULL;
    list_for_each_entry(class_inlink,&loader->loaded_classes,list){
        //Creating ========
        classlinker_class_t* new_class = arena_calloc(linker->arena,1,sizeof(*new_class));
        FAIL_SET_JUMP(new_class,err,CLASSLINKER_OOM,exit);

        INIT_LIST_HEAD(&new_class->list);
        list_add_tail(&new_class->list,&linker->loaded_classes);
        //=================

        //Getting super and this names ============
        FAIL_SET_JUMP(class_inlink->constants[class_inlink->this_class].type == EJCT_class, err, CLASSLINKER_INVALID_CLASS,exit);
        FAIL_SET_JUMP(class_inlink->constants[class_inlink->super_class].type == EJCT_class, err, CLASSLINKER_INVALID_CLASS,exit);

        classloader_constant_class_t* this_lclass = class_inlink->constants[class_inlink->this_class].data;
        classloader_constant_class_t* super_lclass = class_inlink->constants[class_inlink->super_class].data;
        
        char* this_name = class_inlink->constants[this_lclass->name_index].data;
        char* super_name = class_inlink->constants[super_lclass->name_index].data;

        new_class->parent_name = super_name;
        new_class->this_name = arena_strdup(linker->arena,this_name);
        new_class->raw_class = class_inlink;

        unsigned csymmap_i = symmap_i++;

        symbol_map[csymmap_i].symbol = new_class->this_name;
        symbol_map[csymmap_i].value = new_class;
        //========================================
    }

    //Step 1: parent searching for executable classes
    list_for_each_entry(linkable_class, &linker->loaded_classes, list){
        if(linkable_class->parent == NULL && linkable_class->parent_name){ //Эх, сиротка :(
            linkable_class->parent = get_in_symmap(symbol_map, symmap_size, linkable_class->parent_name);
        }
        linkable_class->type = is_array(linkable_class->this_name);

        if(linkable_class->type == EClass && linkable_class->raw_class){
            linkable_class->info = arena_calloc(linker->arena,1,sizeof(classlinker_normalclass_t));
            FAIL_SET_JUMP(linkable_class->info,err,CLASSLINKER_OOM,exit);

            classlinker_normalclass_t* class_info = linkable_class->info;
            class_info->constant_pool.constants_count = linkable_class->raw_class->constants_count;
            class_info->constant_pool.constants = arena_calloc(linker->arena,class_info->constant_pool.constants_count, sizeof(*class_info->constant_pool.constants));
            FAIL_SET_JUMP(class_info->constant_pool.constants,err,CLASSLINKER_OOM,exit);
        }
    }


    //Step 2: resolve constant pool references
    list_for_each_entry(linkable_class, &linker->loaded_classes, list){
        if(linkable_class->type == EClass && linkable_class->raw_class){
            classloader_class_t* raw_class = linkable_class->raw_class;
            classlinker_normalclass_t* class_info = linkable_class->info;

            for(unsigned i = 0; i < raw_class->constants_count; i++){
                class_info->constant_pool.constants[i].constant_type = raw_class->constants[i].type;
                FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_type != EJCT_invokedynamic, err, CLASSLINKER_UNSUPPORTED, exit);
                FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_type != EJCT_methodhandle, err, CLASSLINKER_UNSUPPORTED, exit);
                FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_type != EJCT_methodtype, err, CLASSLINKER_UNSUPPORTED, exit);

                if(class_info->constant_pool.constants[i].constant_type == EJCT_class){
                    classloader_constant_class_t* constant_class = raw_class->constants[i].data;

                    char* find_name = raw_class->constants[constant_class->name_index].data;
                    if(is_array(find_name) == EClass){
                        classlinker_class_t* found = get_in_symmap(symbol_map, symmap_size, find_name);
                        FAIL_SET_JUMP(found,err,CLASSLINKER_NOTFOUND,exit);
                        found->generation = found->parent ? found->parent->generation + 1 : 0;

                        class_info->constant_pool.constants[i].constant_value = found;
                    } else {
                        classlinker_class_t* array = arena_calloc(linker->arena,1,sizeof(*array));
                        FAIL_SET_JUMP(array,err,CLASSLINKER_OOM,exit);

                        array->type = Earray;
                        array->this_name = arena_strdup(linker->arena,find_name);
                        FAIL_SET_JUMP(array->this_name,err,CLASSLINKER_OOM,exit);

                        array->parent_name = raw_class->constants[raw_class->super_class].data;
                        array->parent = get_in_symmap(symbol_map, symmap_size, array->parent_name);

                        array->generation = array->parent ? array->parent->generation + 1 : 0;
                        class_info->constant_pool.constants[i].constant_value = array;
                    }
                }
                if(class_info->constant_pool.constants[i].constant_type == EJCT_methodref 
                        || class_info->constant_pool.constants[i].constant_type == EJCT_fieldref
                        || class_info->constant_pool.constants[i].constant_type == EJCT_interfacemethodref){
                    classloader_constant_fmim_t* ref = raw_class->constants[i].data;

                    classlinker_fmimref_t* new_ref = arena_calloc(linker->arena,1,sizeof(*new_ref));
                    FAIL_SET_JUMP(new_ref, err, CLASSLINKER_OOM, exit);
                
                    classloader_constant_class_t* class = raw_class->constants[ref->class_index].data;
                    char* class_name = raw_class->constants[class->name_index].data;

                    new_ref->class = get_in_symmap(symbol_map, symmap_size, class_name);
                    FAIL_SET_JUMP(new_ref->class,err,CLASSLINKER_NOTFOUND,exit);

                    classloader_constant_name_and_type_t* nameandtype = raw_class->constants[ref->name_and_type_index].data;
                    new_ref->nameandtype.name = arena_strdup(linker->arena,raw_class->constants[nameandtype->name_index].data);
                    new_ref->nameandtype.descriptor = arena_strdup(linker->arena,raw_class->constants[nameandtype->descriptor_index].data);

                    class_info->constant_pool.constants[i].constant_value = new_ref;
                }

                if(class_info->constant_pool.constants[i].constant_type == EJCT_int){
                    class_info->constant_pool.constants[i].constant_value = arena_alloc(linker->arena,sizeof(uint32_t));
                    FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_value,err,CLASSLINKER_OOM,exit);

                    memcpy(class_info->constant_pool.constants[i].constant_value,raw_class->constants[i].data,sizeof(uint32_t));
                }
                if(class_info->constant_pool.constants[i].constant_type == EJCT_float){
                    class_info->constant_pool.constants[i].constant_value = arena_alloc(linker->arena,sizeof(uint32_t));
                    FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_value,err,CLASSLINKER_OOM,exit);

                    memcpy(class_info->constant_pool.constants[i].constant_value,raw_class->constants[i].data,sizeof(uint32_t));
                }
                if(class_info->constant_pool.constants[i].constant_type == EJCT_long){
                    class_info->constant_pool.constants[i].constant_value = arena_alloc(linker->arena,sizeof(uint64_t));
                    FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_value,err,CLASSLINKER_OOM,exit);

                    memcpy(class_info->constant_pool.constants[i].constant_value,raw_class->constants[i].data,sizeof(uint64_t));
                }
                if(class_info->constant_pool.constants[i].constant_type == EJCT_double){
                    class_info->constant_pool.constants[i].constant_value = arena_alloc(linker->arena,sizeof(uint64_t));
                    FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_value,err,CLASSLINKER_OOM,exit);

                    memcpy(class_info->constant_pool.constants[i].constant_value,raw_class->constants[i].data,sizeof(uint64_t));
                }

                if(class_info->constant_pool.constants[i].constant_type == EJCT_string){
                    uint16_t index = *(uint16_t*)raw_class->constants[i].data;

                    class_info->constant_pool.constants[i].constant_type = EJCT_unitialised_string;

                    class_info->constant_pool.constants[i].constant_value = arena_strdup(linker->arena,raw_class->constants[index].data);
                    FAIL_SET_JUMP(class_info->constant_pool.constants[i].constant_value,err,CLASSLINKER_OOM,exit);
                }

            }
        }
    }
    
    //Step 3: Parse fields
    list_for_each_entry(linkable_class, &linker->loaded_classes, list){
        if(linkable_class->type == EClass && linkable_class->raw_class){
            classloader_class_t* raw_class = linkable_class->raw_class;
            classlinker_normalclass_t* class_info = linkable_class->info;
            
            size_t static_fields = 0;
            size_t fields = 0;
            for(unsigned i = 0; i < raw_class->fields_count; i++){
                if((raw_class->fields[i].access_flags & ACC_STATIC) == ACC_STATIC)
                    static_fields++;
                else fields++;
            }

            class_info->static_fields_count = static_fields;
            class_info->fields_count = fields;

            class_info->static_fields = arena_calloc(linker->arena,
                                                     class_info->static_fields_count,
                                                     sizeof(*class_info->static_fields));
            class_info->fields = arena_calloc(linker->arena,
                                              class_info->fields_count,
                                              sizeof(*class_info->fields));
            FAIL_SET_JUMP(class_info->static_fields || class_info->static_fields_count == 0,
                          err,CLASSLINKER_OOM,exit);

            FAIL_SET_JUMP(class_info->fields || class_info->fields_count == 0,
                          err,CLASSLINKER_OOM,exit);

            size_t sfield_index = 0;
            size_t field_index = 0;

            for(unsigned i = 0; i < raw_class->fields_count; i++){
                classlinker_field_t* new_field = (raw_class->fields[i].access_flags & ACC_STATIC) == ACC_STATIC ? &class_info->static_fields[sfield_index++] : &class_info->fields[field_index++];

                classloader_field_t* loader_field = &raw_class->fields[i];

                new_field->flags = loader_field->access_flags;
                new_field->value.type = ((char*)raw_class->constants[loader_field->descriptor_index].data)[0];
                new_field->name = arena_strdup(linker->arena,raw_class->constants[loader_field->name_index].data);

                FAIL_SET_JUMP(new_field->name,err,CLASSLINKER_OOM,exit);


                for(unsigned attr = 0; attr < loader_field->attributes_count; attr++){
                    classloader_attribute_t* field_attr = &loader_field->attributes[attr];
                    char* attr_name = raw_class->constants[field_attr->name_index].data;
                    classlinker_attributetype_t attr_type = attribute_gettype(attr_name);

                    switch(attr_type){
                        default:
                            printf("%s : Unknown attribute '%s'\n",__PRETTY_FUNCTION__,attr_name);
                            break;

                        case ATTRIBUTE_CONSTVALUE:{
                                uint16_t constantvalue_index = be16_to_cpu(*(uint16_t*)field_attr->classloader_attribute);
                                classlinker_constant_t* constantvalue = &class_info->constant_pool.constants[constantvalue_index];

                                switch(constantvalue->constant_type){
                                    case EJCT_int:
                                        new_field->value.type = EJVT_INT;
                                        *(uint32_t*)new_field->value.value = *(uint32_t*)constantvalue->constant_value;
                                        break;
                                    case EJCT_float:
                                        new_field->value.type = EJVT_FLOAT;
                                        *(float*)new_field->value.value = *(float*)constantvalue->constant_value;
                                        break;
                                    case EJCT_long:
                                        new_field->value.type = EJVT_LONG;
                                        *(uint32_t*)new_field->value.value = *(uint64_t*)constantvalue->constant_value;
                                        break;
                                    case EJCT_double:
                                        new_field->value.type = EJVT_DOUBLE;
                                        *(double*)new_field->value.value = *(double*)constantvalue->constant_value;
                                        break;
                                    case EJCT_string:
                                        new_field->value.type = EJVT_REFERENCE;
                                        *(void**)new_field->value.value = constantvalue->constant_value;
                                        break;

                                    default: break;
                                }
                            }
                            break;
                    }
                }
            }
        }
    }

    //Step 4 parse methods
    list_for_each_entry(linkable_class, &linker->loaded_classes, list){
        if(linkable_class->type == EClass && linkable_class->raw_class){
            classlinker_normalclass_t* class_info = linkable_class->info;
            classloader_class_t* raw_class = linkable_class->raw_class;

            class_info->methods_count = raw_class->methods_count;

            class_info->methods = arena_calloc(linker->arena,class_info->methods_count,sizeof(*class_info->methods));
            FAIL_SET_JUMP(class_info->methods,err,CLASSLINKER_OOM,exit);

            for(unsigned i = 0; i < class_info->methods_count; i++){
                classloader_method_t* raw_method = &raw_class->methods[i];
                classlinker_method_t* new_method = &class_info->methods[i];

                char* name = raw_class->constants[raw_method->name_index].data;
                char* description = raw_class->constants[raw_method->descriptor_index].data;

                new_method->name = arena_strdup(linker->arena,name);
                new_method->raw_description = arena_strdup(linker->arena, description);
                new_method->not_builtin = true;

                parse_description(&new_method->description,description, linker);

                FAIL_SET_JUMP(new_method->name, err, CLASSLINKER_OOM,exit);

                new_method->flags = raw_method->access_flags;
                new_method->class = linkable_class;

                for(unsigned attr = 0; attr < raw_method->attributes_count; attr++){
                    classlinker_attributetype_t method_attr = attribute_gettype(raw_class->constants[raw_method->attributes[attr].name_index].data);
                    
                    switch(method_attr){
                        default: break;

                        case ATTRIBUTE_CODE:{
                            FAIL_SET_JUMP(!((new_method->flags & ACC_NATIVE) == ACC_NATIVE),err,CLASSLINKER_INVALID_CLASS,exit);
                            FAIL_SET_JUMP(!((new_method->flags & ACC_ABSTRACT) == ACC_ABSTRACT),err,CLASSLINKER_INVALID_CLASS,exit);

                            new_method->userctx = parse_code(raw_method->attributes[attr].classloader_attribute,linker,new_method);
                            FAIL_SET_JUMP(new_method->userctx,err,CLASSLINKER_INVALID_CLASS,exit);

                            new_method->fn = jvm_bytecode_executor;

                            classlinker_bytecode_t* bytecode = new_method->userctx;

                            new_method->frame_descriptor.locals_count = bytecode->locals_count;
                            new_method->frame_descriptor.stack_size = bytecode->stack_size;
                            
                            int arguments_count = new_method->description.arguments_count;
                            FAIL_SET_JUMP(!(arguments_count < 0),err,CLASSLINKER_INVALID_CLASS,exit);

                            new_method->frame_descriptor.arguments_count = arguments_count;
                        }
                        break;
                    }
                }
                if((new_method->flags & ACC_NATIVE) == ACC_NATIVE){
                    classlinker_method_t* found_method = find_jni_method(linker,linkable_class->this_name,new_method->name,new_method->raw_description,&new_method->userctx);
                    FAIL_SET_JUMP(found_method,err,CLASSLINKER_NOTFOUND,exit);
                    FAIL_SET_JUMP(found_method->fn,err,CLASSLINKER_NOTFOUND,exit);                

                    *new_method = *found_method;
                    new_method->class = linkable_class;
                }

                if(new_method->frame_descriptor.locals_count == 0){
                    new_method->frame_descriptor.locals_count = new_method->frame_descriptor.arguments_count; //This is for thoose who lazy to add locals count
                            
                }
                if((new_method->flags & ACC_STATIC) != ACC_STATIC){
                        new_method->frame_descriptor.locals_count++;
                }      

            }
        } else if(linkable_class->type == EClass && linkable_class->raw_class == NULL){ //Allow builtin classes to use native (maybe to load jnis from library?)
            classlinker_normalclass_t* class_info = linkable_class->info;
            for(unsigned i = 0; i < class_info->methods_count; i++){
                classlinker_method_t* new_method = &class_info->methods[i];

                if((new_method->flags & ACC_NATIVE) == ACC_NATIVE){
                    classlinker_method_t* found_method = find_jni_method(linker,linkable_class->this_name,new_method->name,new_method->raw_description,&new_method->userctx);
                    FAIL_SET_JUMP(found_method,err,CLASSLINKER_NOTFOUND,exit);
                    FAIL_SET_JUMP(found_method->fn,err,CLASSLINKER_NOTFOUND,exit);

                    *new_method = *found_method;
                }                

                new_method->class = linkable_class;
                if(new_method->frame_descriptor.locals_count == 0){
                    new_method->frame_descriptor.locals_count = new_method->frame_descriptor.arguments_count; //This is for thoose who lazy to add locals count
                }                
                if((new_method->flags & ACC_STATIC) != ACC_STATIC){
                        new_method->frame_descriptor.locals_count++;
                }
            }
        }
    }

    //Step 5: interface resolution
    list_for_each_entry(linkable_class, &linker->loaded_classes, list){
        if(linkable_class->type == EClass && linkable_class->raw_class){
            classloader_class_t* raw_class = linkable_class->raw_class;
            classlinker_normalclass_t* class_info = linkable_class->info;
            
            linkable_class->implements_count = raw_class->interfaces_count;
            linkable_class->implements = arena_calloc(linker->arena,linkable_class->implements_count,sizeof(*linkable_class->implements));
            FAIL_SET_JUMP(linkable_class->implements_count == 0 || linkable_class->implements, err, CLASSLINKER_OOM,exit);

            for(unsigned i = 0; i < linkable_class->implements_count; i++){
                uint16_t class_index = raw_class->interfaces[i];
                if(class_index != 0){
                    linkable_class->implements[i] = class_info->constant_pool.constants[class_index].constant_value;
                    FAIL_SET_JUMP(linkable_class->implements[i],err,CLASSLINKER_UNKNOWN,exit);

                    printf("'%s' implements '%s'\n",linkable_class->this_name,linkable_class->implements[i]->this_name);
                }else printf("%s: class %s have interface pointing to 0\n",__PRETTY_FUNCTION__,linkable_class->this_name);
            }
        }
    }

    //Step 6: initialising monitors
    list_for_each_entry(linkable_class, &linker->loaded_classes,list){
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&linkable_class->monitor,&attr);
    }
exit:
    return err;
}

classlinker_class_t* classlinker_find_class(classlinker_instance_t* linker, char* name){
    classlinker_class_t* cur = NULL;
    list_for_each_entry(cur, &linker->loaded_classes, list){
        if(strcmp(cur->this_name,name) == 0)
            return cur;
    }
    return NULL;
}

classlinker_method_t* classlinker_find_method(jvm_frame_t* frame, classlinker_class_t* class, char* name, char* description){
    for(classlinker_class_t* cur = class; cur ;cur = cur->parent){
        if(cur->type == EClass){
            classlinker_normalclass_t* class_info = cur->info;
            for(unsigned i = 0; i < class_info->methods_count; i++){
                if(strcmp(name,class_info->methods[i].name) == 0){
                    if(description == NULL || strcmp(description,class_info->methods[i].raw_description) == 0){

                        if((class_info->methods[i].flags & ACC_PRIVATE) == ACC_PRIVATE){
                            if(frame && frame->method && cur == frame->method->class)
                                return &class_info->methods[i];
                            else continue;
                        } else return &class_info->methods[i];;
                    }
                }
            }
        }
    }
    return NULL;
}

classlinker_field_t* classlinker_find_staticfield(jvm_frame_t* frame, classlinker_class_t* class, char* field_name){
    if(class != NULL){
        for(classlinker_class_t* cur = class; cur; cur = cur->parent){
        classlinker_normalclass_t* class_info = class->info;
            for(unsigned i = 0; i < class_info->static_fields_count; i++){
                if(strcmp(field_name, class_info->static_fields[i].name) == 0){

                    if((class_info->static_fields[i].flags & ACC_PRIVATE) == ACC_PRIVATE){
                        if(frame && frame->method && cur == frame->method->class)
                            return &class_info->static_fields[i];
                        else continue;
                    } else return &class_info->static_fields[i];
                }
            }
        }
    }
    return NULL;
}

bool classlinker_is_classes_compatible(classlinker_class_t* class, classlinker_class_t* compatible_to){
    for(classlinker_class_t* cur = class; cur; cur = cur->parent){
        if(compatible_to == cur)
            return true;

        for(unsigned i = 0; i < cur->implements_count; i++){
            if(classlinker_is_classes_compatible(cur->implements[i], compatible_to))
                return true;
        }
    }
    return false;
}
