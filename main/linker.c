#include "linker.h"
#include "compiler.h"
#include "hashmap.h"
#include "list.h"
#include "loader.h"
#include "jerror.h"
#include "bumper.h"
#include "class.h"
#include "cfg.h"
#include "bstable.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>

static int bstable_JClass_cmp(const void* a, const void* b){
    JClass_t* ca = *(void**)a;
    JClass_t* cb = *(void**)b;

    return strcmp(ca->name, cb->name);
}

static void* bstable_JClass_find(bstable_t* classes, const void* name){
    JClass_t* class = &(JClass_t){
        .name = (char*)name,
    };

    void* ret = bstable_find_raw(classes, &class);

    return ret ? *(void**)ret : NULL;
}

int JLinker_init(JLinker_t* linker, JLoader_t* loader, bump_allocator_t* arena){
    linker->arena = arena;
    linker->loader = loader;

    linker->linker_global_data.linker_flags.is_firstlaunch = 1;

    INIT_LIST_HEAD(&linker->class_list);
    return bstable_init(&linker->class_map, linker->arena, loader->num_loaded, bstable_JClass_cmp, bstable_JClass_find);
}

static size_t value_sizeof(JValueType_t type){
    switch(type){
        case EJVT_REFERENCE:
        case EJVT_INT:
        case EJVT_FLOAT:
            return sizeof(uint32_t);
        
        case EJVT_LONG:
        case EJVT_DOUBLE:
            return sizeof(uint64_t);

        case EJVT_BOOL:
        case EJVT_BYTE:
            return sizeof(uint8_t);

        case EJVT_SHORT:
        case EJVT_CHAR:
            return sizeof(uint16_t);
        
        case EJVT_VOID:
            return 0;
    }

    return 0;
}

static int JClassSymbol_cmp(const void* a, const void* b){
    const JClassSymbol_t* sym_a = a;
    const JClassSymbol_t* sym_b = b;

    return sym_a->class_index - sym_b->class_index;
}

static JError_t JClassSymtab_init(JClassSymtab_t* symtab, JRawClass_t* description, bump_allocator_t* arena){
    JError_t err = JERR_OK;

    FAIL_SET_JUMP(symtab && description && arena,err,JERR_BADPARAM,exit);
    JConstantPool_t* cp = &description->constantpool;

    unsigned symtab_length = 0;
    for(unsigned i = 1; i < cp->count; i++){
        JConstant_t* constant = JConstantPool_get(cp, i);
        switch(constant->type){
            default: break;

            case EJCT_CLASS:
            case EJCT_STRING:
            case EJCT_LONG:
            case EJCT_DOUBLE:
            case EJCT_INT:
            case EJCT_FLOAT:
            case EJCT_FIELDREF:
            case EJCT_METHODREF:
            case EJCT_INTERFACE_METHODREF:
                symtab_length++;
                break;
        }
    }

    symtab->length = symtab_length;
    FAIL_SET_JUMP(((symtab->symbols = bumper_calloc(arena,symtab_length,sizeof(*symtab->symbols)))),err,JERR_OOM,exit);

    uint16_t sym_i = 0;
    for(unsigned i = 1; i < cp->count; i++){
        JConstant_t* constant = JConstantPool_get(cp, i);
        switch(constant->type){
            default: break;

            case EJCT_CLASS:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_CLASS;
            }
            break;

            case EJCT_STRING:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_STRING;
            }
            break;

            case EJCT_LONG:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i++; //Skip next entry
                symbol->symbol_type = EJRCT_LONG;
            }
            break;

            case EJCT_DOUBLE:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i++;
                symbol->symbol_type = EJRCT_DOUBLE;
            }
            break;

            case EJCT_INT:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_INT;
            }
            break;

            case EJCT_FLOAT:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_FLOAT;
            }
            break;

            case EJCT_FIELDREF:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_FIELD;
            }
            break;

            case EJCT_METHODREF:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_METHOD;
            }
            break;

            case EJCT_INTERFACE_METHODREF:{
                JClassSymbol_t* symbol = &symtab->symbols[sym_i++];
                symbol->class_index = i;
                symbol->symbol_type = EJRCT_INTERFACEMETHODREF;
            }
            break;
        }
    }

    qsort(symtab->symbols,symtab->length,sizeof(*symtab->symbols),JClassSymbol_cmp);

