#include "linker.h"
#include "bumper.h"
#include "bytecode.h"
#include "class.h"
#include "class_loader.h"
#include "fht.h"
#include "list.h"
#include "ljvm.h"
#include "lb_endian.h"
#include <assert.h>
#include <stdint.h>

#define LINKER_ARENA_SIZE 1 * 1024 * 1024

static unsigned JValue_sizes[] = { //Minimum value size is uint32_t is because of alligment
    [EJVT_BYTE - EJVT_BYTE] = sizeof(uint32_t),
    [EJVT_CHAR - EJVT_BYTE] = sizeof(uint32_t), //java char should be uint16_t
    [EJVT_DOUBLE - EJVT_BYTE] = sizeof(double),
    [EJVT_FLOAT - EJVT_BYTE] = sizeof(float),
    [EJVT_INT - EJVT_BYTE] = sizeof(uint32_t),
    [EJVT_LONG - EJVT_BYTE] = sizeof(uint64_t),
    [EJVT_REFERENCE - EJVT_BYTE] = sizeof(void*),
    [EJVT_SHORT - EJVT_BYTE] = sizeof(uint32_t),
    [EJVT_BOOL - EJVT_BYTE] = sizeof(uint32_t),
    [EJVT_VOID - EJVT_BYTE] = 0,
};

unsigned JValue_sizeof(JValue_type_t type){
    return JValue_sizes[type - EJVT_BYTE];
}

int linker_init(linker_t* linker, classloader_instance_t* loader){
    if(bumper_create(&linker->arena,LINKER_ARENA_SIZE) == 0){
        INIT_LIST_HEAD(&linker->classes);
        linker->loader = loader;
        return 0;
    }
    return 1;
}

JClass_t* class_find(linker_t* linker, char* name){
    JClass_t* cur = NULL;
    list_for_each_entry(cur,&linker->classes,list){
        if(strcmp(cur->name, name) == 0){
            return cur;
        }
    }
    return NULL;
}

//TODO: move to separate file
/*extern*/ JClass_t* builtin_classes[2] = {
    &(JClass_t){
        .name = "java/lang/Object",
    },
    NULL,
};

unsigned linker_load_builtins(linker_t* linker){
    unsigned loaded_count = 0;
    for(unsigned i = 0; builtin_classes[i]; i++,loaded_count++){
        JClass_t* class_copy = bumper_alloc(&linker->arena,sizeof(*class_copy));
        memcpy(class_copy,builtin_classes[i],sizeof(*class_copy));

        class_copy->flags.top_level = 1;

        INIT_LIST_HEAD(&class_copy->list);
        list_add(&class_copy->list,&linker->classes);
    }

    JClass_t* cur = NULL;
    list_for_each_entry(cur,&linker->classes,list){
        cur->parent = cur->parent ? class_find(linker,cur->parent->name) : NULL;
        if(cur->parent) cur->parent->flags.top_level = 0;
    }

    return loaded_count;
}

typedef struct{
    classloader_class_t* raw_class;
    char* parent_name;

    fht_t* field_constvals;
}link_metadata_t;

static inline void field_set(JField_t* field, uint8_t* location, void* to_set){
    memcpy(&location[field->offset],to_set,JValue_sizeof(field->type));
}

static inline void field_get(JField_t* field, uint8_t* location, void* output){
    memcpy(output,&location[field->offset],JValue_sizeof(field->type));
}

