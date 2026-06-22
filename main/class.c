#include "config.h"

#include "class.h"
#include "jerror.h"
#include "list.h"
#include "loader.h"
#include "parser.h"
#include "bumper.h"
#include "stringpool.h"
#include "native_methods_service.h"
#include "thread.h"
#include "heap.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bump_allocator_t s_permament_arena = {0}, s_temporary_arena = {0};
static ClassTable_t s_class_table = {0};
static struct list_head s_class_list = {0};
static bool s_initialised = false; //Reinit protection (VM reset in firmwares)

#define JVM_CLASSPATH "java_src"

void classes_init(){
    INIT_LIST_HEAD(&s_class_list);

    if(!s_initialised){
        assert(bumper_create(&s_permament_arena, CLASS_PERMAMENT_ARENA) == 0);
        assert(bumper_create(&s_temporary_arena, CLASS_TEMPOPARY_ARENA) == 0);
        s_initialised = true;
    } else {
        memset(&s_class_table, 0, sizeof(s_class_table));
        bumper_reset(&s_permament_arena);
        bumper_reset(&s_temporary_arena);
    }
}

static uint32_t hash(uint16_t name_id) {
    uint32_t x = (uint32_t)name_id;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

typedef struct{
    uint32_t index;
    struct{
        union{
            uint8_t all;
            struct{
                unsigned is_error:1;
                unsigned is_found:1;
            };
        };
    }flags;
}ClassTableCalculatedIndex_t;

static ClassTableCalculatedIndex_t calculate_index(uint16_t name_id){
    uint32_t start_pos = hash(name_id) % MAX_LOADED_CLASSES;
    ClassTableCalculatedIndex_t retval = {0};

    for(uint32_t i = 0; i < MAX_LOADED_CLASSES; i++){
        uint32_t current_idx = (start_pos + i) % MAX_LOADED_CLASSES;
        if(s_class_table.classes[current_idx] == NULL){
            retval.index = current_idx;
            retval.flags.is_found = 0;
            retval.flags.is_error = 0;
            goto exit;
        } else if(s_class_table.classes[current_idx]->name_id == name_id){
            retval.index = current_idx;
            retval.flags.is_found = 1;
            retval.flags.is_error = 0;
            goto exit;
        }
    }

    retval.flags.is_error = 1;
exit:
    return retval;
}

Class_t* class_find(uint16_t name_id){
    ClassTableCalculatedIndex_t index = calculate_index(name_id);

    return index.flags.is_found && !index.flags.is_error ? s_class_table.classes[index.index] : NULL;
}

Error_t class_insert(Class_t* class){
    Error_t err = JERR_OK;
    ClassTableCalculatedIndex_t index = calculate_index(class->name_id);

    FAIL_SET_JUMP(!index.flags.is_error, err, JERR_OOM, exit);

    INIT_LIST_HEAD(&class->list);
    list_add(&class->list, &s_class_list);
    s_class_table.classes[index.index] = class;

exit:
    return err;
}

extern struct list_head* classes_get_all(){
    return &s_class_list;
}

static Error_t class_convert_from_raw(JRawClass_t* parsed_class, Class_t** out);
static Error_t class_link(Class_t* class);
static JavaValueType_t array_class_type(char* name){
    if(strlen(name) > 1) return TYPE_REFERENCE;
    
    JavaValueType_t types[] = {TYPE_BOOL, TYPE_BYTE, TYPE_CHAR, TYPE_SHORT, TYPE_INT,
                               TYPE_FLOAT, TYPE_LONG, TYPE_DOUBLE, TYPE_REFERENCE, TYPE_VOID};

    for(unsigned i = 0; i < sizeof(types) / sizeof(types[0]); i++){
        if(name[0] == types[i]) return types[i];
    }

    return TYPE_REFERENCE;
}

Error_t class_load_bynameid(uint16_t name_id, Class_t** out){
    Error_t err = JERR_OK;
    assert(out);

    if((*out = class_find(name_id))) return JERR_OK;

    char* string_name = stringpool_get(name_id);
    FAIL_SET_JUMP(string_name, err, JERR_BADPARAM, exit); //What the fuck did you just passed here?
    if(string_name[0] == '['){
        Class_t* jlObject = NULL;
        int jlname_id = stringpool_add("java/lang/Object");
        FAIL_SET_JUMP(jlname_id >= 0, err, JERR_OOM, exit);

        FAIL_SET_JUMP((err = class_load_bynameid(jlname_id, &jlObject)) == JERR_OK, err, err, exit);

        Class_t* array_class = bumper_calloc(&s_permament_arena, 1, sizeof(*array_class));
        FAIL_SET_JUMP(array_class, err, JERR_OOM, exit);

        INIT_LIST_HEAD(&array_class->hierarchy_list);
        
        array_class->name_id = name_id;

        array_class->array_type = array_class_type(strrchr(string_name, '[') + 1);
        FAIL_SET_JUMP(array_class->array_type != TYPE_VOID, err, JERR_TYPECHECK_FAILURE, exit); //WHYYYYYYY?

        array_class->parent = jlObject;
        array_class->vtable = jlObject->vtable;
        array_class->vtable_size = jlObject->vtable_size;
        array_class->object_size = jlObject->object_size;

        Class_t* jlClass = NULL;
        FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/Class"),  &jlClass)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = heap_class_object_alloc(jlClass, &array_class->class_object)) == JERR_OK, err, err, exit);

        array_class->flags.is_linked = 1;
        array_class->flags.is_array = 1;

        *out = array_class;
        FAIL_SET_JUMP((err = class_insert(array_class)) == JERR_OK, err, err, exit);
    } else {
        FAIL_SET_JUMP((err = class_convert_from_raw(loader_load_class(string_name), out)) == JERR_OK, err, err, exit);
        FAIL_SET_JUMP((err = class_link(*out)) == JERR_OK, err, err, exit);
    }

exit:
    return err;
}

static int find_patched_symbol_index(unsigned cp_index, struct list_head* patch_list){
    ConstantPoolPatchSymbol_t* patch_sym = NULL;
    list_for_each_entry(patch_sym, patch_list, list){
        if(patch_sym->cp_index == cp_index)
            return patch_sym->symtab_index;
    }
    return -1;
}

static int patch_constantpool(Class_t* this_class, struct list_head* cp_convert_list, JConstantPool_t* constantpool){
    unsigned symtab_index = 0; //Index at symbol table

    for(unsigned i = 1; i < constantpool->count; i++){
        JConstant_t* constant = &constantpool->constants[i];
        if(constant->type == EJCT_CLASS || constant->type == EJCT_INT
           || constant->type == EJCT_FLOAT || constant->type == EJCT_LONG
           || constant->type == EJCT_DOUBLE || constant->type == EJCT_FIELDREF
           || constant->type == EJCT_METHODREF || constant->type == EJCT_INTERFACE_METHODREF
           || constant->type == EJCT_STRING){

            ConstantPoolPatchSymbol_t* patch_sym = bumper_calloc(&s_temporary_arena, 1, sizeof(*patch_sym));
            if(!patch_sym) return 1;

            INIT_LIST_HEAD(&patch_sym->list);
            patch_sym->cp_index = i;
            patch_sym->symtab_index = symtab_index++;

            list_add(&patch_sym->list, cp_convert_list);

            if(constant->type == EJCT_LONG || constant->type == EJCT_DOUBLE) i++;
        }
    }

    this_class->symtab.count = symtab_index;
    this_class->symtab.symbols = bumper_calloc(&s_permament_arena, this_class->symtab.count, sizeof(*this_class->symtab.symbols));
    if(!this_class->symtab.symbols) return 1;

    ConstantPoolPatchSymbol_t* patch_sym = NULL;
    list_for_each_entry(patch_sym, cp_convert_list, list){
        JConstant_t* constant = parser_constantpool_get(constantpool, patch_sym->cp_index);
        ClassSymbol_t* symbol = &this_class->symtab.symbols[patch_sym->symtab_index];
        
        switch(constant->type){
            case EJCT_CLASS:{
                ClassProxySymbol_t* proxy_class = bumper_calloc(&s_permament_arena, 1, sizeof(*proxy_class));
                if(!proxy_class) return 1;

                uint16_t name_index = *(uint16_t*)constant->value;

                JRawUTF8_t* utf8_name = parser_constantpool_get(constantpool, name_index)->value;
                int class_name_id = stringpool_add((char*)utf8_name->string);
                
                if(class_name_id < 0) return 1;

                proxy_class->origin_name_id = class_name_id;
                proxy_class->self_name_id = 0; //DO NOT USE IT THERE!

                symbol->type = PROXY_SYMBOL_CLASS;
                symbol->value = proxy_class;
            }
            break;

            case EJCT_INTERFACE_METHODREF:
            case EJCT_METHODREF:
            case EJCT_FIELDREF:{
                ClassProxySymbol_t* proxy_FMIM = bumper_calloc(&s_permament_arena, 1, sizeof(*proxy_FMIM));
                if(!proxy_FMIM) return 1;

                JRaw_FMIM_ref_t* FMIM = constant->value;
                
                uint16_t class_name_index = *(uint16_t*)parser_constantpool_get(constantpool, FMIM->class_index)->value;

                JRawUTF8_t* class_utf8_name = parser_constantpool_get(constantpool, class_name_index)->value;
                int class_name_id = stringpool_add((char*)class_utf8_name->string);

                if(class_name_id < 0) return 1;

                JRawNameAndType_t* nameandtype = parser_constantpool_get(constantpool, FMIM->nameandtype_index)->value;
                JRawUTF8_t* name = parser_constantpool_get(constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* descriptor = parser_constantpool_get(constantpool, nameandtype->descriptor_index)->value;

                char* name_cstr = (char*)name->string;
                char* descriptor_cstr = (char*)descriptor->string;

                size_t mangled_len = strlen(name_cstr) + strlen(descriptor_cstr) + 2;
                char mangled_name[mangled_len];
                memset(mangled_name, 0, mangled_len);
                snprintf(mangled_name, mangled_len, "%s@%s", name_cstr, descriptor_cstr);

                int fmim_name_id = stringpool_add(mangled_name);
                if(fmim_name_id < 0) return 1;

                proxy_FMIM->origin_name_id = class_name_id;
                proxy_FMIM->self_name_id = fmim_name_id;
                
                switch(constant->type){
                    case EJCT_FIELDREF:
                        symbol->type = PROXY_SYMBOL_FIELD;
                        break;
                    case EJCT_INTERFACE_METHODREF:
                    case EJCT_METHODREF:
                        symbol->type = PROXY_SYMBOL_METHOD;
                        break;
                }
                symbol->value = proxy_FMIM;
            }
            break;

            case EJCT_STRING:{
                ClassProxySymbol_t* proxy_string = bumper_calloc(&s_permament_arena, 1, sizeof(*proxy_string));
                if(!proxy_string) return 1;

                int class_name_id = stringpool_add("java/lang/String");
                if(class_name_id < 0) return 1;

                JRawUTF8_t* utf8 = parser_constantpool_get(constantpool, *(uint16_t*)constant->value)->value;

                int string_name_id = stringpool_add((char*)utf8->string);
                if(string_name_id < 0) return 1;

                proxy_string->origin_name_id = class_name_id;
                proxy_string->self_name_id = string_name_id;

                symbol->type = PROXY_SYMBOL_STRING;
                symbol->value = proxy_string;
            }
            break;

            case EJCT_DOUBLE:
            case EJCT_LONG:
            case EJCT_FLOAT:
            case EJCT_INT:{
                switch(constant->type){
                    case EJCT_INT:
                        symbol->type = SYMBOL_INT;
                        break;
                    case EJCT_FLOAT:
                        symbol->type = SYMBOL_FLOAT;
                        break;
                    case EJCT_LONG:
                        symbol->type = SYMBOL_LONG;
                        break;
                    case EJCT_DOUBLE:
                        symbol->type = SYMBOL_DOUBLE;
                        break;
                }

                unsigned sz = symbol->type == SYMBOL_LONG || symbol->type == SYMBOL_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
                symbol->value = bumper_calloc(&s_permament_arena, 1, sz);
                if(!symbol->value) return 1;

                memcpy(symbol->value, constant->value, sz);
            }
            break;
        }
    }

    return 0;
}

#include "lb_endian.h"
#include "opcodes.h"

static void patch_bytecode(Class_t* class, MethodBytecode_t* bytecode){
    uint8_t* code = bytecode->code;
    ClassLinkTimeMetadata_t* metadata = class->metadata;

    for(uint32_t pc = 0; pc < bytecode->code_length; pc += 1 + JOpcode_args_sizes[code[pc]]){
        uint8_t* opcode = &code[pc];
        switch(*opcode){
            default: break;

            case EJOPCODE_LOOKUPSWITCH:
            case EJOPCODE_TABLESWITCH:
                assert(0 && "TODO: switches");

            case EJOPCODE_INVOKEINTERFACE:
            case EJOPCODE_INSTANCEOF:
            case EJOPCODE_CHECKCAST:
            case EJOPCODE_ANEWARRAY:
            case EJOPCODE_MULTIANEWARRAY:
            case EJOPCODE_NEW:
            case EJOPCODE_LDC_W:
            case EJOPCODE_LDC2_W:
            case EJOPCODE_INVOKESPECIAL:
            case EJOPCODE_INVOKESTATIC:
            case EJOPCODE_INVOKEVIRTUAL:
            case EJOPCODE_GETSTATIC:
            case EJOPCODE_GETFIELD:
            case EJOPCODE_PUTSTATIC:
            case EJOPCODE_PUTFIELD:{
                *((uint16_t*)(opcode + 1)) = cpu_to_be16(find_patched_symbol_index(be16_to_cpu(*(uint16_t*)(opcode + 1)), &metadata->cp_patch_list));
            }
            break;

            case EJOPCODE_LDC:{
                *((uint8_t*)(opcode + 1)) = find_patched_symbol_index(*(opcode + 1), &metadata->cp_patch_list);
            }
            break;
        }
    }
}

static Error_t parse_method_descriptor(const char* descriptor, JavaValueType_t* arguments_size, JavaValueType_t* return_type){
    Error_t err = JERR_OK;
    
    FAIL_SET_JUMP(descriptor && return_type && arguments_size, err, JERR_BADPARAM, exit);
    
    // Descriptor format: (param_types)return_type
    // Examples: ()V, (II)I, (Ljava/lang/String;)V, ([I)Z
    
    if(*descriptor != '('){
        return JERR_BADPARAM;
    }
    descriptor++;
    
    *arguments_size = 0;
    
    // Parse argument types
    while(*descriptor != ')' && *descriptor != '\0'){
        JavaValueType_t type;
        
        if(*descriptor == 'L'){
            // Object type: Ljava/lang/String;
            type = TYPE_REFERENCE;
            descriptor = strchr(descriptor, ';');
            if(!descriptor) return JERR_BADPARAM;
            descriptor++;
        }
        else if(*descriptor == '['){
            // Array type: [I, [Ljava/lang/String;, etc
            while(*descriptor == '[') descriptor++;
            
            if(*descriptor == 'L'){
                type = TYPE_REFERENCE;
                descriptor = strchr(descriptor, ';');
                if(!descriptor) return JERR_BADPARAM;
                descriptor++;
            }
            else {
                type = (JavaValueType_t)*descriptor;
                descriptor++;
            }
        }
        else {
            // Primitive type
            type = (JavaValueType_t)*descriptor;
            descriptor++;
        }
        
        *arguments_size += type == TYPE_LONG || type == TYPE_DOUBLE ? 2 : 1;
    }
    
    if(*descriptor != ')'){
        return JERR_BADPARAM;
    }
    descriptor++;
    
    // Parse return type
    if(*descriptor == '\0'){
        return JERR_BADPARAM;
    }
    
    if(*descriptor == 'L'){
        // Object return type
        *return_type = TYPE_REFERENCE;
    }
    else if(*descriptor == '['){
        // Array return type
        *return_type = TYPE_REFERENCE;  // Arrays are references
    }
    else if(*descriptor == 'V'){
        // Void return type
        *return_type = TYPE_VOID;
    }
    else {
        // Primitive return type
        *return_type = (JavaValueType_t)*descriptor;
    }
    
    
exit:
    return err;
}

static Error_t generate_method_locals_bitmap(const char* descriptor, bool is_virtual, uint32_t* bitmap){
    Error_t err = JERR_OK;
    
    FAIL_SET_JUMP(descriptor, err, JERR_BADPARAM, exit);
    
    // Descriptor format: (param_types)return_type
    // Examples: ()V, (II)I, (Ljava/lang/String;)V, ([I)Z
    
    if(*descriptor != '('){
        return JERR_BADPARAM;
    }
    descriptor++;
    
    if(is_virtual)
        SHADOW_SET_REF(bitmap, 0);
    unsigned index = is_virtual;

    // Parse argument types
    while(*descriptor != ')' && *descriptor != '\0'){
        JavaValueType_t type;
        
        if(*descriptor == 'L'){
            // Object type: Ljava/lang/String;
            SHADOW_SET_REF(bitmap, index++);
            descriptor = strchr(descriptor, ';');
            if(!descriptor) return JERR_BADPARAM;
            descriptor++;
        }
        else if(*descriptor == '['){
            // Array type: [I, [Ljava/lang/String;, etc
            SHADOW_SET_REF(bitmap, index++);
            while(*descriptor == '[') descriptor++;
            
            if(*descriptor == 'L'){
                descriptor = strchr(descriptor, ';');
                if(!descriptor) return JERR_BADPARAM;
                descriptor++;
            }
            else {
                descriptor++;
            }
        }
        else {
            // Primitive type
            if(*descriptor == TYPE_LONG || *descriptor == TYPE_DOUBLE)
                index += 2;
            else index++;

            descriptor++;
        }
        
    }
    
    if(*descriptor != ')'){
        return JERR_BADPARAM;
    }
    
exit:
    return err;
}

static Error_t class_convert_from_raw(JRawClass_t* parsed_class, Class_t** out){
    Error_t err = JERR_OK;
    FAIL_SET_JUMP(parsed_class, err, JERR_NOCLASSDEF, exit);
    FAIL_SET_JUMP(out, err, JERR_BADPARAM, exit);

    JConstantPool_t* constantpool = &parsed_class->constantpool;
    //Base initialisation

    JConstant_t* this_class_const = parser_constantpool_get(constantpool, parsed_class->this_class);
    JConstant_t* super_class_const = parser_constantpool_get(constantpool, parsed_class->super_class);

    uint16_t this_name_const_index = *(uint16_t*)this_class_const->value;
    uint16_t super_name_const_index = super_class_const->type == EJCT_CLASS ? *(uint16_t*)super_class_const->value : 0;

    char* this_name_string = (char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, this_name_const_index)->value)->string;
    char* super_name_string = super_name_const_index ? (char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, super_name_const_index)->value)->string : NULL;

    int this_name_id = stringpool_add(this_name_string);
    int super_name_id = stringpool_add(super_name_string);

    FAIL_SET_JUMP((this_name_id >= 0 && (super_name_id >= 0 || super_name_string == NULL)), err, JERR_OOM, exit);

    Class_t* this_class = bumper_calloc(&s_permament_arena, 1, sizeof(*this_class));
    FAIL_SET_JUMP(this_class, err, JERR_OOM, exit);

    this_class->array_type = TYPE_VOID;
    this_class->flags.is_interface = (parsed_class->flags & ACC_INTERFACE) == ACC_INTERFACE;
    this_class->flags.is_final = (parsed_class->flags & ACC_FINAL) == ACC_FINAL;
    this_class->flags.is_abstract = (parsed_class->flags & ACC_ABSTRACT) == ACC_ABSTRACT;

    ClassLinkTimeMetadata_t* metadata = bumper_calloc(&s_temporary_arena, 1, sizeof(*metadata));
    FAIL_SET_JUMP(metadata, err, JERR_OOM, exit);

    metadata->is_root = !super_name_string;
    metadata->parent_name_id = super_name_id;
    metadata->implements_count = parsed_class->interfaces_count;
    metadata->implements = bumper_calloc(&s_temporary_arena, metadata->implements_count,sizeof(*metadata->implements));
    FAIL_SET_JUMP(metadata->implements, err, JERR_OOM, exit);

    for(unsigned i = 0; i < metadata->implements_count; i++){
        JConstant_t* iclass_const = parser_constantpool_get(constantpool, parsed_class->interfaces[i]);

        char* name = (char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, *(uint16_t*)iclass_const->value)->value)->string;
        int name_id = stringpool_add(name);
        FAIL_SET_JUMP(name_id >= 0, err, JERR_OOM, exit);

        metadata->implements[i] = name_id;
    }


    INIT_LIST_HEAD(&this_class->hierarchy_list);
    this_class->metadata = metadata;
    this_class->name_id = this_name_id;
    this_class->implements.count = metadata->implements_count;
    this_class->implements.implementations = bumper_calloc(&s_permament_arena, this_class->implements.count, sizeof(*this_class->implements.implementations));
    FAIL_SET_JUMP(this_class->implements.implementations, err, JERR_OOM, exit);

    //Constantpool / symtab init
    INIT_LIST_HEAD(&metadata->cp_patch_list);
    FAIL_SET_JUMP(patch_constantpool(this_class, &metadata->cp_patch_list, constantpool) == 0, err, JERR_OOM, exit);

    //Field initalisation
    size_t fields_count[2] = {0}; //0 - instance, 1 - static
    for(unsigned i = 0; i < parsed_class->fields_count; i++){
        fields_count[(parsed_class->fields[i].flags & ACC_STATIC) == ACC_STATIC]++;
    }

    this_class->static_fields.count = fields_count[1];
    this_class->instance_fields.count = fields_count[0];
    this_class->instance_fields.fields = bumper_calloc(&s_permament_arena, this_class->instance_fields.count, sizeof(*this_class->instance_fields.fields));
    this_class->static_fields.fields = bumper_calloc(&s_permament_arena, this_class->static_fields.count, sizeof(*this_class->static_fields.fields));

    FAIL_SET_JUMP(this_class->instance_fields.fields && this_class->static_fields.fields, err, JERR_OOM, exit); 

    unsigned field_index[2] = {0};
    size_t offsets[2] = {0};
    for(unsigned i = 0; i < parsed_class->fields_count; i++){
        JRawField_t* raw_field = &parsed_class->fields[i];
        bool is_static = (raw_field->flags & ACC_STATIC) == ACC_STATIC;

        Field_t* field_array = is_static ? this_class->static_fields.fields : this_class->instance_fields.fields;
        Field_t* field = &field_array[field_index[is_static]++];

        JConstant_t* raw_field_descriptor = parser_constantpool_get(constantpool, raw_field->descriptor_index);
        JConstant_t* raw_field_name = parser_constantpool_get(constantpool, raw_field->name_index);
        
        JRawUTF8_t* raw_field_descriptor_utf8 = raw_field_descriptor->value;
        JRawUTF8_t* raw_field_name_utf8 = raw_field_name->value;

        char* raw_field_descriptor_cstr = (char*)raw_field_descriptor_utf8->string;
        char* raw_field_name_cstr = (char*)raw_field_name_utf8->string;

        size_t mangled_len = strlen(raw_field_descriptor_cstr) + strlen(raw_field_name_cstr) + 2;
        char* mangled_name = bumper_calloc(&s_temporary_arena, 1, mangled_len);
        FAIL_SET_JUMP(mangled_name, err, JERR_OOM, exit);
        snprintf(mangled_name, mangled_len, "%s@%s", raw_field_name_cstr, raw_field_descriptor_cstr);

        field->class = this_class;
        field->type = raw_field_descriptor_utf8->string[0];
        field->offset = offsets[is_static];
        field->flags.is_static = is_static;
        field->size = field->type == TYPE_LONG || field->type == TYPE_DOUBLE ? sizeof(int64_t) : sizeof(int32_t);
        offsets[is_static] += field->type == TYPE_LONG || field->type == TYPE_DOUBLE ? 2 : 1;

        int name_id = stringpool_add(mangled_name);
        FAIL_SET_JUMP(name_id >= 0, err, JERR_OOM, exit);

        field->name_id = name_id;

        if(is_static){
            JRawAttribute_t* attribute = NULL;
            list_for_each_entry(attribute, &raw_field->attributes, list){
                if(attribute->type == EJAT_CONSTANTVALUE){
                    int symbol_index = find_patched_symbol_index(*(uint16_t*)attribute->info, &metadata->cp_patch_list);
                    FAIL_SET_JUMP(symbol_index >= 0, err, JERR_OOM, exit);
                    
                    field->constantvalue = &this_class->symtab.symbols[symbol_index];
                }
            }
        }
    }
    this_class->storage = bumper_calloc(&s_permament_arena, offsets[1], sizeof(int32_t));
    FAIL_SET_JUMP(this_class->storage, err, JERR_OOM, exit);

    this_class->object_size = offsets[0] * sizeof(int32_t); //Will will use this also on linker stage to calculate proper offsets

    this_class->methods.count = parsed_class->methods_count;
    this_class->methods.methods = bumper_calloc(&s_permament_arena, this_class->methods.count, sizeof(*this_class->methods.methods));
    FAIL_SET_JUMP(this_class->methods.methods, err, JERR_OOM, exit);

    for(unsigned i = 0; i < parsed_class->methods_count; i++){
        JRawMethod_t* raw_method = &parsed_class->methods[i];
        bool is_static = (raw_method->flags & ACC_STATIC) == ACC_STATIC;
        bool is_special = (raw_method->flags & ACC_PRIVATE) == ACC_PRIVATE || strcmp((char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, raw_method->name_index)->value)->string, "<init>") == 0;

        Method_t* method = &this_class->methods.methods[i];

        method->flags.is_native = (raw_method->flags & ACC_NATIVE) == ACC_NATIVE;
        method->flags.is_interface = (parsed_class->flags & ACC_INTERFACE) == ACC_INTERFACE;
        method->flags.is_syncronized = (raw_method->flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED;

        method->flags.is_static = is_static;
        method->flags.is_virtual = !(is_static || is_special);
        method->flags.is_special = is_special;
        
        method->class = this_class;

        JConstant_t* raw_method_descriptor = parser_constantpool_get(constantpool, raw_method->descriptor_index);
        JConstant_t* raw_method_name = parser_constantpool_get(constantpool, raw_method->name_index);
        
        JRawUTF8_t* raw_method_descriptor_utf8 = raw_method_descriptor->value;
        JRawUTF8_t* raw_method_name_utf8 = raw_method_name->value; 
        
        char* raw_method_descriptor_cstr = (char*)raw_method_descriptor_utf8->string;
        char* raw_method_name_cstr = (char*)raw_method_name_utf8->string;

        size_t mangled_len = strlen(raw_method_descriptor_cstr) + strlen(raw_method_name_cstr) + 2;
        char* mangled_name = bumper_calloc(&s_temporary_arena, 1, mangled_len);
        FAIL_SET_JUMP(mangled_name, err, JERR_OOM, exit);

        snprintf(mangled_name, mangled_len, "%s@%s", raw_method_name_cstr, raw_method_descriptor_cstr);
        int name_id = stringpool_add(mangled_name);

        FAIL_SET_JUMP(name_id >= 0, err, JERR_OOM, exit);

        method->name_id = name_id;
        
        FAIL_SET_JUMP((err = parse_method_descriptor(raw_method_descriptor_cstr,&method->args_slots,&method->return_type)) == JERR_OK, err, err, exit);
        method->args_slots += !method->flags.is_static;
        method->args_bitmap_size = (method->args_slots + 31) / 32; //32 bits in uint32_t.......
        FAIL_SET_JUMP((method->args_bitmap = bumper_calloc(&s_permament_arena, method->args_bitmap_size, sizeof(*method->args_bitmap))), err, JERR_OOM, exit);
        FAIL_SET_JUMP((err = generate_method_locals_bitmap(raw_method_descriptor_cstr, !method->flags.is_static, method->args_bitmap)) == JERR_OK, err, err, exit); 

        if(method->flags.is_native){
            FAIL_SET_JUMP((method->code = natives_find(stringpool_get(this_class->name_id), mangled_name)), err, JERR_NOTFOUND, exit);
        } else {
            JRawAttribute_t* attribute = NULL;
            list_for_each_entry(attribute, &raw_method->attributes, list){
                if(attribute->type == EJAT_CODE){
                    JCodeAttribute_t* code = attribute->info;
                    MethodBytecode_t* bytecode = bumper_calloc(&s_permament_arena, 1, sizeof(*bytecode));
                    FAIL_SET_JUMP(bytecode, err, JERR_OOM, exit);
                    FAIL_SET_JUMP((bytecode->verifier_info = bumper_calloc(&s_permament_arena, 1, sizeof(*bytecode->verifier_info))), err, JERR_OOM, exit);

                    JRawAttribute_t* inside_attribute = NULL;
                    list_for_each_entry(inside_attribute, &code->attributes, list){
                        if(inside_attribute->type == EJAT_STACKMAP){
                            JStackMap_t* stackmap = inside_attribute->info;

                            bytecode->verifier_info->frame_count = stackmap->entries_count;
                            FAIL_SET_JUMP((bytecode->verifier_info->frames = bumper_calloc(&s_permament_arena, bytecode->verifier_info->frame_count, sizeof(*bytecode->verifier_info->frames))), err, JERR_OOM, exit);

                            for(unsigned i = 0; i < bytecode->verifier_info->frame_count; i++){
                                JStackMapFrame_t* frame_original = &stackmap->entries[i];
                                BytecodeVerifierFrame_t* frame = &bytecode->verifier_info->frames[i];

                                frame->locals_count = frame_original->locals_count;
                                frame->stack_size = frame->stack_size;
                                
                                FAIL_SET_JUMP((frame->locals = bumper_calloc(&s_permament_arena, frame->locals_count, sizeof(*frame->locals))), err, JERR_OOM, exit);
                                FAIL_SET_JUMP((frame->stack = bumper_calloc(&s_permament_arena, frame->stack_size, sizeof(*frame->stack))), err, JERR_OOM, exit);

                                for(unsigned j = 0; j < frame->locals_count; j++){
                                    frame->locals[j].ctx = frame_original->locals[j].ctx;
                                    frame->locals[j].type = frame_original->locals[j].type;
                                }

                                for(unsigned j = 0; j < frame->stack_size; j++){
                                    frame->stack[j].ctx = frame_original->stack[j].ctx;
                                    frame->stack[j].type = frame_original->stack[j].type;
                                }
                            }

                            break;
                        }
                    }

                    bytecode->max_locals = code->max_locals;
                    bytecode->max_stack = code->max_stack;
                    bytecode->code_length = code->code_length;
                    bytecode->code = bumper_alloc(&s_permament_arena, bytecode->code_length);
                    FAIL_SET_JUMP(bytecode->code, err, JERR_OOM, exit);

                    memcpy(bytecode->code, code->code, bytecode->code_length);

                    bytecode->exception_count = code->exception_table_length;
                    bytecode->exceptions = bumper_calloc(&s_permament_arena, bytecode->exception_count, sizeof(*bytecode->exceptions));
                    FAIL_SET_JUMP(bytecode->exceptions, err, JERR_OOM, exit);

                    for(unsigned i = 0; i < bytecode->exception_count; i++){
                        MethodExceptionHandler_t* exception = &bytecode->exceptions[i];
                        typeof(code->exception_table[i])* raw_exception = &code->exception_table[i];

                        exception->start_pc = raw_exception->start_pc;
                        exception->end_pc = raw_exception->end_pc;
                        exception->handler_pc = raw_exception->handler_pc;
                        exception->type = raw_exception->catch_type == 0 ? NULL : &this_class->symtab.symbols[find_patched_symbol_index(raw_exception->catch_type, &metadata->cp_patch_list)];
                    }

                    method->code = bytecode;
                    patch_bytecode(this_class, method->code);
                    break;
                }
            }
        }
    }

    FAIL_SET_JUMP(class_insert(this_class) == 0, err, JERR_OOM, exit);
    *out = this_class;