exit:
    return err;
} 

//Class index is index from raw class file!
JClassSymbol_t* JClassSymtab_get_symbol(JClassSymtab_t* symtab, uint16_t class_index){
    JClassSymbol_t to_find = {
        .class_index = class_index,
    };

    return bsearch(&to_find,symtab->symbols,symtab->length,sizeof(*symtab->symbols),JClassSymbol_cmp);
}

unsigned JClassSymtab_indexof(JClassSymtab_t* symtab, JClassSymbol_t* symbol){
    assert(symbol);
    return symbol - symtab->symbols;
}

static int bstable_JField_cmp(const void* a, const void* b){
    JField_t* fa = *(void**)a;
    JField_t* fb = *(void**)b;

    return strcmp(fa->name, fb->name);
}

static void* bstable_JField_find(bstable_t* fields, const void* name){
    JField_t* field = &(JField_t){
        .name = (char*)name,
    };

    void* ret = bstable_find_raw(fields, &field);

    return ret ? *(void**)ret : NULL;
}

static int bstable_JMethod_cmp(const void* a, const void* b){
    JMethod_t* ma = *(void**)a;
    JMethod_t* mb = *(void**)b;

    return strcmp(ma->name, mb->name);
}

static void* bstable_JMethod_find(bstable_t* methods, const void* name){
    JMethod_t* method = &(JMethod_t){
        .name = (char*)name,
    };

    void* ret = bstable_find_raw(methods, &method);

    return ret ? *(void**)ret : NULL;
}

//This method will create class, lookup its name and add it to the linker class bstable
//But it will not lookup its interface, lookup it constant pool and etc.
static JClass_t* create_class(JLinker_t* linker, JRawClass_t* raw_class){
    void* ret_val = NULL;
    
    //Lookup name
    JConstant_t* sclass_constant = JConstantPool_get(&raw_class->constantpool, raw_class->this_class);
    FAIL_SET_JUMP(sclass_constant && sclass_constant->type == EJCT_CLASS,ret_val,NULL,exit);

    JConstant_t* sname_constant = JConstantPool_get(&raw_class->constantpool, *(uint16_t*)sclass_constant->value);
    FAIL_SET_JUMP(sname_constant && sname_constant->type == EJCT_UTF8,ret_val,NULL,exit);

    JRawUTF8_t* name_utf8 = sname_constant->value;

    //Create class
    JClass_t* class = bstable_find(&linker->class_map,(char*)name_utf8->string);
    if(!class)
        class = bumper_calloc(linker->arena,1,sizeof(*class)); //Class does not exist, create new
    else goto exit;

    FAIL_SET_JUMP(class,ret_val,NULL,exit);
    class->flags.flags = 0;

    INIT_LIST_HEAD(&class->children);
    INIT_LIST_HEAD(&class->as_child);
    INIT_LIST_HEAD(&class->list);

    class->name = (char*)name_utf8->string; //There is normal C string, except that it is in java encoding.....


    //Allocate symtab
    FAIL_SET_JUMP(JClassSymtab_init(&class->symtab,raw_class,linker->arena) == JERR_OK,ret_val,NULL,exit);

    //Allocate interfaces
    class->interfaces.count = raw_class->interfaces_count;
    FAIL_SET_JUMP((class->interfaces.implement = bumper_calloc(linker->arena,class->interfaces.count,sizeof(*class->interfaces.implement))),
                                                                                                                                          ret_val,NULL,exit);

    //Allocate metadata
    JLinkerMetadata_t* metadata = bumper_calloc(linker->arena,1,sizeof(*metadata));
    FAIL_SET_JUMP(metadata, ret_val,NULL,exit);

    metadata->ifield_curoffset = 0;
    metadata->raw_self = raw_class;

    FAIL_SET_JUMP(bstable_init(&metadata->fields, linker->arena, raw_class->fields_count, bstable_JField_cmp, bstable_JField_find) == 0,ret_val,NULL,exit);
     FAIL_SET_JUMP(bstable_init(&metadata->methods, linker->arena, raw_class->methods_count, bstable_JMethod_cmp, bstable_JMethod_find) == 0,ret_val,NULL,exit);
    class->metadata = metadata;

    FAIL_SET_JUMP(bstable_insert(&linker->class_map, class) == 0,ret_val,NULL,exit);
    list_add(&class->list,&linker->class_list);
    ret_val = class;

exit:
    return ret_val;
}