static JError_t parse_bytecode(uint8_t* raw_bytecode, JMethod_t* method, bump_allocator_t* allocator){
    JError_t err = EJERR_OK;

    JBytecode_t* bytecode = bumper_alloc(allocator,sizeof(*bytecode));
    FAIL_SET_JUMP(bytecode,err,EJERR_OOM,exit);

    method->frame_info.stack_size = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);
    method->frame_info.locals_count = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);
    bytecode->code_length = be32_to_cpu(*(uint32_t*)raw_bytecode); raw_bytecode += sizeof(uint32_t);

    bytecode->code = bumper_alloc(allocator,bytecode->code_length);
    FAIL_SET_JUMP(bytecode->code,err,EJERR_OOM,exit);

    memcpy(bytecode->code,raw_bytecode,bytecode->code_length); raw_bytecode += bytecode->code_length;
    bytecode->exception_table_size = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);

    bytecode->exception_table = bumper_calloc(allocator,bytecode->exception_table_size,sizeof(*bytecode->exception_table));
    FAIL_SET_JUMP(bytecode->exception_table,err,EJERR_OOM,exit);

    for(unsigned i = 0; i < bytecode->exception_table_size; i++){
        bytecode->exception_table[i].start_pc = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);
        bytecode->exception_table[i].end_pc = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);
        bytecode->exception_table[i].handler_pc = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t); 
        
        uint16_t catcher_index = be16_to_cpu(*(uint16_t*)raw_bytecode); raw_bytecode += sizeof(uint16_t);
        bytecode->exception_table[i].catches = catcher_index == 0 ? NULL : method->owner->info->constant_pool.constants[catcher_index - 1].value;
    }
    method->userctx = bytecode;
    method->method = DEFAULT_INTERPRETER;

exit:
    return err;
}

static inline int get_argument_count_from_descriptor(const char* descriptor){
    if (!descriptor || descriptor[0] != '(') return -1;
    int arg_count = 0;
    const char* p = descriptor + 1;
    while (*p != '\0' && *p != ')') {
        arg_count++;
        while (*p == '[') {
            p++;
            if (*p == '\0' || *p == ')') return -1;
        }
        if (*p == 'L') {
            while (*p != '\0' && *p != ';') p++;
            if (*p != ';') return -1;
            p++;
        } else if (strchr("BCDFIJSZ", *p) != NULL) {
            p++;
        } else {
            return -1;
        }
    }
    if (*p != ')') return -1;
    return arg_count;
}

void parse_args_to_array(const char* descriptor, JValue_type_t* args_array, unsigned arg_count) {
    // 1. Validate inputs. If nothing to do or inputs are invalid, return immediately.
    if (!descriptor || !args_array || arg_count == 0 || descriptor[0] != '(') {
        return;
    }

    // 2. Start a single pass to fill the array.
    const char* p = descriptor + 1; // Start scanning after the opening '('

    for (unsigned i = 0; i < arg_count; ++i) {
        // Safety check: break if the descriptor ends unexpectedly.
        if (*p == '\0' || *p == ')') {
            break;
        }

        char current_type_char = *p;

        // Assign the type directly, as the enum values match the characters.
        if (current_type_char == 'L' || current_type_char == '[') {
            args_array[i] = EJVT_REFERENCE;
        } else {
            args_array[i] = (JValue_type_t)current_type_char;
        }

        // Advance the pointer 'p' to the beginning of the next argument descriptor.
        if (current_type_char == 'L') {
            // It's an object reference, e.g., "Ljava/lang/String;". Skip to the semicolon.
            while (*p != '\0' && *p != ';') {
                p++;
            }
        } else if (current_type_char == '[') {
            // It's an array. Skip all leading '[' characters.
            while (*p == '[') {
                p++;
            }
            // If it's an array of objects (e.g., "[L...;"), skip the class name too.
            if (*p == 'L') {
                while (*p != '\0' && *p != ';') {
                    p++;
                }
            }
        }
        p++; // Move past the primitive character (e.g., 'I') or the semicolon.
    }
}

