#include "config.h"

#include "class.h"
#include "jerror.h"
#include "list.h"
#include "loader.h"
#include "parser.h"
#include "bumper.h"
#include "stringpool.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bump_allocator_t s_permament_arena = {0}, s_temporary_arena = {0};
static ClassTable_t s_class_table = {0};

#define JVM_CLASSPATH "java_src"

void classes_init(){
    assert(bumper_create(&s_permament_arena, CLASS_PERMAMENT_ARENA) == 0);
    assert(bumper_create(&s_temporary_arena, CLASS_TEMPOPARY_ARENA) == 0);
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

    s_class_table.classes[index.index] = class;

exit:
    return err;
}

static unsigned class_field_sizeof(JavaFieldType_t type){
    return type == TYPE_VOID ? 0 : type == TYPE_LONG || type == TYPE_DOUBLE ? sizeof(uint64_t) : sizeof(uint32_t);
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
                    case EJCT_METHODREF:
                        symbol->type = PROXY_SYMBOL_METHOD;
                        break;
                    case EJCT_INTERFACE_METHODREF:
                        symbol->type = PROXY_SYMBOL_IMETHOD;
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
            case EJOPCODE_MULTIANEWARRAY:
            case EJOPCODE_NEW:
            case EJOPCODE_LDC_W:
            case EJOPCODE_LDC2_W:
            case EJOPCODE_INVOKESPECIAL:
            case EJOPCODE_INVOKESTATIC:
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

            case EJOPCODE_INVOKEVIRTUAL:{
                *((uint16_t*)(opcode + 1)) = cpu_to_be16(find_patched_symbol_index(be16_to_cpu(*(uint16_t*)(opcode + 1)), &metadata->cp_patch_list));
            }
            break;
        }
    }
}

