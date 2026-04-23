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

JError_t JEEX_create_builder(JEEXBuilder_t* builder, JLinker_t* linker ,bump_allocator_t* arena){
    JError_t err = JERR_OK;

    builder->linker = linker;
    builder->output_arena = arena;

    builder->jeex_header = bumper_calloc(arena,1,sizeof(*builder->jeex_header));
    FAIL_SET_JUMP(builder->jeex_header,err,JERR_OOM,exit);

    builder->jeex_header->classes_count = linker->linker_global_data.ID_tracker[JFID_CLASS];
    builder->jeex_header->fields_count = linker->linker_global_data.ID_tracker[JFID_FIELD];
    builder->jeex_header->methods_count = linker->linker_global_data.ID_tracker[JFID_METHOD];

    FAIL_SET_JUMP((builder->jeex_header->class_table = bumper_calloc(arena,builder->jeex_header->classes_count, sizeof(*builder->jeex_header->class_table))),err,JERR_OOM,exit);
    FAIL_SET_JUMP((builder->jeex_header->method_table = bumper_calloc(arena,builder->jeex_header->methods_count, sizeof(*builder->jeex_header->method_table))),err,JERR_OOM,exit);
    FAIL_SET_JUMP((builder->jeex_header->field_table = bumper_calloc(arena,builder->jeex_header->fields_count, sizeof(*builder->jeex_header->field_table))),err,JERR_OOM,exit);

exit:
    return err;
}


//Create classes, allocates fields, puts childrens.
static JError_t create_classes(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    JEEXClass_t* jeex_class = bumper_calloc(builder->output_arena,1,sizeof(*jeex_class));
    FAIL_SET_JUMP(jeex_class,err,JERR_OOM,exit);

    builder->jeex_header->class_table[class->ID] = jeex_class;
    unsigned children_count = 0;
    JClass_t* child = NULL;
    list_for_each_entry(child, &class->children, as_child){
        FAIL_SET_JUMP(create_classes(child, builder) == JERR_OK,err,JERR_UNKNOWN,exit);
        children_count++;
    }

    if(class->parent){
        jeex_class->parent = builder->jeex_header->class_table[class->parent->ID];
        assert(jeex_class->parent); //I guarantee there will be parent!
    }

    jeex_class->children_count = children_count;
    jeex_class->children = bumper_calloc(builder->output_arena,jeex_class->children_count,sizeof(*jeex_class->children));
    FAIL_SET_JUMP(jeex_class->children,err,JERR_OOM,exit);

    unsigned ci = 0;
    list_for_each_entry(child, &class->children, as_child){
        assert((jeex_class->children[ci++] = builder->jeex_header->class_table[child->ID]));
    }

    jeex_class->implements_count = class->interfaces.count;
    jeex_class->implements = bumper_calloc(builder->output_arena,jeex_class->implements_count,sizeof(*jeex_class->implements));
    FAIL_SET_JUMP(jeex_class->implements,err,JERR_OOM,exit);

    jeex_class->object_size = class->ifields_size;

    jeex_class->symtab_length = class->symtab.length;
    jeex_class->symtab = bumper_calloc(builder->output_arena,jeex_class->symtab_length,sizeof(*jeex_class->symtab));
    FAIL_SET_JUMP(jeex_class->symtab,err,JERR_OOM,exit);

    jeex_class->ID = class->ID;

    jeex_class->vtable.count = class->vtable.count;
    jeex_class->vtable.methods = bumper_calloc(builder->output_arena,jeex_class->vtable.count,sizeof(*jeex_class->vtable.methods));
    FAIL_SET_JUMP(jeex_class->vtable.methods,err,JERR_OOM,exit);

    jeex_class->fields[0].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[0];
    jeex_class->fields[1].count = ((JLinkerMetadata_t*)class->metadata)->fields_count[1];

    FAIL_SET_JUMP((jeex_class->fields[0].fields = bumper_calloc(builder->output_arena,jeex_class->fields[0].count,sizeof(*jeex_class->fields[0].fields))),err,JERR_OOM,exit);
    FAIL_SET_JUMP((jeex_class->fields[1].fields = bumper_calloc(builder->output_arena,jeex_class->fields[1].count,sizeof(*jeex_class->fields[1].fields))),err,JERR_OOM,exit);

    jeex_class->name = bumper_strdup(builder->output_arena,class->name);
    FAIL_SET_JUMP(jeex_class->name,err,JERR_OOM,exit);

exit:
    return err;
}