JError_t linker_link(linker_t* linker){
    JError_t err = EJERR_OK;
    unsigned builtins_count = linker_load_builtins(linker); //This function should copy builtin classes into our linker

    //Step 0:
    classloader_class_t* cur_raw = NULL;
    JClass_t* cur_inlink = NULL;

    fht_t metadata_ht = {0}; //Temporary HT to store metadata
    void* metadata_ht_memory = malloc(fht_calculate_size(linker->loader->classes_stats.classes_loaded + builtins_count));
    FAIL_SET_JUMP(metadata_ht_memory,err,EJERR_SYSTEM_OOM,exit);

    bump_allocator_t metadata_allocator = {0};
    FAIL_SET_JUMP(bumper_create(&metadata_allocator,linker->loader->classes_stats.classes_loaded * sizeof(link_metadata_t)) == 0,
                err,EJERR_SYSTEM_OOM,exit);

    fht_init(&metadata_ht,metadata_ht_memory,
                linker->loader->classes_stats.classes_loaded,fht_hash_string,fht_compare_string);

    list_for_each_entry(cur_raw,&linker->loader->loaded_classes,list){
        JClass_t* new_class = bumper_calloc(&linker->arena,1,sizeof(*new_class));
        FAIL_SET_JUMP(new_class,err,EJERR_OOM,exit);

        INIT_LIST_HEAD(&new_class->list);
        list_add(&new_class->list,&linker->classes);

        new_class->flags.non_builtin = 1;
        new_class->flags.top_level = 1;
        linker->linker_stats.top_level_classes++;

        FAIL_SET_JUMP(cur_raw->constants[cur_raw->this_class].type == EJCT_class, err, EJERR_INVALID_CLASS,exit);
        FAIL_SET_JUMP(cur_raw->constants[cur_raw->super_class].type == EJCT_class, err, EJERR_INVALID_CLASS,exit);
        
        char* this_name = cur_raw->constants[((classloader_constant_class_t*)cur_raw->constants[cur_raw->this_class].data)->name_index].data;
        char* super_name = cur_raw->constants[((classloader_constant_class_t*)cur_raw->constants[cur_raw->super_class].data)->name_index].data;

        new_class->name = bumper_strdup(&linker->arena,this_name);

        link_metadata_t* metadata = bumper_alloc(&metadata_allocator,sizeof(*metadata));
        FAIL_SET_JUMP(metadata,err,EJERR_OOM,exit);

        metadata->parent_name = super_name;
        metadata->raw_class = cur_raw;
        metadata->field_constvals = NULL;

        fht_set(&metadata_ht,new_class->name,metadata);

        DEBUG_LOG("Loaded class: '%s'\n",new_class->name);
    }
    DEBUG_LOG("=======================\n");

//Step 1: parent searching
    list_for_each_entry(cur_inlink,&linker->classes,list){
        link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);

        if(cur_inlink->parent == NULL && cur_inlink->flags.non_builtin == 1){
            FAIL_SET_JUMP(metadata,err,EJERR_INVALID_CLASS,exit);

            cur_inlink->parent = class_find(linker,metadata->parent_name);
            FAIL_SET_JUMP(cur_inlink->parent,err,({
                printf("%s: cannot found class '%s' of '%s'\n",__PRETTY_FUNCTION__,metadata->parent_name,cur_inlink->name);
                (EJERR_NOT_FOUND);
            }),exit);
            cur_inlink->parent->flags.top_level = 0;
            linker->linker_stats.top_level_classes--;
            DEBUG_LOG("'%s' have '%s' as parent\n",cur_inlink->name,cur_inlink->parent ? cur_inlink->parent->name : NULL);
        }

        if(cur_inlink->flags.non_builtin == 1){
            cur_inlink->info = bumper_alloc(&linker->arena,sizeof(*cur_inlink->info));
            FAIL_SET_JUMP(cur_inlink->info,err,EJERR_OOM,exit);

            cur_inlink->info->constant_pool.count = metadata->raw_class->constants_count;
            cur_inlink->info->constant_pool.constants = bumper_calloc(&linker->arena,cur_inlink->info->constant_pool.count,
                                                                sizeof(*cur_inlink->info->constant_pool.constants));
            FAIL_SET_JUMP(cur_inlink->info->constant_pool.constants,err,EJERR_OOM,exit);

            DEBUG_LOG("'%s' have %d constants\n",cur_inlink->name, cur_inlink->info->constant_pool.count);
        }
    }
    DEBUG_LOG("=======================\n");

    list_for_each_entry(cur_inlink,&linker->classes,list){
        if(cur_inlink->flags.non_builtin){
            link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);
            classloader_class_t* raw_class = metadata->raw_class;

            for(unsigned i = 0; i < raw_class->constants_count; i++){
                classloader_constant_t* raw_constant = &raw_class->constants[i];
                JClass_constant_t* parsed_constant = &cur_inlink->info->constant_pool.constants[i];

                parsed_constant->type = raw_constant->type;

                switch(raw_constant->type){
                    default: break;

                    case EJCT_class:{
                        classloader_constant_class_t* constant_class = raw_constant->data;
                        char* class_name = raw_class->constants[constant_class->name_index].data;

                        if(class_name[0] != '['){
                            JClass_t* found = class_find(linker,class_name);
                            parsed_constant->value = found;
                        } else {
                            JClass_t* array_class = bumper_calloc(&linker->arena,1,sizeof(*array_class));
                            FAIL_SET_JUMP(array_class,err,EJERR_OOM,exit);

                            array_class->flags.non_builtin = 1;
                            array_class->flags.is_array = 1;

                            array_class->name = bumper_strdup(&linker->arena,class_name);
                            FAIL_SET_JUMP(array_class->name,err,EJERR_OOM,exit);

                            array_class->parent = class_find(linker,"java/lang/Object");
                            parsed_constant->value = array_class;
                        }
                    }
                    break;

                    case EJCT_int:
                    case EJCT_float:
                    case EJCT_long:
                    case EJCT_double:{
                        uint8_t size = (raw_constant->type == EJCT_int || raw_constant->type == EJCT_float) ? sizeof(uint32_t) : sizeof(uint64_t);
                        parsed_constant->value = bumper_calloc(&linker->arena,1,size);
                        FAIL_SET_JUMP(parsed_constant->value,err,EJERR_OOM,exit);

                        memcpy(parsed_constant->value,raw_constant->data,size);
                    }
                    break;

                    case EJCT_string:{
                        parsed_constant->type = EJCT_unitialised_string; //Will be initialised as Java object in ldc opcode
                        uint16_t string_index = *(uint16_t*)raw_constant->data;

                        parsed_constant->value = bumper_strdup(&linker->arena,raw_class->constants[string_index].data);
                        FAIL_SET_JUMP(parsed_constant->value,err,EJERR_OOM,exit);
                    }
                    break;
                }
            }
        }
    }