exit:
    return err;
}

static Method_t* class_find_vtable_method(Class_t* class, uint16_t name_id){
    for(unsigned i = 0; i < class->vtable_size; i++){
        Method_t* method = class->vtable[i];
        if(method->name_id == name_id)
            return method;
    }

    return NULL;
}

Method_t* class_find_method(Class_t* class, uint16_t name_id){
    for(unsigned i = 0; i < class->methods.count; i++){
        Method_t* method = &class->methods.methods[i];
        if(method->name_id == name_id)
            return method;
    }

    return NULL;
}


//Hack but should work well with class_fix_hierarchy interface resolution logic
static Error_t class_find_imethod(Class_t* class, uint16_t name_id, uint16_t* index_out){
    unsigned vindex = 0;
    for(unsigned i = 0; i < class->methods.count; i++){
        Method_t* method = &class->methods.methods[i];
        if(method->flags.is_virtual){
            vindex++;
            if(method->name_id == name_id){
                *index_out = vindex;
                return JERR_OK;
            }
        }
    }

    return JERR_NOTFOUND;
}

static Field_t* class_find_static_field(Class_t* class, uint16_t name_id){
    for(unsigned i = 0; i < class->static_fields.count; i++){
        Field_t* field = &class->static_fields.fields[i];
        if(field->name_id == name_id)
            return field;
    }
    return NULL;
}