static JError_t parse_method_prototype(JMethod_t* method, const char* descriptor, bump_allocator_t* arena) {
    if (!method || !descriptor || !arena) {
        return JERR_BADPARAM;
    }

    const char* p = descriptor;
    typeof(method->prototype)* prototype = &method->prototype;

    if (*p != '(') return JERR_BADPARAM;
    p++;

    prototype->arguments_count = 0;
    const char* counter_p = p;
    while (*counter_p != ')' && *counter_p != '\0') {
        prototype->arguments_count++;
        if (*counter_p == 'L') {
            counter_p = strchr(counter_p, ';');
            if (!counter_p) return JERR_BADPARAM;
        } else if (*counter_p == '[') {
            while (*counter_p == '[') counter_p++;
            if (*counter_p == 'L') {
                 counter_p = strchr(counter_p, ';');
                 if (!counter_p) return JERR_BADPARAM;
            }
        }
        counter_p++;
    }
    if (*counter_p == '\0') return JERR_BADPARAM;

    if (prototype->arguments_count > 0) {
        prototype->argument_types = bumper_alloc(arena, prototype->arguments_count * sizeof(JValueType_t));
        if (!prototype->argument_types) return JERR_OOM;
    } else {
        prototype->argument_types = NULL;
    }
    
    unsigned arg_index = 0;
    while (*p != ')') {
        JValueType_t type = (JValueType_t)*p;
        
        if (type == 'L' || type == '[') {
            prototype->argument_types[arg_index++] = EJVT_REFERENCE;
        } else {
            prototype->argument_types[arg_index++] = type;
        }

        if (type == 'L') {
            p = strchr(p, ';');
            p++;
        } else if (type == '[') {
            while(*p == '[') p++;
            if (*p == 'L') {
                p = strchr(p, ';');
            }
            p++;
        } else {
            p++;
        }
    }

    p++;

    if (*p == '\0') return JERR_BADPARAM;
    JValueType_t return_type_char = (JValueType_t)*p;
    
    if (return_type_char == '[' || return_type_char == 'L') {
        prototype->return_type = EJVT_REFERENCE;
    } else {
        prototype->return_type = return_type_char;
    }

    return JERR_OK;
}