//Step 3: initialise and calculate fields
    list_for_each_entry(cur_inlink,&linker->classes,list){
        if(!cur_inlink->flags.is_array && cur_inlink->flags.non_builtin){
            link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);
            classloader_class_t* raw_class = metadata->raw_class;

            cur_inlink->info->fields = bumper_alloc(&linker->arena, sizeof(fht_t));
            FAIL_SET_JUMP(cur_inlink->info->fields,err,EJERR_OOM,exit);

            void* fields_memory = bumper_calloc(&linker->arena,1,fht_calculate_size(raw_class->fields_count));
            FAIL_SET_JUMP(fields_memory,err,EJERR_OOM,exit);

            fht_init(cur_inlink->info->fields,fields_memory,raw_class->fields_count,fht_hash_string,fht_compare_string);

            for(unsigned i = 0; i < raw_class->fields_count; i++){
                classloader_field_t* raw_field = &raw_class->fields[i];

                JField_t* new_field = bumper_alloc(&linker->arena, sizeof(*new_field));
                FAIL_SET_JUMP(new_field,err,EJERR_OOM,exit);

                new_field->owner = cur_inlink;
                new_field->flags.is_static = (raw_field->access_flags & ACC_STATIC) == ACC_STATIC;
                new_field->flags.is_private = (raw_field->access_flags & ACC_PRIVATE) == ACC_PRIVATE;

                new_field->type = ((char*)raw_class->constants[raw_class->fields[i].descriptor_index].data)[0];
                if(new_field->type == '[') //Arrays will be just references
                    new_field->type = EJVT_REFERENCE;

                char* name = raw_class->constants[raw_field->name_index].data;
                char* descriptor = raw_class->constants[raw_class->fields[i].descriptor_index].data;
        
                char* mangled_name = bumper_alloc(&linker->arena,strlen(name) + strlen(descriptor) + 2*sizeof(char));
                FAIL_SET_JUMP((new_field->mangled_name = mangled_name),err,EJERR_OOM,exit);

                sprintf(mangled_name,"%s@%s",name,descriptor);

                DEBUG_LOG("%s/%s initialised\n",cur_inlink->name,new_field->mangled_name);

                for(unsigned attr = 0; attr < raw_field->attributes_count; attr++){
                    char* attr_name = raw_class->constants[raw_field->attributes[attr].name_index].data;
                    uint16_t constantvalue_index = be16_to_cpu(*(uint16_t*)raw_field->attributes[attr].classloader_attribute);

                    if(strcmp("ConstantValue",attr_name) == 0){ //Keep that for later......
                        if(metadata->field_constvals == NULL){ 
                            metadata->field_constvals = alloca(sizeof(fht_t));
                            fht_init(metadata->field_constvals,alloca(fht_calculate_size(raw_class->fields_count)),
                                                                raw_class->fields_count,fht_hash_string,fht_compare_string);

                        }
                        fht_set(metadata->field_constvals,new_field->mangled_name,cur_inlink->info->constant_pool.constants[constantvalue_index - 1].value);
                        DEBUG_LOG("Field '%s' have constant value attribute. Keeping it for later\n",new_field->mangled_name);
                    }
                }

                fht_set(cur_inlink->info->fields,new_field->mangled_name,new_field);
            }
        }
    }

    unsigned TLC_index = 0;
    JClass_t** top_level_classes = alloca((linker->linker_stats.top_level_classes + 1) * sizeof(*top_level_classes));
    list_for_each_entry(cur_inlink,&linker->classes,list){
        if(cur_inlink->flags.non_builtin && !cur_inlink->flags.is_array && cur_inlink->flags.top_level){
            top_level_classes[TLC_index++] = cur_inlink;
        }
    }
   
    for(unsigned i = 0; i < TLC_index; i++){
        size_t max_offset[2] = {0};
        JField_t* last_field[2] = {0};
        for(JClass_t* cur = top_level_classes[i]; cur; cur = cur->parent){
            if(cur->info && cur->info->fields){
                fht_iterator_t iter = {0};
                fht_iterator_init(cur->info->fields,&iter);

                fht_entry_t* current_entry = NULL;
                while((current_entry = fht_next(&iter))){
                    JField_t* field = current_entry->value;
                    last_field[field->flags.is_static] = field;
                    max_offset[field->flags.is_static] += JValue_sizeof(field->type);

                }

                cur->info->fields_size = max_offset[0]; //Size of memory block required for object of this class is 
                                                                                                   //the sum of maximum offset + sizeof value at that offset
            }
        }

        for(JClass_t* cur = top_level_classes[i]; cur; cur = cur->parent){
            if(cur->info)
                cur->info->fields_size += cur->parent ? cur->parent->info ? cur->parent->info->fields_size : 0 : 0; //Accumulate size of non static fields

            if(cur->info) DEBUG_LOG("%s have non-static fields size: %d bytes\n",cur->name,cur->info->fields_size);
        }

        void* static_fields = bumper_calloc(&linker->arena,1,
                    max_offset[1] + JValue_sizeof(last_field[1]->type));

        FAIL_SET_JUMP(static_fields,err,EJERR_OOM,exit);

        for(JClass_t* cur = top_level_classes[i]; cur; cur = cur->parent){
            if(cur->info && cur->info->fields){
                fht_iterator_t iter = {0};
                fht_iterator_init(cur->info->fields,&iter);

                fht_entry_t* current_entry = NULL;
                while((current_entry = fht_next(&iter))){
                    JField_t* field = current_entry->value;
                    field->offset = (max_offset[field->flags.is_static] -= JValue_sizeof(field->type));

                    DEBUG_LOG("%s/%s have offset %zu\n",cur->name,field->mangled_name,field->offset);
                }
                cur->info->static_fields = static_fields;
            }
        }
    }