static Field_t* class_find_instance_field(Class_t* class, uint16_t name_id){
    for(unsigned i = 0; i < class->instance_fields.count; i++){
        Field_t* field = &class->instance_fields.fields[i];
        if(field->name_id == name_id)
            return field;
    }
    return NULL;
}

inline Error_t class_resolv_symbol(ClassSymbol_t* symbol){
    Error_t err = JERR_OK;

    if(symbol->type < PROXY_SYMBOL_CLASS) return JERR_OK;

    ClassProxySymbol_t* proxy_symbol = symbol->value;
    
    Class_t* origin = NULL;
    FAIL_SET_JUMP((err = class_load_bynameid(proxy_symbol->origin_name_id, &origin)) == JERR_OK,err,err,exit);

    switch(symbol->type){
        case PROXY_SYMBOL_CLASS:{
            symbol->type = SYMBOL_CLASS;
            symbol->value = origin;
        }
        break;

        case PROXY_SYMBOL_FIELD:{
            symbol->type = SYMBOL_FIELD;
            symbol->value = (symbol->value = class_find_static_field(origin, proxy_symbol->self_name_id)) ? 
                            symbol->value : class_find_instance_field(origin, proxy_symbol->self_name_id);
            FAIL_SET_JUMP(symbol->value, err, JERR_NOTFOUND, exit);
        }
        break;

        case PROXY_SYMBOL_METHOD:{
            symbol->type = SYMBOL_METHOD;
            FAIL_SET_JUMP((symbol->value = class_find_method(origin, proxy_symbol->self_name_id)), err, JERR_NOTFOUND, exit);
        }
        break;

        case PROXY_SYMBOL_STRING:{
            assert(0 && "TODO: string support!");
        }
        break;

        default: break;
    }

exit:
    return err;
}

