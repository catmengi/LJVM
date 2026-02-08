#include "linker.h"
#include "hashmap.h"
#include "list.h"
#include "loader.h"
#include "jerror.h"
#include "bumper.h"
#include "class.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>

static void* linker_ht_arena_alloc(void* userctx, size_t size){
    return bumper_alloc(userctx,size);
}

int JLinker_init(JLinker_t* linker, JLoader_t* loader, bump_allocator_t* arena){
    linker->arena = arena;
    linker->loader = loader;

    INIT_LIST_HEAD(&linker->class_list);
    return hashmap_init(&linker->class_map,32,linker_ht_arena_alloc,linker->arena);
}

static size_t value_sizeof(JValueType_t type){
    if(type == EJVT_DOUBLE || type == EJVT_LONG)
        return sizeof(uint64_t);
    else return sizeof(uint32_t);
}

//This method will create class, lookup its name and add it to the linker hashtable
//But it will not lookup its interface, lookup it constant pool and etc.
static JClass_t* create_class(JLinker_t* linker, JRawClass_t* raw_class){
    void* ret_val = NULL;
    
    JClass_t* class = bumper_calloc(linker->arena,1,sizeof(*class));
    FAIL_SET_JUMP(class,ret_val,NULL,exit);

    INIT_LIST_HEAD(&class->child_list);
    INIT_LIST_HEAD(&class->as_child);
    INIT_LIST_HEAD(&class->list);

    //Lookup name
    JConstant_t* sclass_constant = JConstantPool_get(&raw_class->constantpool, raw_class->this_class);
    FAIL_SET_JUMP(sclass_constant && sclass_constant->type == EJCT_CLASS,ret_val,NULL,exit);

    JConstant_t* sname_constant = JConstantPool_get(&raw_class->constantpool, *(uint16_t*)sclass_constant->value);
    FAIL_SET_JUMP(sname_constant && sname_constant->type == EJCT_UTF8,ret_val,NULL,exit);

    JRawUTF8_t* name_utf8 = sname_constant->value;

    class->name = (char*)name_utf8->string; //There is normal C string, except that it is in java encoding.....


    //Allocate constant pool
    class->constantpool.size = raw_class->constantpool.count;
    class->constantpool.constants = bumper_calloc(linker->arena,class->constantpool.size,sizeof(*class->constantpool.constants));
    FAIL_SET_JUMP(class->constantpool.constants,ret_val,NULL,exit);

    //Allocate interfaces
    class->interfaces.count = raw_class->interfaces_count;
    FAIL_SET_JUMP((class->interfaces.implement = bumper_calloc(linker->arena,class->interfaces.count,sizeof(*class->interfaces.implement))),
                                                                                                                                          ret_val,NULL,exit);

    //Allocate metadata
    JLinkerMetadata_t* metadata = bumper_calloc(linker->arena,1,sizeof(*metadata));
    FAIL_SET_JUMP(metadata, ret_val,NULL,exit);

    metadata->ifield_curoffset = 0;
    metadata->raw_self = raw_class;

    FAIL_SET_JUMP(hashmap_init(&metadata->fields,8,linker_ht_arena_alloc,linker->arena) == 0,ret_val,NULL,exit);
    FAIL_SET_JUMP(hashmap_init(&metadata->all_methods,8,linker_ht_arena_alloc,linker->arena) == 0,ret_val,NULL,exit);
    class->metadata = metadata;

    FAIL_SET_JUMP(hashmap_set(&linker->class_map,class->name, class) == 0,ret_val,NULL,exit);
    list_add(&class->list,&linker->class_list);
    ret_val = class;

exit:
    return ret_val;
}