//Step 4: Create and patch methods
    list_for_each_entry(cur_inlink, &linker->classes, list){
        if(cur_inlink->flags.non_builtin && !cur_inlink->flags.is_array){
            link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);
            classloader_class_t* raw_class = metadata->raw_class;

            cur_inlink->info->methods = bumper_alloc(&linker->arena,sizeof(*cur_inlink->info->methods));
            FAIL_SET_JUMP(cur_inlink->info->methods,err,EJERR_OOM,exit);

            void* methods_memory = bumper_alloc(&linker->arena, fht_calculate_size(raw_class->methods_count));
            FAIL_SET_JUMP(methods_memory,err,EJERR_OOM,exit);

            fht_init(cur_inlink->info->methods,methods_memory,raw_class->methods_count,fht_hash_string,fht_compare_string);

            for(unsigned i = 0; i < raw_class->methods_count; i++){
                classloader_method_t* raw_method = &raw_class->methods[i];

                JMethod_t* new_method = bumper_calloc(&linker->arena,1,sizeof(*new_method));
                FAIL_SET_JUMP(new_method,err,EJERR_OOM,exit);

                new_method->flags.non_builtin = 1;
                new_method->flags.is_native = (raw_method->access_flags & ACC_NATIVE) == ACC_NATIVE;
                new_method->flags.is_static = (raw_method->access_flags & ACC_STATIC) == ACC_STATIC;
                new_method->flags.is_synchronized = (raw_method->access_flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED;

                new_method->owner = cur_inlink;

                char* name = raw_class->constants[raw_method->name_index].data;
                char* description = raw_class->constants[raw_method->descriptor_index].data;
                char* mangled_name = bumper_alloc(&linker->arena,strlen(name) + strlen(description) + 2*sizeof(char));
                FAIL_SET_JUMP(mangled_name,err,EJERR_OOM,exit);

                sprintf(mangled_name,"%s@%s",name,description);
                new_method->mangled_name = mangled_name;

                char* return_type = strchr(description,')') + 1;
                new_method->return_type = *return_type == '[' ? 'L' : *return_type;

                int argument_count = 0;
                FAIL_SET_JUMP((argument_count = get_argument_count_from_descriptor(description)) >= 0,err,EJERR_INVALID_CLASS,exit);

                new_method->frame_info.argument_types = bumper_calloc(&linker->arena,argument_count,sizeof(JValue_type_t));
                FAIL_SET_JUMP(new_method->frame_info.argument_types, err, EJERR_OOM,exit);

                parse_args_to_array(description, new_method->frame_info.argument_types, argument_count);
                new_method->frame_info.arguments_count = argument_count;

                for(unsigned attr = 0; attr < raw_method->attributes_count; attr++){
                    classloader_attribute_t* attr_info = &raw_method->attributes[attr];
                    char* attr_name = raw_class->constants[attr_info->name_index].data;
                    void* attr_data = attr_info->classloader_attribute;

                    if(strcmp("Code",attr_name) == 0 && !new_method->flags.is_native){
                        JError_t parser_err = parse_bytecode(attr_data,new_method,&linker->arena);
                        FAIL_SET_JUMP(parser_err == EJERR_OK,err,parser_err,exit);

                        continue;
                    }
                }
                if(new_method->flags.is_native){
                    TODO("Native function interface (method resolution)");
                }

                fht_set(cur_inlink->info->methods,mangled_name,new_method);
            }
        }
    }