static unsigned count_instance_methods(Class_t* class){
    unsigned count = 0;
    for(unsigned i = 0; i < class->methods.count; i++){
        count += class->methods.methods[i].flags.is_virtual ? 1 : 0;
    }
    return count;
}

static Error_t class_fix_hierarchy(Class_t* this_class){
    Error_t err = JERR_OK;

    Class_t* parent = this_class->parent;
    ClassLinkTimeMetadata_t* metadata = this_class->metadata;

    FAIL_SET_JUMP(!parent || !parent->flags.is_final, err, JERR_TYPECHECK_FAILURE, exit);

    //Fix field offsets
    this_class->object_size += parent ? parent->object_size : 0;
    size_t field_offset_fixup = parent ? (parent->object_size / sizeof(int32_t)) : 0;
    for(unsigned i = 0; i < this_class->instance_fields.count; i++){
        this_class->instance_fields.fields[i].offset += field_offset_fixup;
    }

    //Generate vtable
    if(!this_class->flags.is_interface){
        this_class->vtable_size = parent ? parent->vtable_size : 0;
        for(unsigned i = 0; i < this_class->methods.count; i++){
            Method_t* method = &this_class->methods.methods[i];
            if(method->flags.is_virtual){
                this_class->vtable_size += parent ? class_find_vtable_method(parent, method->name_id) == NULL : 1;
            }
        }

        this_class->vtable = bumper_calloc(&s_permament_arena, this_class->vtable_size, sizeof(*this_class->vtable));
        FAIL_SET_JUMP(this_class->vtable, err, JERR_OOM, exit);

        unsigned vtable_index = parent ? parent->vtable_size : 0;
        if(parent) memcpy(this_class->vtable, parent->vtable, sizeof(*this_class->vtable) * vtable_index);

        for(unsigned i = 0; i < this_class->methods.count; i++){
            Method_t* method = &this_class->methods.methods[i];
            if(method->flags.is_virtual){
                Method_t* overriden = parent ? class_find_vtable_method(parent, method->name_id) : NULL;
                if(overriden){
                    method->vtable_index = overriden->vtable_index;
                    this_class->vtable[method->vtable_index] = method;
                } else {
                    method->vtable_index = vtable_index++;
                    this_class->vtable[method->vtable_index] = method;
                }
            }
        }
    } else this_class->vtable_size = 0;

    this_class->flags.is_linked = 1;


    //Finally fix interfaces
    for(unsigned i = 0; i < this_class->implements.count; i++){
        Implementation_t* implementation = &this_class->implements.implementations[i];
        FAIL_SET_JUMP((err = class_load_bynameid(metadata->implements[i], &implementation->interface)) == JERR_OK, err, err, exit);
        Class_t* interface = implementation->interface;

        FAIL_SET_JUMP(interface->flags.is_interface, err, JERR_BADPARAM, exit);

        implementation->methods_count = count_instance_methods(interface);
        FAIL_SET_JUMP((implementation->methods = bumper_calloc(&s_permament_arena, implementation->methods_count, sizeof(*implementation->methods))), err, JERR_OOM, exit);

        unsigned iindex = 0;
        for(unsigned j = 0; j < interface->methods.count; j++){
            Method_t* imethod = &interface->methods.methods[j];
            if(imethod->flags.is_virtual){
                FAIL_SET_JUMP((implementation->methods[iindex] = class_find_vtable_method(this_class, imethod->name_id)), err, JERR_NOTFOUND, exit);
                imethod->interface_index = iindex;
                iindex++;
            }
        }
    }

    this_class->metadata = NULL;

    Method_t* clinit = class_find_method(this_class, stringpool_add("<clinit>@()V"));
    FAIL_SET_JUMP(!clinit || (err = java_method_invoke(clinit, NULL, NULL)) == JERR_OK, err, err, exit);

    Class_t* jlClass = NULL;
    FAIL_SET_JUMP((err = class_load_bynameid(stringpool_add("java/lang/Class"),  &jlClass)) == JERR_OK, err, err, exit);
    FAIL_SET_JUMP((err = heap_class_object_alloc(jlClass, &this_class->class_object)) == JERR_OK, err, err, exit);

exit:
    return 0;
}