static Error_t class_convert_from_raw(JRawClass_t* parsed_class, Class_t** out){
    Error_t err = JERR_OK;
    FAIL_SET_JUMP(parsed_class && out, err, JERR_BADPARAM, exit);

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


    INIT_LIST_HEAD(&this_class->list);
    this_class->metadata = metadata;
    this_class->name_id = this_name_id;
    this_class->implements.count = metadata->implements_count;
    this_class->implements.implements = bumper_calloc(&s_permament_arena, this_class->implements.count, sizeof(*this_class->implements.implements));
    FAIL_SET_JUMP(this_class->implements.implements, err, JERR_OOM, exit);

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

        field->type = raw_field_descriptor_utf8->string[0];
        field->offset = offsets[is_static];
        field->size = class_field_sizeof(field->type);
        offsets[is_static] += field->size;
        
        int name_id = stringpool_add((char*)raw_field_name_utf8->string);
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
    this_class->static_fields_storage = bumper_calloc(&s_permament_arena, 1, offsets[1]);
    FAIL_SET_JUMP(this_class->static_fields_storage, err, JERR_OOM, exit);

    this_class->object_size = offsets[0]; //Will will use this also on linker stage to calculate proper offsets

    size_t special_count = 0;
    size_t methods_count[2] = {0};
    for(unsigned i = 0; i < parsed_class->methods_count; i++){
        JRawMethod_t* raw_method = &parsed_class->methods[i];
        if((raw_method->flags & ACC_PRIVATE) != ACC_PRIVATE && strcmp((char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, raw_method->name_index)->value)->string, "<init>") != 0){
            methods_count[(raw_method->flags & ACC_STATIC) == ACC_STATIC]++;
        } else special_count++;
    }

    this_class->instance_methods.count = methods_count[0];
    this_class->static_methods.count = methods_count[1];
    this_class->special_methods.count = special_count; 


    this_class->special_methods.methods = bumper_calloc(&s_permament_arena, this_class->special_methods.count, sizeof(*this_class->special_methods.methods));
    this_class->instance_methods.methods = bumper_calloc(&s_permament_arena, this_class->instance_methods.count, sizeof(*this_class->instance_methods.methods));
    this_class->static_methods.methods = bumper_calloc(&s_permament_arena, this_class->static_methods.count, sizeof(*this_class->static_methods.methods));
    FAIL_SET_JUMP(this_class->instance_methods.methods && this_class->static_methods.methods && this_class->special_methods.methods, err, JERR_OOM, exit);

    unsigned special_index = 0;
    unsigned method_index[3] = {0};
    for(unsigned i = 0; i < parsed_class->methods_count; i++){
        JRawMethod_t* raw_method = &parsed_class->methods[i];
        bool is_static = (raw_method->flags & ACC_STATIC) == ACC_STATIC;
        bool is_special = (raw_method->flags & ACC_PRIVATE) == ACC_PRIVATE || strcmp((char*)((JRawUTF8_t*)parser_constantpool_get(constantpool, raw_method->name_index)->value)->string, "<init>") == 0;

        Method_t* method_array = is_static ? this_class->static_methods.methods : is_special ? this_class->special_methods.methods : this_class->instance_methods.methods;
        Method_t* method = &method_array[is_special ? special_index++ : method_index[is_static]++];

        method->flags.is_native = (raw_method->flags & ACC_NATIVE) == ACC_NATIVE;
        
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
        method->return_size = class_field_sizeof(raw_method_descriptor_cstr[0]);
        method->arguments_size = 0; //TODO
        
        if(method->flags.is_native){
            printf("TODO: native method lookup\n");
        } else {
            JRawAttribute_t* attribute = NULL;
            list_for_each_entry(attribute, &raw_method->attributes, list){
                if(attribute->type == EJAT_CODE){
                    JCodeAttribute_t* code = attribute->info;
                    MethodBytecode_t* bytecode = bumper_calloc(&s_permament_arena, 1, sizeof(*bytecode));
                    FAIL_SET_JUMP(bytecode, err, JERR_OOM, exit);

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

Error_t class_link(Class_t* class);
Error_t class_load_bynameid(uint16_t name_id, Class_t** out){
    Error_t err = JERR_OK;
    assert(out);

    if((*out = class_find(name_id))) return JERR_OK;

    FAIL_SET_JUMP((err = class_convert_from_raw(loader_load_class(stringpool_get(name_id)), out)) == JERR_OK, err, err, exit);
    FAIL_SET_JUMP((err = class_link(*out)) == JERR_OK, err, err, exit);

exit:
    return err;
}

Method_t* class_find_static_method(Class_t* class, uint16_t name_id){
    MethodTable_t* table = &class->static_methods;
    for(unsigned i = 0; i < table->count; i++){
        Method_t* method = &table->methods[i];
        if(method->name_id == name_id)
            return method;
    }

    return NULL;
}

Method_t* class_find_special_method(Class_t* class, uint16_t name_id){
    MethodTable_t* table = &class->special_methods;
    for(unsigned i = 0; i < table->count; i++){
        Method_t* method = &table->methods[i];
        if(method->name_id == name_id)
            return method;
    }

    return NULL;
}

Method_t* class_find_virtual_method(Class_t* class, uint16_t name_id){
    for(unsigned i = 0; i < class->vtable_size; i++){
        Method_t* method = class->vtable[i];
        if(method->name_id == name_id)
            return method;
    }

    return NULL;
}

static unsigned count_methods(Class_t* class, bool is_static){
    return is_static ? class->static_methods.count : class->instance_methods.count;
}

static Error_t class_fix_hierarchy(Class_t* this_class){
    Error_t err = JERR_OK;

    Class_t* parent = this_class->parent;
    ClassLinkTimeMetadata_t* metadata = this_class->metadata;

    //Fix field offsets
    this_class->object_size += parent ? parent->object_size : 0;
    for(unsigned i = 0; i < this_class->instance_fields.count; i++){
        Field_t* field = &this_class->instance_fields.fields[i];
        field->offset += parent ? parent->object_size : 0;
    }

    //Generate vtable
    this_class->vtable_size = parent ? parent->vtable_size : 0;
    for(unsigned i = 0; i < this_class->instance_methods.count; i++){
        this_class->vtable_size += parent ? class_find_virtual_method(parent, this_class->instance_methods.methods[i].name_id) == NULL : 1;
    }

    this_class->vtable = bumper_calloc(&s_permament_arena, this_class->vtable_size, sizeof(*this_class->vtable));
    FAIL_SET_JUMP(this_class->vtable, err, JERR_OOM, exit);

    unsigned vtable_index = parent ? parent->vtable_size : 0;
    if(parent) memcpy(this_class->vtable, parent->vtable, sizeof(*this_class->vtable) * vtable_index);

    for(unsigned i = 0; i < this_class->instance_methods.count; i++){
        Method_t* method = &this_class->instance_methods.methods[i];
        Method_t* overriden = parent ? class_find_virtual_method(parent, method->name_id) : NULL;
        if(overriden){
            method->vtable_index = overriden->vtable_index;
            this_class->vtable[method->vtable_index] = method;
        } else {
            method->vtable_index = vtable_index++;
            this_class->vtable[method->vtable_index] = method;
        }
    }
    this_class->flags.is_linked = 1;


    //Finally fix interfaces
    for(unsigned i = 0; i < this_class->implements.count; i++){
        FAIL_SET_JUMP((err = class_load_bynameid(metadata->implements[i], &this_class->implements.implements[i])) == JERR_OK, err, err, exit);
    }

    this_class->metadata = NULL;

exit:
    return 0;
}

Error_t class_link(Class_t* class){
    static unsigned deepness = 0;
    Error_t err = JERR_OK;
    LIST_HEAD(hierarchy_list);

    deepness++;

    Class_t* cur_class = class;
    while(cur_class){
        list_add(&cur_class->list, &hierarchy_list);
        if(cur_class->flags.is_linked) break;

        ClassLinkTimeMetadata_t* metadata = cur_class->metadata;
        if(!metadata->is_root && !(cur_class->parent = class_find(metadata->parent_name_id))){
            FAIL_SET_JUMP((err = class_convert_from_raw(loader_load_class(stringpool_get(metadata->parent_name_id)), &cur_class->parent)) == JERR_OK, err, err, exit);
            cur_class = cur_class->parent;
        } else break;
    }

    Class_t* linking_class = NULL;
    list_for_each_entry(linking_class, &hierarchy_list, list){
        if(!linking_class->flags.is_linked)
            FAIL_SET_JUMP((err = class_fix_hierarchy(linking_class)) == JERR_OK, err, err, exit);
    }

exit:
    if(--deepness == 0)
        bumper_reset(&s_temporary_arena);
    return err;
}