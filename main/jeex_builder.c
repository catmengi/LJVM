#include "jeex_builder.h"
#include "bumper.h"
#include "cfg.h"
#include "class.h"
#include "jeex.h"
#include "jerror.h"
#include "linker.h"
#include "list.h"
#include "loader.h"

#include <string.h>
#include <assert.h>

JError_t JEEX_create_builder(JEEXBuilder_t* builder, JLinker_t* linker ,bump_allocator_t* arena){
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

    jeex_class->ID = class->ID;

    jeex_class->vtable.count = class->vtable.count;
    jeex_class->vtable.methods = bumper_calloc(builder->arena,jeex_class->vtable.count,sizeof(*jeex_class->vtable.methods));
    FAIL_SET_JUMP(jeex_class->vtable.methods,err,JERR_OOM,exit);

    jeex_class->fields[0].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[0];
    jeex_class->fields[1].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[1];

    FAIL_SET_JUMP((jeex_class->fields[0].fields = bumper_calloc(builder->arena,jeex_class->fields[0].count,sizeof(*jeex_class->fields[0].fields))),err,JERR_OOM,exit);
    FAIL_SET_JUMP((jeex_class->fields[1].fields = bumper_calloc(builder->arena,jeex_class->fields[1].count,sizeof(*jeex_class->fields[1].fields))),err,JERR_OOM,exit);

    jeex_class->name = bumper_strdup(builder->arena,class->name);
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

        jeex_field->ID = field->ID;
        jeex_field->offset = field->offset;
        jeex_field->type = field->type;

        jeex_class->fields[field->flags.is_static].fields[fi[field->flags.is_static]++] = jeex_field;

        builder->jeex->id_table[field->ID].type = EJEEXID_FIELD;
        builder->jeex->id_table[field->ID].element = jeex_field;
    }

    for(unsigned i = 0; i < class_meta->methods.count; i++){
        JMethod_t* method = (void*)class_meta->methods.elements[i];
        JEEXMethod_t* jeex_method = bumper_calloc(builder->arena,1,sizeof(*jeex_method));
        FAIL_SET_JUMP(jeex_method,err,JERR_OOM,exit);

        jeex_method->flags.is_native = method->flags.is_native;
        jeex_method->flags.is_static = method->flags.is_static;

        jeex_method->mangled_name = bumper_strdup(builder->arena,method->name);
        FAIL_SET_JUMP(jeex_method->mangled_name,err,JERR_OOM,exit);

        jeex_method->ID = method->ID;
        
        if(jeex_method->flags.is_native){
            assert(0 && "TODO: find method in native table!");
            //FIND NATIVE ID OF METHOD!
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
                assert(builder->jeex->id_table[((JClass_t*)sym->value)->ID].type == EJEEXID_METHOD);
                jeex_class->symtab[i].value = builder->jeex->id_table[((JMethod_t*)sym->value)->ID].element;
            }
            break;

            case EJRCT_FIELD:{
                assert(builder->jeex->id_table[((JClass_t*)sym->value)->ID].type == EJEEXID_FIELD);
                jeex_class->symtab[i].value = builder->jeex->id_table[((JField_t*)sym->value)->ID].element;
            }
            break;

            case EJRCT_INT:
            case EJRCT_LONG:
            case EJRCT_FLOAT:
            case EJRCT_DOUBLE:{
                unsigned sz = (sym->symbol_type == EJRCT_LONG || sym->symbol_type == EJRCT_DOUBLE) ? sizeof(uint64_t) : sizeof(uint32_t);
                
                jeex_class->symtab[i].type = sz == sizeof(uint32_t) ? EJEEXID_32CONST : EJEEXID_64CONST;
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
                
                jeex_class->symtab[i].type = EJEEXID_STRING;
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

JError_t JEEX_build(JEEXBuilder_t* builder){
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


    builder->jeex->static_fields_size = builder->linker->linker_global_data.sfield_curoffset;

exit:
    return err;
}