static JMethodRef_t* lookup_vtable(JClass_t* class, char* mangled_name){
    for(JClass_t* cur = class; cur; cur = cur->parent){
        JLinkerMetadata_t* metadata = cur->metadata;
        JMethod_t* is_found = hashmap_get(&metadata->all_methods,mangled_name);
        if(is_found && is_found->methodref)
            return is_found->methodref;
    }

    return NULL;
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

static unsigned methodtable_originals_count(JMethodTable_t* new_methods, JMethodTable_t* parent_vtable) {
    if (!parent_vtable || parent_vtable->count == 0) {
        // If there's no parent, count all non-statically-linked methods.
        unsigned count = 0;
        for (unsigned i = 0; i < new_methods->count; ++i) {
            if (new_methods->methods[i] && !new_methods->methods[i]->flags.is_staticlinked) {
                count++;
            }
        }
        return count;
    }
    
    if (new_methods->count == 0) {
        return 0;
    }

    unsigned originals_count = 0;

    for (unsigned i = 0; i < new_methods->count; ++i) {
        JMethod_t* new_method = new_methods->methods[i];

        if (!new_method || !new_method->name || new_method->flags.is_staticlinked) {
            continue;
        }

        bool found_in_parent = false;

        for (unsigned j = 0; j < parent_vtable->count; ++j) {
            JMethod_t* parent_method = parent_vtable->methods[j];
            if (!parent_method || !parent_method->name) {
                continue;
            }

            if (strcmp(new_method->name, parent_method->name) == 0) {
                found_in_parent = true;
                break;
            }
        }

        if (!found_in_parent) {
            originals_count++;
        }
    }

    return originals_count;
}

//Recursive shit
static JError_t build_class(JLinker_t* linker, JClass_t* class){
    JError_t err = JERR_OK;

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

        field->flags.is_static = (raw_field->flags & ACC_STATIC) == ACC_STATIC;
        if(is_interface) assert(field->flags.is_static);

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
        field->owner = class;

        FAIL_SET_JUMP(hashmap_set(&metadata->fields, field->name, field) == 0, err, JERR_UNKNOWN, exit);
    }

    class->fields[0].fields = bumper_calloc(linker->arena,class->fields[0].count,sizeof(*class->fields[0].fields));
    class->fields[1].fields = bumper_calloc(linker->arena,class->fields[1].count,sizeof(*class->fields[1].fields));
    FAIL_SET_JUMP(class->fields[0].fields,err,JERR_OOM,exit);
    FAIL_SET_JUMP(class->fields[1].fields,err,JERR_OOM,exit);

    size_t field_index[2] = {0};
    hashmap_iterator_t field_iter = {0};
    hashmap_iterator_init(&metadata->fields,&field_iter);
    hashmap_entry_t* field_entry = NULL;
    while((field_entry = hashmap_iterator_next(&field_iter))){
        JField_t* field = field_entry->value;
        class->fields[field->flags.is_static].fields[field_index[field->flags.is_static]++] = field;
    }

    //Generate methods
    class->vtable.count = description->methods_count + (class->parent ? class->parent->vtable.count : 0);
    class->vtable.methods = bumper_calloc(linker->arena,class->vtable.count, sizeof(*class->vtable.methods));
    unsigned cur_vtable_start = class->parent ? class->parent->vtable.count : 0;
    if(class->parent){
        for(unsigned i = 0; i < class->parent->vtable.count; i++){
            class->vtable.methods[i] = class->parent->vtable.methods[i];
        }
    }

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
        method->flags.is_staticlinked = method->flags.is_static || ((raw_method->flags & ACC_PRIVATE) == ACC_PRIVATE) || (strcmp(name, "<init>") == 0);
        method->flags.is_frominterface = is_interface;
        method->flags.is_set = 0;

        if(!is_interface || (is_interface && method->flags.is_static)){
            if(!method->flags.is_native){
                //TODO("PARSE CODE ATTRIBUTE!!!!!");
            } else {
                //TODO("NATIVE METHOD LOOKUP");
            }
        }

            
        FAIL_SET_JUMP(hashmap_set(&metadata->all_methods,method->name,method) == 0,err,JERR_UNKNOWN,exit);
    }

    if(!is_interface){ //No reason to build vtable if this is an interface
        //Build vtable

        unsigned redefines_count = 0;
        for(unsigned i = 0; i < (class->parent ? class->parent->vtable.count : 0); i++){
            JMethod_t* redefine_with = hashmap_get(&metadata->all_methods,class->parent->vtable.methods[i]->name);
            if(redefine_with && !redefine_with->flags.is_staticlinked)
                redefines_count++;
        }

        JMethodTable_t* new_vtable = &class->vtable;
        new_vtable->count = (class->parent ? class->parent->vtable.count : 0) 
                            + (description->methods_count - redefines_count); 
        new_vtable->methods = bumper_calloc(linker->arena,new_vtable->count,sizeof(*new_vtable->methods));
        FAIL_SET_JUMP(new_vtable->methods,err,JERR_OOM,exit);

        if(class->parent)
            memcpy(new_vtable->methods,class->parent->vtable.methods,class->parent->vtable.count * sizeof(JMethod_t*));

        //See if there is something to override
        for(unsigned i = 0; i < (class->parent ? class->parent->vtable.count : 0); i++){
            JMethod_t* method = new_vtable->methods[i];
            assert(!method->flags.is_staticlinked);

            JMethod_t* override_with = hashmap_get(&metadata->all_methods,method->name);
            if(override_with){
                FAIL_SET_JUMP(!method->flags.is_final,err,JERR_BADPARAM,exit);
                new_vtable->methods[i] = override_with;
                override_with->flags.is_set = 1;
            }
        }

        unsigned vtable_index = (class->parent ? class->parent->vtable.count : 0);
        hashmap_iterator_t method_iter = {0};
        hashmap_iterator_init(&metadata->all_methods, &method_iter);

        hashmap_entry_t* cur_entry = NULL;
        while((cur_entry = hashmap_iterator_next(&method_iter))){
            JMethod_t* method = cur_entry->value;
            if(method->flags.is_set) continue;
            if(method->flags.is_staticlinked) continue;

            assert(method->methodref == NULL);

            unsigned method_index = vtable_index++;
            new_vtable->methods[method_index] = method;

            JMethodRef_t* new_ref = bumper_alloc(linker->arena,sizeof(*new_ref));
            FAIL_SET_JUMP(new_ref,err,JERR_OOM,exit);

            new_ref->vtable_offset = method_index;
            method->methodref = new_ref;
        }
        new_vtable->count = vtable_index;
    }
    class->ifields_size = metadata->ifield_curoffset;

    JClass_t* child = NULL;
    list_for_each_entry(child,&class->child_list, as_child){
        FAIL_SET_JUMP(build_class(linker,child) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

static JError_t link_class(JLinker_t* linker, JClass_t* class){
    JError_t err = JERR_OK;

    JLinkerMetadata_t* metadata = class->metadata;
    JConstantPool_t* raw_constantpool = &metadata->raw_self->constantpool;
    for(unsigned i = 1; i < raw_constantpool->count; i++){
        JConstant_t* constant = JConstantPool_get(raw_constantpool, i);
        switch(constant->type){
            default: break;

            case EJCT_FIELDREF:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClass_t* field_class = class->constantpool.constants[fmim_ref->class_index];

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = field_class->metadata;

                JField_t* field = hashmap_get(&metadata->fields,mangled_name);
                FAIL_SET_JUMP(field,err,JERR_NOTFOUND,exit);

                class->constantpool.constants[i] = field;
            }
            break;

            case EJCT_METHODREF:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClass_t* method_class = class->constantpool.constants[fmim_ref->class_index];

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = method_class->metadata;

                JMethod_t* method = hashmap_get(&metadata->all_methods,mangled_name);
                FAIL_SET_JUMP(method,err,JERR_NOTFOUND,exit);

                if(method->flags.is_staticlinked){
                    class->constantpool.constants[i] = method;
                } else{
                    FAIL_SET_JUMP(method->methodref,err,JERR_UNKNOWN,exit);
                    class->constantpool.constants[i] = method->methodref;
                }
            }
            break;
    
            case EJCT_INTERFACE_METHODREF:{
                JRaw_FMIM_ref_t* fmim_ref = constant->value;
                JClass_t* method_class = class->constantpool.constants[fmim_ref->class_index];

                JRawNameAndType_t* nameandtype = JConstantPool_get(raw_constantpool, fmim_ref->nameandtype_index)->value;

                JRawUTF8_t* name_utf8 = JConstantPool_get(raw_constantpool, nameandtype->name_index)->value;
                JRawUTF8_t* description_utf8 = JConstantPool_get(raw_constantpool, nameandtype->descriptor_index)->value;

                char* name = (char*)name_utf8->string;
                char* description = (char*)description_utf8->string;

                char mangled_name[strlen(name) + strlen(description) + 2];
                sprintf(mangled_name,"%s@%s",name,description);

                JLinkerMetadata_t* metadata = method_class->metadata;

                JMethod_t* method = hashmap_get(&metadata->all_methods,mangled_name);
                FAIL_SET_JUMP(method,err,JERR_NOTFOUND,exit);

                JInterfaceMethodRef_t* ref = bumper_alloc(linker->arena,sizeof(*ref));
                FAIL_SET_JUMP(ref,err,JERR_OOM,exit);

                ref->should_implement = method_class;
                ref->name = method->name;

                class->constantpool.constants[i] = ref;
            }
            break;

            case EJCT_STRING:{
                printf("%s:%d: TODO: java constants string are undone!\n",__FILE__,__LINE__);
            }
            break;
        }
    }
    JClass_t* child = NULL;
    list_for_each_entry(child,&class->child_list,as_child){
        FAIL_SET_JUMP(link_class(linker,child) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

exit:
    return err;
}

JError_t JLinker_link(JLinker_t* linker){
    JError_t err = JERR_OK;

    //Step 0: create all classes and parse simple constants (values)
    JRawClass_t* cur_raw = NULL;
    list_for_each_entry(cur_raw,&linker->loader->classes,list){
        JClass_t* new_class = create_class(linker,cur_raw);
        FAIL_SET_JUMP(new_class,err,JERR_UNKNOWN,exit); //This should not really fail at this stage, but if, i dont known. U fucked up already?

        new_class->flags.is_final = ((cur_raw->flags & ACC_FINAL) == ACC_FINAL);

        for(unsigned i = 1; i < cur_raw->constantpool.count; i++){
            JConstant_t* constant = JConstantPool_get(&cur_raw->constantpool,i);
            switch(constant->type){
                case EJCT_INT:
                case EJCT_FLOAT:
                    new_class->constantpool.constants[i] = constant->value;
                break;

                case EJCT_LONG:
                case EJCT_DOUBLE:
                    new_class->constantpool.constants[i] = constant->value;
                    i++;
                break;


                default: break;
            }
        }
    }

    //Step 1: resolve class references
    struct list_head root_list = LIST_HEAD_INIT(root_list);

    JClass_t* cur_class = NULL;
    list_for_each_entry(cur_class,&linker->class_list,list){
        JLinkerMetadata_t* metadata = cur_class->metadata;
        JRawClass_t* description = metadata->raw_self;

        JConstant_t* super_class = JConstantPool_get(&description->constantpool, description->super_class);
        if(super_class->type == EJCT_NULL){ //This is a root object. Treat it separately
            list_add(&cur_class->as_child,&root_list);
            continue;
        }

        FAIL_SET_JUMP(super_class->type == EJCT_CLASS, err, JERR_BADPARAM,exit);

        JConstant_t* super_name = JConstantPool_get(&description->constantpool, *(uint16_t*)super_class->value);
        FAIL_SET_JUMP(super_name && super_name->type == EJCT_UTF8,err, JERR_BADPARAM,exit);

        char* super_name_cstr = (char*)(((JRawUTF8_t*)super_name->value)->string);

        JClass_t* parent = hashmap_get(&linker->class_map, super_name_cstr);
        FAIL_SET_JUMP(parent && !parent->flags.is_final,err,JERR_NOTFOUND,exit);
        //Parent found huray!

        cur_class->parent = parent;
        list_add(&cur_class->as_child,&parent->child_list);

        for(unsigned i = 1; i < description->constantpool.count; i++){
            JConstant_t* constant = JConstantPool_get(&description->constantpool, i);
            if(constant->type != EJCT_CLASS) continue;
            
            JConstant_t* JC_class_name = JConstantPool_get(&description->constantpool, *(uint16_t*)constant->value);
            FAIL_SET_JUMP(JC_class_name && JC_class_name->type == EJCT_UTF8, err, JERR_BADPARAM,exit);

            char* class_name = (char*)(((JRawUTF8_t*)JC_class_name->value)->string);

            JClass_t* refered_class = hashmap_get(&linker->class_map, class_name);
            FAIL_SET_JUMP(refered_class,err,JERR_NOTFOUND,exit);

            cur_class->constantpool.constants[i] = refered_class; //Add this class to constantpool
        }

        for(unsigned i = 0; i < description->interfaces_count; i++){
            JConstant_t* constant = JConstantPool_get(&description->constantpool, description->interfaces[i]);
            FAIL_SET_JUMP(constant->type == EJCT_CLASS, err, JERR_BADPARAM,exit);

            JConstant_t* JC_class_name = JConstantPool_get(&description->constantpool, *(uint16_t*)constant->value);
            FAIL_SET_JUMP(JC_class_name && JC_class_name->type == EJCT_UTF8, err, JERR_BADPARAM,exit);

            char* class_name = (char*)(((JRawUTF8_t*)JC_class_name->value)->string);

            JClass_t* refered_class = hashmap_get(&linker->class_map, class_name);
            FAIL_SET_JUMP(refered_class,err,JERR_NOTFOUND,exit);

            cur_class->interfaces.implement[i] = refered_class;
        }
    }

    //Step 2 iterate over our tree to link classes
    JClass_t* cur_root = NULL;
    list_for_each_entry(cur_root,&root_list,as_child){
        FAIL_SET_JUMP(build_class(linker,cur_root) == JERR_OK,err,JERR_UNKNOWN,exit);
    }

    //Step 3 link constant pool
    list_for_each_entry(cur_root,&root_list,as_child){
        FAIL_SET_JUMP(link_class(linker,cur_root) == JERR_OK,err,JERR_UNKNOWN,exit);
    }
    linker->linker_global_data.sfield_memory = bumper_calloc(linker->arena,linker->linker_global_data.sfield_curoffset,1);
    FAIL_SET_JUMP(linker->linker_global_data.sfield_memory,err,JERR_OOM,exit);

    //TODO: step 4, run verification process

exit:
    return err;
}