//Step 4.5: patch built in methods arguments
    list_for_each_entry(cur_inlink, &linker->classes, list){
        if(cur_inlink->info && cur_inlink->info->methods){
            fht_iterator_t iter = {0};
            fht_iterator_init(cur_inlink->info->methods, &iter);

            fht_entry_t* entry = NULL;
            while((entry = fht_next(&iter))){
                JMethod_t* method = entry->value;
                method->owner = cur_inlink;
                if(!method->flags.non_builtin){
                    method->frame_info.arguments_count += !method->flags.is_static; //Add one argument if non static(this)
                    method->frame_info.locals_count += method->frame_info.arguments_count;
                }
            }
        }
    }

//Step 5: initialise interfaces
    list_for_each_entry(cur_inlink, &linker->classes, list){
        link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);
        if(metadata){
            classloader_class_t* raw_class = metadata->raw_class;
            cur_inlink->implements.count = raw_class->interfaces_count;
            cur_inlink->implements.implement = bumper_calloc(&linker->arena,cur_inlink->implements.count,sizeof(*cur_inlink->implements.implement));
            FAIL_SET_JUMP(cur_inlink->implements.implement,err,EJERR_OOM,exit);

            for(unsigned i = 0; i < cur_inlink->implements.count; i++){
                cur_inlink->implements.implement[i] = cur_inlink->info->constant_pool.constants[raw_class->interfaces[i]].value;
            }
        }
    }