//Puts implements, initialise methods and fields
static JError_t finalize_classes(JClass_t* class, JEEXBuilder_t* builder){
    JError_t err = JERR_OK;

    JEEXClass_t* jeex_class = builder->jeex_header->class_table[class->ID];
    JLinkerMetadata_t* class_meta = class->metadata;

    unsigned fi[2] = {0};
    for(unsigned i = 0; i < class_meta->fields.count; i++){
        JField_t* field = (void*)class_meta->fields.elements[i];
        JEEXField_t* jeex_field = bumper_calloc(builder->output_arena,1,sizeof(*jeex_field));
        FAIL_SET_JUMP(jeex_field,err,JERR_OOM,exit);

        jeex_field->ID = field->ID;
        jeex_field->offset = field->offset;
        jeex_field->type = field->type;

        jeex_class->fields[field->flags.is_static].fields[fi[field->flags.is_static]++] = jeex_field;
        builder->jeex_header->field_table[field->ID] = jeex_field;
    }

    for(unsigned i = 0; i < class_meta->methods.count; i++){
        JMethod_t* method = (void*)class_meta->methods.elements[i];
        JEEXMethod_t* jeex_method = bumper_calloc(builder->output_arena,1,sizeof(*jeex_method));
        FAIL_SET_JUMP(jeex_method,err,JERR_OOM,exit);

        jeex_method->flags.is_native = method->flags.is_native;
        jeex_method->flags.is_static = method->flags.is_static;

        jeex_method->mangled_name = bumper_strdup(builder->output_arena,method->name);
        FAIL_SET_JUMP(jeex_method->mangled_name,err,JERR_OOM,exit);

        jeex_method->ID = method->ID;
        
        if(jeex_method->flags.is_native){
            assert(0 && "TODO: find method in native table!");
            //FIND NATIVE ID OF METHOD!
        }

        jeex_method->prototype.return_type = method->prototype.return_type;
        jeex_method->prototype.arguments_count = method->prototype.arguments_count;

        jeex_method->prototype.arguments_types = bumper_calloc(builder->output_arena,jeex_method->prototype.arguments_count, sizeof(*jeex_method->prototype.arguments_types));
        FAIL_SET_JUMP(jeex_method->prototype.arguments_types,err,JERR_OOM,exit);

        for(unsigned i = 0; i < jeex_method->prototype.arguments_count; i++){
            jeex_method->prototype.arguments_types[i] = method->prototype.argument_types[i];
        }
        
        builder->jeex_header->method_table[method->ID] = jeex_method;
    }

    for(unsigned i = 0; i < class->vtable.count; i++){
        assert((jeex_class->vtable.methods[i] = builder->jeex_header->method_table[class->vtable.methods[i]->ID]));
    }

    for(unsigned i = 0; i < class->interfaces.count; i++){
        jeex_class->implements[i] = builder->jeex_header->class_table[class->interfaces.implement[i]->ID];
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

    JEEXClass_t* jeex_class = builder->jeex_header->class_table[class->ID];
    for(unsigned i = 0; i < class->symtab.length; i++){
        JClassSymbol_t* sym = &class->symtab.symbols[i];
        jeex_class->symtab[i].type = sym->symbol_type; //thoose enums are compatible

        switch(sym->symbol_type){
            case EJRCT_CLASS:{
                jeex_class->symtab[i].value = builder->jeex_header->class_table[((JClass_t*)sym->value)->ID];
            }
            break;

            case EJRCT_INTERFACEMETHODREF:
            case EJRCT_METHOD:{
                jeex_class->symtab[i].value = builder->jeex_header->method_table[((JMethod_t*)sym->value)->ID];
            }
            break;

            case EJRCT_FIELD:{
                jeex_class->symtab[i].value = builder->jeex_header->field_table[((JField_t*)sym->value)->ID];
            }
            break;

            case EJRCT_INT:
            case EJRCT_LONG:
            case EJRCT_FLOAT:
            case EJRCT_DOUBLE:{
                unsigned sz = (sym->symbol_type == EJRCT_LONG || sym->symbol_type == EJRCT_DOUBLE) ? sizeof(uint64_t) : sizeof(uint32_t);
                
                jeex_class->symtab[i].value = bumper_calloc(builder->output_arena,1,sz);
                FAIL_SET_JUMP(jeex_class->symtab[i].value,err,JERR_OOM,exit);

                memcpy(jeex_class->symtab[i].value,sym->value,sz);
            }
            break;

            case EJRCT_STRING:{
                JRawUTF8_t* utf8_org = sym->value;
                JEEXRawUTF8_t* utf8_new = bumper_calloc(builder->output_arena,1,sizeof(*utf8_new));
                FAIL_SET_JUMP(utf8_new,err,JERR_OOM,exit);

                utf8_new->length = utf8_org->length;
                utf8_new->string = bumper_calloc(builder->output_arena,utf8_new->length,1);
                FAIL_SET_JUMP(utf8_new->string,err,JERR_OOM,exit);

                memcpy(utf8_new->string,utf8_org->string,utf8_org->length);
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


    builder->jeex_header->static_fields_size = builder->linker->linker_global_data.sfield_curoffset;

exit:
    return err;
}