//Recursive shit
static JError_t build_class(JLinker_t* linker, JClass_t* class){
    JError_t err = JERR_OK;
    if(class->flags.is_initialised) goto iterate_children; //Skip this class, already initialised

    JLinkerMetadata_t* metadata = class->metadata;
    JLinkerMetadata_t* parent_metadata = class->parent ? class->parent->metadata : NULL;
    metadata->ifield_curoffset = parent_metadata ? parent_metadata->ifield_curoffset : 0;

    JRawClass_t* description = metadata->raw_self;
    bool is_interface = (metadata->raw_self->flags & ACC_INTERFACE) == ACC_INTERFACE;

    //Parse fields i guess
    JConstantPool_t* raw_constantpool = &description->constantpool;
    for(unsigned i = 0; i < description->fields_count; i++){
        JRawField_t* raw_field = &description->fields[i];

        JConstant_t* fname_constant = JConstantPool_get(raw_constantpool,raw_field->name_index);
        JConstant_t* fdescription_constant = JConstantPool_get(raw_constantpool,raw_field->descriptor_index);

        FAIL_SET_JUMP(fname_constant->type == EJCT_UTF8, err, JERR_BADPARAM, exit);
        FAIL_SET_JUMP(fdescription_constant->type == EJCT_UTF8, err, JERR_BADPARAM, exit);

        char* fname = (char*)((JRawUTF8_t*)fname_constant->value)->string;
        char* fdescription = (char*)((JRawUTF8_t*)fdescription_constant->value)->string;

        char* mangled_fname = bumper_calloc(linker->arena, strlen(fname) + strlen(fdescription) + 2, sizeof(char));
        FAIL_SET_JUMP(mangled_fname, err, JERR_OOM, exit);

        sprintf(mangled_fname,"%s@%s",fname,fdescription); //This mangled name will be used to lookup fields in constantpool

        JField_t* field = bumper_calloc(linker->arena,1,sizeof(*field));
        FAIL_SET_JUMP(field,err,JERR_OOM,exit);

        field->name = mangled_fname;
        field->type = *fdescription;

        field->flags.is_static = (raw_field->flags & ACC_STATIC) == ACC_STATIC || is_interface;

        class->fields[field->flags.is_static].count++;

        if(field->flags.is_static){
            metadata->sfield_count++;
            field->offset = linker->linker_global_data.sfield_curoffset;
            linker->linker_global_data.sfield_curoffset += value_sizeof(field->type);

            JRawAttribute_t* attribute = NULL;
            list_for_each_entry(attribute,&raw_field->attributes,list){
                if(attribute->type == EJAT_CONSTANTVALUE){
                    field->flags.is_unitialised = 1;
                    
                    JConstant_t* constant = JConstantPool_get(raw_constantpool,*(uint16_t*)attribute->info);
                    FAIL_SET_JUMP(constant->type != EJCT_NULL,err, JERR_BADPARAM,exit);

                    field->constvalue = constant->value;
                }
            }
        }else{
            metadata->ifield_count++;
            field->offset = metadata->ifield_curoffset;
            metadata->ifield_curoffset += value_sizeof(field->type);
        }
        field->flags.is_alligned = (field->offset % sizeof(uint32_t) == 0) && (field->type != EJVT_DOUBLE && field->type != EJVT_LONG);
        field->owner = class;

        FAIL_SET_JUMP(bstable_insert(&metadata->fields, field) == 0, err, JERR_UNKNOWN, exit);
    }

    class->fields[0].fields = bumper_calloc(linker->arena,class->fields[0].count,sizeof(*class->fields[0].fields));
    class->fields[1].fields = bumper_calloc(linker->arena,class->fields[1].count,sizeof(*class->fields[1].fields));
    FAIL_SET_JUMP(class->fields[0].fields,err,JERR_OOM,exit);
    FAIL_SET_JUMP(class->fields[1].fields,err,JERR_OOM,exit);

    size_t field_index[2] = {0};
    /*hashmap_iterator_t field_iter = {0};
    hashmap_iterator_init(&metadata->fields,&field_iter);
    hashmap_entry_t* field_entry = NULL;
    while((field_entry = hashmap_iterator_next(&field_iter))){
        JField_t* field = field_entry->value;
        class->fields[field->flags.is_static].fields[field_index[field->flags.is_static]++] = field;
    }
    */

    for(unsigned i = 0; i < metadata->fields.count; i++){
        JField_t* field = (void*)metadata->fields.elements[i];
        class->fields[field->flags.is_static].fields[field_index[field->flags.is_static]++] = field;
    }

    //Generate methods
    for(unsigned i = 0; i < description->methods_count; i++){
        JRawMethod_t* raw_method = &description->methods[i];
        JMethod_t* method = bumper_calloc(linker->arena,1,sizeof(*method));
        FAIL_SET_JUMP(method,err,JERR_OOM,exit);

        JConstant_t* name_constant = JConstantPool_get(raw_constantpool,raw_method->name_index);
        JConstant_t* description_constant = JConstantPool_get(raw_constantpool,raw_method->descriptor_index);

        FAIL_SET_JUMP(name_constant->type == EJCT_UTF8, err, JERR_BADPARAM, exit);
        FAIL_SET_JUMP(description_constant->type == EJCT_UTF8, err, JERR_BADPARAM, exit);

        char* name = (char*)((JRawUTF8_t*)name_constant->value)->string;
        char* description = (char*)((JRawUTF8_t*)description_constant->value)->string;

        char* mangled_name = bumper_calloc(linker->arena, strlen(name) + strlen(description) + 2, sizeof(char));
        FAIL_SET_JUMP(mangled_name, err, JERR_OOM, exit);

        sprintf(mangled_name,"%s@%s",name,description); //This mangled name will be used to lookup fields in constantpool

        method->name = mangled_name;
        method->owner = class;
        FAIL_SET_JUMP(parse_method_prototype(method,description,linker->arena) == JERR_OK,err,JERR_UNKNOWN,exit);        

        method->flags.is_native = (raw_method->flags & ACC_NATIVE) == ACC_NATIVE;
        method->flags.is_static = (raw_method->flags & ACC_STATIC) == ACC_STATIC;
        method->flags.is_final = (raw_method->flags & ACC_FINAL) == ACC_FINAL;
        method->flags.is_frominterface = is_interface;
        method->flags.is_set = 0;


        if(method->flags.is_native){
            //Native methods doesnt need to be parsed now.
            //This is the job of compiler and further app loader
        } else {
            JCodeAttribute_t* code = NULL;
            JRawAttribute_t* current_attribute = NULL; //Using raw code attribute because we can directly use it (via our constant pool)
            list_for_each_entry(current_attribute,&raw_method->attributes,list){
                if(current_attribute->type == EJAT_CODE){
                    code = current_attribute->info;
                    break;
                }
            }
            method->code = code;
        }

        metadata->methods_count[method->flags.is_static]++;
        FAIL_SET_JUMP(bstable_insert(&metadata->methods,method) == 0,err,JERR_UNKNOWN,exit);
    }

    if(!is_interface){ //No reason to build vtable if this is an interface
        //Build vtable

        unsigned redefines_count = 0;
        for(unsigned i = 0; i < (class->parent ? class->parent->vtable.count : 0); i++){
            JMethod_t* redefine_with = bstable_find(&metadata->methods,class->parent->vtable.methods[i]->name);
            if(redefine_with && !redefine_with->flags.is_static)
                redefines_count++;
        }

        JMethodTable_t* new_vtable = &class->vtable;
        new_vtable->count = (class->parent ? class->parent->vtable.count : 0) 
                            + (metadata->methods_count[0] - redefines_count); 
        new_vtable->methods = bumper_calloc(linker->arena,new_vtable->count,sizeof(*new_vtable->methods));
        FAIL_SET_JUMP(new_vtable->methods,err,JERR_OOM,exit);

        if(class->parent)
            memcpy(new_vtable->methods,class->parent->vtable.methods,class->parent->vtable.count * sizeof(JMethod_t*));

        //See if there is something to override
        for(unsigned i = 0; i < (class->parent ? class->parent->vtable.count : 0); i++){
            JMethod_t* method = new_vtable->methods[i];
            assert(!method->flags.is_static);

            JMethod_t* override_with = bstable_find(&metadata->methods,method->name);
            if(override_with){
                FAIL_SET_JUMP(!method->flags.is_final,err,JERR_BADPARAM,exit);
                new_vtable->methods[i] = override_with;
                override_with->flags.is_set = 1;
            }
        }

        unsigned vtable_index = (class->parent ? class->parent->vtable.count : 0);
        /*hashmap_iterator_t method_iter = {0};
        hashmap_iterator_init(&metadata->methods, &method_iter);

        hashmap_entry_t* cur_entry = NULL;
        while((cur_entry = hashmap_iterator_next(&method_iter))){
            JMethod_t* method = cur_entry->value;
            if(method->flags.is_set) continue;
            if(method->flags.is_static) continue;

            unsigned method_index = vtable_index++;

            new_vtable->methods[method_index] = method;
            method->vtable_index = method_index;
        }
        new_vtable->count = vtable_index;
        */

        for(unsigned i = 0; i < metadata->methods.count; i++){
            JMethod_t* method = (void*)metadata->methods.elements[i];
            if(method->flags.is_set) continue;
            if(method->flags.is_static) continue;

            unsigned method_index = vtable_index++;

            new_vtable->methods[method_index] = method;
            method->vtable_index = method_index;
        }
    }
    class->ifields_size = metadata->ifield_curoffset;

iterate_children:
    JClass_t* child = NULL;
    list_for_each_entry(child,&class->children, as_child){
        FAIL_SET_JUMP(build_class(linker,child) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    class->flags.is_initialised = 1;

exit:
    return err;
}

static JError_t link_class(JLinker_t* linker, JClass_t* class){
    JError_t err = JERR_OK;

    JLinkerMetadata_t* metadata = class->metadata;
    JClassSymtab_t* symtab = &class->symtab;
    JConstantPool_t* raw_constantpool = &metadata->raw_self->constantpool;
    for(unsigned i = 0; i < symtab->length; i++){
        JClassSymbol_t* cur_symbol = &symtab->symbols[i];
        JConstant_t* constant = JConstantPool_get(raw_constantpool, cur_symbol->class_index);
        switch(cur_symbol->symbol_type){
            default: break;

            case EJRCT_FIELD:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClassSymbol_t* class_symbol = JClassSymtab_get_symbol(&class->symtab, fmim_ref->class_index);
                FAIL_SET_JUMP(class_symbol && class_symbol->symbol_type == EJRCT_CLASS && class_symbol->value != NULL, err,JERR_BADPARAM,exit);

                JClass_t* field_class = class_symbol->value;

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = field_class->metadata;

                JField_t* field = bstable_find(&metadata->fields,mangled_name);
                FAIL_SET_JUMP(field,err,JERR_NOTFOUND,exit);

                cur_symbol->value = field;
            }
            break;

            case EJRCT_METHOD:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClassSymbol_t* class_symbol = JClassSymtab_get_symbol(&class->symtab, fmim_ref->class_index);
                FAIL_SET_JUMP(class_symbol && class_symbol->symbol_type == EJRCT_CLASS && class_symbol->value != NULL, err,JERR_BADPARAM,exit);

                JClass_t* method_class = class_symbol->value;

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = method_class->metadata;

                JMethod_t* method = bstable_find(&metadata->methods,mangled_name);
                FAIL_SET_JUMP(method,err,JERR_NOTFOUND,exit);

                cur_symbol->value = method;
            }
            break;
    
            case EJRCT_INTERFACEMETHODREF:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClassSymbol_t* class_symbol = JClassSymtab_get_symbol(&class->symtab, fmim_ref->class_index);
                FAIL_SET_JUMP(class_symbol && class_symbol->symbol_type == EJRCT_CLASS && class_symbol->value != NULL, err,JERR_BADPARAM,exit);

                JClass_t* method_class = class_symbol->value;

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = method_class->metadata;

                JMethod_t* method = bstable_find(&metadata->methods,mangled_name);
                FAIL_SET_JUMP(method,err,JERR_NOTFOUND,exit);

                cur_symbol->value = method;
            }
            break;

            case EJRCT_STRING:{
                JRawString_t* raw_string = constant->value;

                JConstant_t* utf8_const = JConstantPool_get(raw_constantpool, raw_string->utf8_index);
                FAIL_SET_JUMP(utf8_const,err,JERR_NOTFOUND,exit);
                JRawUTF8_t* utf8 = utf8_const->value;
            
                cur_symbol->value = utf8;
            }
            break;
        }
    }
    JClass_t* child = NULL;
    list_for_each_entry(child,&class->children,as_child){
        FAIL_SET_JUMP(link_class(linker,child) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

JError_t JLinker_link(JLinker_t* linker){
    JError_t err = JERR_OK;
    JRawClass_t empty_description = {0}; //Array classes stub

    //Step 0: create all classes and parse simple constants (values)
    JRawClass_t* cur_raw = NULL;
    list_for_each_entry(cur_raw,&linker->loader->classes,list){
        JClass_t* new_class = create_class(linker,cur_raw);
        FAIL_SET_JUMP(new_class,err,JERR_UNKNOWN,exit); //This should not really fail at this stage, but if, i dont known. U fucked up already?

        if(new_class->flags.is_initialised == 0){
            new_class->flags.is_final = ((cur_raw->flags & ACC_FINAL) == ACC_FINAL);
            new_class->linker = linker;

            
            JClassSymtab_t* symtab = &new_class->symtab;
            for(unsigned i = 0; i < symtab->length; i++){
                JClassSymbol_t* cur_symbol = &symtab->symbols[i];
                JConstant_t* constant = JConstantPool_get(&cur_raw->constantpool,cur_symbol->class_index);
                switch(constant->type){
                    case EJRCT_INT:
                    case EJRCT_FLOAT:
                        cur_symbol->value = constant->value;
                    break;

                    case EJRCT_LONG:
                    case EJRCT_DOUBLE:
                        cur_symbol->value = constant->value;
                    break;


                    default: break;
                }
            }
        }
    }

    //Step 1: resolve class references
    INIT_LIST_HEAD(&linker->root_list);
    JClass_t* cur_class = NULL;
    list_for_each_entry(cur_class,&linker->class_list,list){
        JLinkerMetadata_t* metadata = cur_class->metadata;
        JRawClass_t* description = metadata->raw_self;

        JConstant_t* super_class = JConstantPool_get(&description->constantpool, description->super_class);
        if(super_class->type == EJCT_NULL){ //This is a root object. Treat it separately
            list_add(&cur_class->as_child,&linker->root_list); //Adding to root list even if it is unitialised(otherwise we cant reach others)
            continue;
        }

        if(!cur_class->flags.is_initialised){
            FAIL_SET_JUMP(super_class->type == EJCT_CLASS, err, JERR_BADPARAM,exit);

            JConstant_t* super_name = JConstantPool_get(&description->constantpool, *(uint16_t*)super_class->value);
            FAIL_SET_JUMP(super_name && super_name->type == EJCT_UTF8,err, JERR_BADPARAM,exit);

            char* super_name_cstr = (char*)(((JRawUTF8_t*)super_name->value)->string);

            JClass_t* parent = bstable_find(&linker->class_map, super_name_cstr);
            FAIL_SET_JUMP(parent && !parent->flags.is_final,err,JERR_NOTFOUND,exit);
            //Parent found huray!

            cur_class->parent = parent;
            list_add(&cur_class->as_child,&parent->children);
        }

        JClassSymtab_t* symtab = &cur_class->symtab;
        for(unsigned i = 0; i < symtab->length; i++){
            JClassSymbol_t* cur_symbol = &symtab->symbols[i];
            JConstant_t* constant = JConstantPool_get(&description->constantpool, cur_symbol->class_index);
            if(cur_symbol->symbol_type != EJRCT_CLASS) continue;
            
            JConstant_t* JC_class_name = JConstantPool_get(&description->constantpool, *(uint16_t*)constant->value);
            FAIL_SET_JUMP(JC_class_name && JC_class_name->type == EJCT_UTF8, err, JERR_BADPARAM,exit);

            char* class_name = (char*)(((JRawUTF8_t*)JC_class_name->value)->string);
            JClass_t* refered_class = bstable_find(&linker->class_map, class_name);
            if(class_name[0] == '[' && refered_class == NULL && !cur_class->flags.is_initialised){ //This should work. At least temporary
                JClass_t* array_class = bumper_calloc(linker->arena,1,sizeof(*array_class));
                FAIL_SET_JUMP(array_class,err,JERR_OOM,exit);

                INIT_LIST_HEAD(&array_class->children);
                INIT_LIST_HEAD(&array_class->as_child);
                INIT_LIST_HEAD(&array_class->list);

                array_class->name = class_name;
                array_class->parent = bstable_find(&linker->class_map, "java/lang/Object");

                //TODO: interfaces

                array_class->flags.is_initialised = 0;
                array_class->linker = linker;
                list_add(&array_class->list,&linker->class_list);
                list_add(&array_class->as_child,&array_class->parent->children);

                JLinkerMetadata_t* array_metadata = bumper_calloc(linker->arena,1,sizeof(*array_metadata));
                FAIL_SET_JUMP(array_metadata,err,JERR_OOM,exit);

                bstable_init(&array_metadata->fields,linker->arena,0,bstable_JField_cmp,bstable_JField_find);
                bstable_init(&array_metadata->methods,linker->arena,0,bstable_JMethod_cmp,bstable_JMethod_find);

                array_metadata->raw_self = &empty_description;

                array_class->metadata = array_metadata;
                FAIL_SET_JUMP(bstable_insert(&linker->class_map,array_class) == 0,err,JERR_OOM,exit);

                refered_class = array_class;
            }
            FAIL_SET_JUMP(refered_class,err,({printf("Cannot find class: %s\n",class_name);(JERR_NOTFOUND);}),exit);

            cur_symbol->value = refered_class; //Add this class to constantpool
        }

        for(unsigned i = 0; i < description->interfaces_count; i++){
            JConstant_t* constant = JConstantPool_get(&description->constantpool, description->interfaces[i]);
            FAIL_SET_JUMP(constant->type == EJCT_CLASS, err, JERR_BADPARAM,exit);

            JConstant_t* JC_class_name = JConstantPool_get(&description->constantpool, *(uint16_t*)constant->value);
            FAIL_SET_JUMP(JC_class_name && JC_class_name->type == EJCT_UTF8, err, JERR_BADPARAM,exit);

            char* class_name = (char*)(((JRawUTF8_t*)JC_class_name->value)->string);
            cur_class->interfaces.implement[i] = bstable_find(&linker->class_map, class_name);
        }
    }

    //Step 2 iterate over our tree to link classes
    JClass_t* cur_root = NULL;
    list_for_each_entry(cur_root,&linker->root_list,as_child){
        FAIL_SET_JUMP(build_class(linker,cur_root) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

    //Step 3 link constant pool
    list_for_each_entry(cur_root,&linker->root_list,as_child){
        FAIL_SET_JUMP(link_class(linker,cur_root) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    
exit:
    linker->linker_global_data.linker_flags.is_firstlaunch = 0;
    return err;
}

JClass_t* JClass_get(JLinker_t* linker, char* class_name){
    return linker && class_name ? bstable_find(&linker->class_map,class_name) : NULL;
}

//It requires MANGLED name. in name@description format!
JMethod_t* JClass_get_method(JClass_t* class, char* method_name){
    JMethod_t* found = NULL;
    for(JClass_t* cur_class = class; cur_class; cur_class = cur_class->parent){
        JLinkerMetadata_t* class_metadata = cur_class->metadata;
        found = bstable_find(&class_metadata->methods, method_name);
        if(found) break;
    }
    return found;
}

JField_t* JClass_get_field(JClass_t* class, char* field_name){
    JField_t* found = NULL;
    for(JClass_t* cur_class = class; cur_class; cur_class = cur_class->parent){
        JLinkerMetadata_t* class_metadata = cur_class->metadata;
        found = bstable_find(&class_metadata->fields,field_name);
        if(found) break;
    }
    return found;    
}