//Step 6: field and method references
    list_for_each_entry(cur_inlink,&linker->classes,list){
        if(cur_inlink->flags.non_builtin){
            link_metadata_t* metadata = fht_get(&metadata_ht,cur_inlink->name);
            classloader_class_t* raw_class = metadata->raw_class;

            for(unsigned i = 0; i < raw_class->constants_count; i++){
                classloader_constant_t* raw_constant = &raw_class->constants[i];
                JClass_constant_t* parsed_constant = &cur_inlink->info->constant_pool.constants[i];

                parsed_constant->type = raw_constant->type;

                switch(raw_constant->type){
                    default: break;

                    case EJCT_methodref:
                    case EJCT_fieldref:
                    case EJCT_interfacemethodref:{
                        classloader_constant_fmim_t* ref = raw_constant->data;

                        classloader_constant_class_t* class = raw_class->constants[ref->class_index].data;
                        char* class_name = raw_class->constants[class->name_index].data;

                        JClass_t* ref_class = class_find(linker, class_name);
                        FAIL_SET_JUMP(ref_class,err,EJERR_NOT_FOUND,exit);

                        classloader_constant_name_and_type_t* nameandtype = raw_class->constants[ref->name_and_type_index].data;
                        char* name = raw_class->constants[nameandtype->name_index].data;
                        char* descriptor = raw_class->constants[nameandtype->descriptor_index].data;

                        char* mangled_name = bumper_alloc(&linker->arena,strlen(name) + strlen(descriptor) + 2*sizeof(char));
                        FAIL_SET_JUMP(mangled_name,err,EJERR_OOM,exit);

                        sprintf(mangled_name,"%s@%s",name,descriptor);

                        if(ref_class->info){
                            fht_t* ref_ht = parsed_constant->type == EJCT_methodref || parsed_constant->type == EJCT_interfacemethodref 
                                                                                    ? ref_class->info->methods : ref_class->info->fields;

                            parsed_constant->value = fht_get(ref_ht,mangled_name);
                            assert(parsed_constant->value);
                        } else parsed_constant->value = NULL;                      
                    }
                    break;
                }
            }
        }
    }

    for(unsigned i = 0; i < TLC_index; i++){
        for(JClass_t* cur = top_level_classes[i]; cur; cur = cur->parent){
            link_metadata_t* metadata = fht_get(&metadata_ht,cur->name);
            if(metadata && metadata->field_constvals){
                fht_iterator_t iter = {0};
                fht_iterator_init(cur->info->fields, &iter);

                fht_entry_t* entry = NULL;
                while((entry = fht_next(&iter))){
                    JField_t* field = entry->value;
                    void* value = fht_get(metadata->field_constvals,field->mangled_name);

                    if(value){
                        field_set(field,cur->info->static_fields,value);
                        DEBUG_LOG("Field '%s' constant value is set\n",field->mangled_name);
                    }
                }
            }
        }   
    }

exit:
    free(metadata_ht_memory);
    bumper_destroy(&metadata_allocator);
    return err;
}

JField_t* class_find_field(JClass_t* class, char* name, bool is_static){
    JField_t* ret = NULL;

    for(JClass_t* cur = class; cur; cur = cur->parent){
        if(!cur->flags.is_array && cur->info && cur->info->fields){
            JField_t* field = fht_get(cur->info->fields, name);
            if(field && field->flags.is_static == is_static){
                ret = field;
            }if(field && field->flags.is_static != is_static) break;
        }
    }
    return ret;
}

void* class_get_staticfield(JField_t* field){ //TODO: check if field really static
    return field ? &field->owner->info->static_fields[field->offset] : NULL;
}

JMethod_t* class_find_method(JClass_t* class, char* mangled_name, bool is_static){
    for(JClass_t* cur = class; cur; cur = cur->parent){
        if(cur->info && cur->info->methods){
            JMethod_t* method = fht_get(cur->info->methods,mangled_name);

            if(method && method->flags.is_static == is_static) return method;
            else if(method && !(method->flags.is_static == is_static)) return NULL;
        }
    }
    return NULL;
}

bool is_classes_compatible(JClass_t* class, JClass_t* compatible_to){
    for(JClass_t* cur = class; cur; cur = cur->parent){
        if(compatible_to == cur)
            return true;

        for(unsigned i = 0; i < cur->implements.count; i++){
            if(cur->implements.implement[i] == cur || cur->implements.implement[i] == class)
                continue;
            
            if(is_classes_compatible(cur->implements.implement[i], compatible_to))
                return true;
        }
    }
    return false;
}