static Error_t class_link(Class_t* class){
    static unsigned deepness = 0;
    Error_t err = JERR_OK;
    LIST_HEAD(hierarchy_list);

    deepness++;

    Class_t* cur_class = class;
    while(cur_class){
        INIT_LIST_HEAD(&cur_class->hierarchy_list);
        list_add(&cur_class->hierarchy_list, &hierarchy_list);
        if(cur_class->flags.is_linked) break;

        ClassLinkTimeMetadata_t* metadata = cur_class->metadata;
        if(!metadata->is_root && !(cur_class->parent = class_find(metadata->parent_name_id))){
            //Using raw API to make it work properly
            FAIL_SET_JUMP((err = class_convert_from_raw(loader_load_class(stringpool_get(metadata->parent_name_id)), &cur_class->parent)) == JERR_OK, err, err, exit);
            cur_class = cur_class->parent;
        } else break;
    }

    Class_t* linking_class = NULL;
    list_for_each_entry(linking_class, &hierarchy_list, hierarchy_list){
        if(!linking_class->flags.is_linked)
            FAIL_SET_JUMP((err = class_fix_hierarchy(linking_class)) == JERR_OK, err, err, exit);
    }

exit:
    if(--deepness == 0)
        bumper_reset(&s_temporary_arena);
    return err;
}

bool class_is_compatible(Class_t* class, Class_t* compatible_to){
    for(Class_t* cur = class; cur; cur = cur->parent){
        if(compatible_to == cur)
            return true;

        for(unsigned i = 0; i < cur->implements.count; i++){
            if(class_is_compatible(cur->implements.implementations[i].interface, compatible_to))
                return true;
        }
    }
    return false;
}