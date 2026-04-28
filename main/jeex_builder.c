#include "jeex_builder.h"
#include "bumper.h"
#include "class.h"
#include "jeex.h"
#include "jerror.h"
#include "linker.h"
#include "list.h"
#include "loader.h"

#include <string.h>
#include <assert.h>

JError_t JEEXBuilder_init(JEEXBuilder_t* builder, JLinker_t* linker ,bump_allocator_t* arena){
    JError_t err = JERR_OK;

    builder->linker = linker;
    builder->arena = arena;

    builder->jeex = bumper_calloc(arena,1,sizeof(*builder->jeex));
    FAIL_SET_JUMP(builder->jeex,err,JERR_OOM,exit);

    builder->jeex->id_table_length = linker->linker_global_data.max_ID;

    FAIL_SET_JUMP((builder->jeex->id_table = bumper_calloc(arena,builder->jeex->id_table_length, sizeof(*builder->jeex->id_table))),err,JERR_OOM,exit);

exit:
    return err;
}


//Create classes, allocates fields, puts childrens.
static JError_t create_classes(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    JEEXClass_t* jeex_class = bumper_calloc(builder->arena,1,sizeof(*jeex_class));
    FAIL_SET_JUMP(jeex_class,err,JERR_OOM,exit);

    builder->jeex->id_table[class->ID].element = jeex_class;
    builder->jeex->id_table[class->ID].type = EJEEXID_CLASS;

    unsigned children_count = 0;
    JClass_t* child = NULL;
    list_for_each_entry(child, &class->children, as_child){
        FAIL_SET_JUMP(create_classes(child, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
        children_count++;
    }

    if(class->parent){
        jeex_class->parent = builder->jeex->id_table[class->parent->ID].element;
        assert(jeex_class->parent); //I guarantee there will be parent!
        assert(builder->jeex->id_table[class->parent->ID].type == EJEEXID_CLASS); //If this fail we are DEEPLY fucked!
    }

    jeex_class->children_count = children_count;
    jeex_class->children = bumper_calloc(builder->arena,jeex_class->children_count,sizeof(*jeex_class->children));
    FAIL_SET_JUMP(jeex_class->children,err,JERR_OOM,exit);

    unsigned ci = 0;
    list_for_each_entry(child, &class->children, as_child){
        assert(builder->jeex->id_table[child->ID].type == EJEEXID_CLASS);
        assert((jeex_class->children[ci++] = builder->jeex->id_table[child->ID].element));
    }

    jeex_class->implements_count = class->interfaces.count;
    jeex_class->implements = bumper_calloc(builder->arena,jeex_class->implements_count,sizeof(*jeex_class->implements));
    FAIL_SET_JUMP(jeex_class->implements,err,JERR_OOM,exit);

    jeex_class->object_size = class->ifields_size;

    jeex_class->symtab_length = class->symtab.length;
    jeex_class->symtab = bumper_calloc(builder->arena,jeex_class->symtab_length,sizeof(*jeex_class->symtab));
    FAIL_SET_JUMP(jeex_class->symtab,err,JERR_OOM,exit);

    //jeex_class->ID = class->ID;

    jeex_class->static_methods.count = ((JLinkerMetadata_t*)class->metadata)->methods_count[1]; //We need static methods count
    jeex_class->static_methods.methods = bumper_calloc(builder->arena,jeex_class->static_methods.count,sizeof(*jeex_class->static_methods.methods));
    FAIL_SET_JUMP(jeex_class->static_methods.methods,err,JERR_OOM,exit);    

    jeex_class->vtable.count = class->vtable.count;
    jeex_class->vtable.methods = bumper_calloc(builder->arena,jeex_class->vtable.count,sizeof(*jeex_class->vtable.methods));
    FAIL_SET_JUMP(jeex_class->vtable.methods,err,JERR_OOM,exit);

    jeex_class->fields[0].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[0];
    jeex_class->fields[1].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[1];

    FAIL_SET_JUMP((jeex_class->fields[0].fields = bumper_calloc(builder->arena,jeex_class->fields[0].count,sizeof(*jeex_class->fields[0].fields))),err,JERR_OOM,exit);
    FAIL_SET_JUMP((jeex_class->fields[1].fields = bumper_calloc(builder->arena,jeex_class->fields[1].count,sizeof(*jeex_class->fields[1].fields))),err,JERR_OOM,exit);

    jeex_class->name = class->name;
    FAIL_SET_JUMP(jeex_class->name,err,JERR_OOM,exit);

exit:
    return err;
}

//Puts implements, initialise methods and fields
static JError_t finalize_classes(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    assert(builder->jeex->id_table[class->ID].type == EJEEXID_CLASS);
    JEEXClass_t* jeex_class = builder->jeex->id_table[class->ID].element;
    JLinkerMetadata_t* class_meta = class->metadata;

    unsigned fi[2] = {0};
    for(unsigned i = 0; i < class_meta->fields.count; i++){
        JField_t* field = (void*)class_meta->fields.elements[i];
        JEEXField_t* jeex_field = bumper_calloc(builder->arena,1,sizeof(*jeex_field));
        FAIL_SET_JUMP(jeex_field,err,JERR_OOM,exit);

        //jeex_field->ID = field->ID;
        jeex_field->offset = field->offset;
        jeex_field->type = field->type;
        jeex_field->initialiser = field->constvalue;

        jeex_class->fields[field->flags.is_static].fields[fi[field->flags.is_static]++] = jeex_field;

        builder->jeex->id_table[field->ID].type = EJEEXID_FIELD;
        builder->jeex->id_table[field->ID].element = jeex_field;
    }

    for(unsigned i = 0; i < class_meta->methods.count; i++){
        JMethod_t* method = (void*)class_meta->methods.elements[i];
        JEEXMethod_t* jeex_method = bumper_calloc(builder->arena,1,sizeof(*jeex_method));
        FAIL_SET_JUMP(jeex_method,err,JERR_OOM,exit);

        jeex_method->owner = jeex_class;
        jeex_method->flags.is_native = method->flags.is_native;
        jeex_method->flags.is_static = method->flags.is_static;

        jeex_method->mangled_name = method->name;
        FAIL_SET_JUMP(jeex_method->mangled_name,err,JERR_OOM,exit);

        //jeex_method->ID = method->ID;
        
        if(jeex_method->flags.is_native){
            jeex_method->code.native_id = method->code.native_id;
        } else {
            jeex_method->code.bytecode = bumper_calloc(builder->arena, 1, sizeof(*jeex_method->code.bytecode));
            FAIL_SET_JUMP(jeex_method->code.bytecode,err,JERR_OOM,exit);

            jeex_method->code.bytecode->exception_count = method->code.bytecode->exception_table_length;
            jeex_method->code.bytecode->exceptions = bumper_calloc(builder->arena,jeex_method->code.bytecode->exception_count,sizeof(*jeex_method->code.bytecode->exceptions));
            FAIL_SET_JUMP(jeex_method->code.bytecode->exceptions,err,JERR_OOM,exit);

            jeex_method->code.bytecode->code_length = method->code.bytecode->code_length;
            jeex_method->code.bytecode->locals_count = method->code.bytecode->max_locals;
            jeex_method->code.bytecode->stack_size = method->code.bytecode->max_stack;
            jeex_method->code.bytecode->code_length = method->code.bytecode->code_length; //Code length will be the same between original class and JEEX
            jeex_method->code.bytecode->code = method->code.bytecode->code;
        }

        jeex_method->prototype.return_type = method->prototype.return_type;
        jeex_method->prototype.arguments_count = method->prototype.arguments_count;

        jeex_method->prototype.arguments_types = bumper_calloc(builder->arena,jeex_method->prototype.arguments_count, sizeof(*jeex_method->prototype.arguments_types));
        FAIL_SET_JUMP(jeex_method->prototype.arguments_types,err,JERR_OOM,exit);

        for(unsigned i = 0; i < jeex_method->prototype.arguments_count; i++){
            jeex_method->prototype.arguments_types[i] = method->prototype.argument_types[i];
        }
        
        builder->jeex->id_table[method->ID].type = EJEEXID_METHOD;
        builder->jeex->id_table[method->ID].element = jeex_method;
    }

    for(unsigned i = 0; i < class->vtable.count; i++){
        assert(builder->jeex->id_table[class->vtable.methods[i]->ID].type == EJEEXID_METHOD);
        assert((jeex_class->vtable.methods[i] = builder->jeex->id_table[class->vtable.methods[i]->ID].element));
    }

    for(unsigned i = 0; i < class->interfaces.count; i++){
        assert(builder->jeex->id_table[class->interfaces.implement[i]->ID].type == EJEEXID_CLASS);
        jeex_class->implements[i] = builder->jeex->id_table[class->interfaces.implement[i]->ID].element;
    }

    unsigned smi = 0; //Static Method Index
    for(unsigned i = 0; i < class_meta->methods.count; i++){
        JMethod_t* method = (void*)class_meta->methods.elements[i];
        if(method->flags.is_static){
            assert(builder->jeex->id_table[method->ID].type == EJEEXID_METHOD);
            jeex_class->static_methods.methods[smi++] = builder->jeex->id_table[method->ID].element;
        }
    }

    JClass_t* child = NULL;
    list_for_each_entry(child, &class->children, as_child){
        FAIL_SET_JUMP(finalize_classes(child, builder) == JERR_OK, err, JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

static JError_t init_symtabs(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    assert(builder->jeex->id_table[class->ID].type == EJEEXID_CLASS);
    JEEXClass_t* jeex_class = builder->jeex->id_table[class->ID].element;

    for(unsigned i = 0; i < class->symtab.length; i++){
        JClassSymbol_t* sym = &class->symtab.symbols[i];
        jeex_class->symtab[i].type = sym->symbol_type; //thoose enums are compatible

        switch(sym->symbol_type){
            case EJRCT_CLASS:{
                assert(builder->jeex->id_table[((JClass_t*)sym->value)->ID].type == EJEEXID_CLASS);
                jeex_class->symtab[i].value = builder->jeex->id_table[((JClass_t*)sym->value)->ID].element;
            }
            break;

            case EJRCT_INTERFACEMETHODREF:
            case EJRCT_METHOD:{
                assert(builder->jeex->id_table[((JMethod_t*)sym->value)->ID].type == EJEEXID_METHOD);
                jeex_class->symtab[i].value = builder->jeex->id_table[((JMethod_t*)sym->value)->ID].element;
            }
            break;

            case EJRCT_FIELD:{
                assert(builder->jeex->id_table[((JField_t*)sym->value)->ID].type == EJEEXID_FIELD);
                jeex_class->symtab[i].value = builder->jeex->id_table[((JField_t*)sym->value)->ID].element;
            }
            break;

            case EJRCT_INT:
            case EJRCT_LONG:
            case EJRCT_FLOAT:
            case EJRCT_DOUBLE:{
                unsigned sz = (sym->symbol_type == EJRCT_LONG || sym->symbol_type == EJRCT_DOUBLE) ? sizeof(uint64_t) : sizeof(uint32_t);
                
                jeex_class->symtab[i].type = sym->symbol_type;
                jeex_class->symtab[i].value = bumper_calloc(builder->arena,1,sz);
                FAIL_SET_JUMP(jeex_class->symtab[i].value,err,JERR_OOM,exit);

                memcpy(jeex_class->symtab[i].value,sym->value,sz);
            }
            break;

            case EJRCT_STRING:{
                JRawUTF8_t* utf8_org = sym->value;
                JEEXRawUTF8_t* utf8_new = bumper_calloc(builder->arena,1,sizeof(*utf8_new));
                FAIL_SET_JUMP(utf8_new,err,JERR_OOM,exit);

                utf8_new->length = utf8_org->length;
                utf8_new->string = bumper_calloc(builder->arena,utf8_new->length,1);
                FAIL_SET_JUMP(utf8_new->string,err,JERR_OOM,exit);

                memcpy(utf8_new->string,utf8_org->string,utf8_org->length);
                
                jeex_class->symtab[i].type = sym->symbol_type;
                jeex_class->symtab[i].value = utf8_new;
            }
            break;
        }
    }

    JClass_t* child = NULL;
    list_for_each_entry(child, &class->children, as_child){
        FAIL_SET_JUMP(init_symtabs(child, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

static JError_t fix_exceptions(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;
    JLinkerMetadata_t* class_meta = class->metadata;

    for(unsigned i = 0; i < class_meta->methods.count; i++){
        JMethod_t* method = (void*)class_meta->methods.elements[i];
        if(!method->flags.is_native){
            JEEXMethod_t* jeex_method = builder->jeex->id_table[method->ID].element;
            JCodeAttribute_t* java_code = method->code.bytecode;
            JEEXMethodBytecode_t* jeex_code = jeex_method->code.bytecode;
            
            for(unsigned i = 0; i < java_code->exception_table_length; i++){
                typeof(java_code->exception_table) exception = &java_code->exception_table[i];
                JEEXBytecodeException_t* jeex_exception = &jeex_code->exceptions[i];


                if(exception->catch_type != 0){
                    JClassSymbol_t* catch_sym = JClassSymtab_get_symbol(&class->symtab, exception->catch_type);

                    assert(catch_sym);
                    assert(catch_sym->symbol_type == EJRCT_CLASS);

                    JClass_t* catch = catch_sym->value;

                    jeex_exception->type = builder->jeex->id_table[catch->ID].element;
                }

                jeex_exception->start_pc = exception->start_pc;
                jeex_exception->end_pc = exception->end_pc;
                jeex_exception->handler_pc = exception->handler_pc;
            }
        }
    }

    JClass_t* child = NULL;
    list_for_each_entry(child, &class->children, as_child){
        FAIL_SET_JUMP(fix_exceptions(child, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

JError_t JEEXBuilder_build(JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    JClass_t* root = NULL;
    list_for_each_entry(root, &builder->linker->root_list, as_child){
        FAIL_SET_JUMP(create_classes(root, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    list_for_each_entry(root, &builder->linker->root_list, as_child){
        FAIL_SET_JUMP(finalize_classes(root, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    list_for_each_entry(root, &builder->linker->root_list, as_child){
        FAIL_SET_JUMP(init_symtabs(root, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    list_for_each_entry(root, &builder->linker->root_list, as_child){
        FAIL_SET_JUMP(fix_exceptions(root, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
    }


    builder->jeex->static_fields_size = builder->linker->linker_global_data.sfield_curoffset;

exit:
    return err;
}

JEEXClass_t* JEEXClass_get(JEEXHeader_t* jeex, char* name){
    for(unsigned i = 0; i < jeex->id_table_length; i++){
        if(jeex->id_table[i].type == EJEEXID_CLASS){
            JEEXClass_t* class = jeex->id_table[i].element;
            if(strcmp(class->name, name) == 0)
                return class;
        }
    }
    return NULL;
}

JEEXMethod_t* JEEXMethod_get(JEEXClass_t* class, char* mangled_name){
    for(unsigned i = 0; i < class->static_methods.count; i++){
        JEEXMethod_t* method = class->static_methods.methods[i];
        if(strcmp(method->mangled_name, mangled_name) == 0)
            return method;
    }
    return NULL;
}