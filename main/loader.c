#include "loader.h"
#include "bumper.h"
#include "list.h"

#include "jerror.h"
#include "memstream.h"

#include <assert.h>
#include <string.h>

int JLoader_init(JLoader_t* loader, bump_allocator_t* arena){
    INIT_LIST_HEAD(&loader->classes);
    loader->arena = arena;
    loader->num_loaded = 0;

    return 0;
}

#define JMAGIC 0xCAFEBABE

JError_t JConstantPool_init(JConstantPool_t* constantpool, unsigned size, bump_allocator_t* allocate_from){
    assert(size > 0);

    constantpool->count = size;
    constantpool->constants = bumper_calloc(allocate_from,constantpool->count,sizeof(*constantpool->constants));
    if(!constantpool->constants) return JERR_OOM;

    return JERR_OK;
}

JConstant_t* JConstantPool_get(JConstantPool_t* constantpool, unsigned index){
    return index < constantpool->count ? &constantpool->constants[index] : NULL;
}

//Parser errors are fatal, so this can be considered OK
typedef void* (*ConstantParser_t)(bump_allocator_t* arena, memstream_t* stream);

void* parse_utf8(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    JRawUTF8_t* utf8_constant = bumper_alloc(arena,sizeof(*utf8_constant));
    FAIL_SET_JUMP(utf8_constant,ret,NULL,exit);

    FAIL_SET_JUMP(memstream_readU16(stream,&utf8_constant->length) == MSERR_OK,ret,NULL,exit);

    utf8_constant->string = bumper_alloc(arena,utf8_constant->length + 1);
    FAIL_SET_JUMP(utf8_constant->string,ret,NULL,exit);

    FAIL_SET_JUMP(memstream_readBUF(stream,utf8_constant->string,utf8_constant->length) == MSERR_OK,ret,NULL,exit);
    utf8_constant->string[utf8_constant->length] = '\0'; //Null terminating it

    ret = utf8_constant;
exit:
    return ret;
}

void* parse_class(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    uint16_t name_index = 0;
    FAIL_SET_JUMP(memstream_readU16(stream,&name_index) == MSERR_OK,ret,NULL,exit);

    uint16_t* constant_info = bumper_alloc(arena,sizeof(name_index));
    FAIL_SET_JUMP(constant_info,ret,NULL,exit);

    *constant_info = name_index;
    ret = constant_info;
exit:
    return ret;
}

void* parse_string(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    uint16_t index = 0;
    FAIL_SET_JUMP(memstream_readU16(stream,&index) == MSERR_OK,ret,NULL,exit);

    uint16_t* string_index = bumper_alloc(arena,sizeof(index));
    FAIL_SET_JUMP(string_index,ret,NULL,exit);

    *string_index = index;
    ret = string_index;
exit:
    return ret;
}

void* parse_u32(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    uint32_t* u32 = bumper_alloc(arena,sizeof(*u32));
    FAIL_SET_JUMP(u32,ret,NULL,exit);

    FAIL_SET_JUMP(memstream_readU32(stream,u32) == MSERR_OK,ret,NULL,exit);
    ret = u32;
exit:
    return ret;
}

void* parse_u64(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    uint64_t* u64 = bumper_alloc(arena,sizeof(*u64));
    FAIL_SET_JUMP(u64,ret,NULL,exit);

    FAIL_SET_JUMP(memstream_readU64(stream,u64) == MSERR_OK,ret,NULL,exit);
    ret = u64;
exit:
    return ret;
}

void* parse_fmim(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    JRaw_FMIM_ref_t* fmim_ref = bumper_alloc(arena,sizeof(*fmim_ref));
    FAIL_SET_JUMP(fmim_ref,ret,NULL,exit);

    FAIL_SET_JUMP(memstream_readU16(stream,&fmim_ref->class_index) == MSERR_OK,ret,NULL,exit);
    FAIL_SET_JUMP(memstream_readU16(stream,&fmim_ref->nameandtype_index) == MSERR_OK,ret,NULL,exit);

    ret = fmim_ref;
exit:
    return ret;
}

void* parse_nameandtype(bump_allocator_t* arena, memstream_t* stream){
    void* ret = NULL;

    JRawNameAndType_t* nameandtype = bumper_alloc(arena,sizeof(*nameandtype));
    FAIL_SET_JUMP(nameandtype, ret, NULL, exit);

    FAIL_SET_JUMP(memstream_readU16(stream,&nameandtype->name_index) == MSERR_OK,ret,NULL,exit);
    FAIL_SET_JUMP(memstream_readU16(stream,&nameandtype->descriptor_index) == MSERR_OK,ret,NULL,exit);
    
    ret = nameandtype;
exit:
    return ret;
}

static ConstantParser_t constant_parsers[] = {
    [EJCT_UTF8] = parse_utf8, 
    [EJCT_CLASS] = parse_class,
    [EJCT_FIELDREF] = parse_fmim,
    [EJCT_METHODREF] = parse_fmim,
    [EJCT_INTERFACE_METHODREF] = parse_fmim,
    [EJCT_STRING] = parse_string,
    [EJCT_INT] = parse_u32,
    [EJCT_FLOAT] = parse_u32,
    [EJCT_LONG] = parse_u64,
    [EJCT_DOUBLE] = parse_u64,
    [EJCT_NAMEANDTYPE] = parse_nameandtype,
};

typedef void* (*AttributeInfoParserFN_t)(bump_allocator_t* arena, JRawClass_t* current_class, memstream_t* stream);
typedef struct{
    char* name;
    JRawAttributeType_t type;
    AttributeInfoParserFN_t parser;
}AttributeInfoParserDesc_t;

static JError_t parse_attributes(struct list_head* output_list,JRawClass_t* current_class, bump_allocator_t* arena, memstream_t* stream);
static void* code_parse(bump_allocator_t* arena, JRawClass_t* current_class, memstream_t* stream){
    JCodeAttribute_t* code = bumper_calloc(arena,1,sizeof(*code));
    if(!code) return NULL;

    if(memstream_readU16(stream,&code->max_stack) != MSERR_OK) return NULL;
    if(memstream_readU16(stream,&code->max_locals) != MSERR_OK) return NULL;
    if(memstream_readU32(stream,&code->code_length) != MSERR_OK) return NULL;

    code->code = bumper_alloc(arena,code->code_length);
    if(!code->code) return NULL;

    if(memstream_readBUF(stream, code->code, code->code_length) != MSERR_OK) return NULL;
    if(memstream_readU16(stream,&code->exception_table_length) != MSERR_OK) return NULL;

    code->exception_table = bumper_calloc(arena,code->exception_table_length,sizeof(*code->exception_table));
    if(!code->exception_table) return NULL;

    for(unsigned i = 0; i < code->exception_table_length; i++){
        typeof(*code->exception_table)* exception = &code->exception_table[i];
        if(memstream_readU16(stream,&exception->start_pc) != MSERR_OK) return NULL;
        if(memstream_readU16(stream,&exception->end_pc) != MSERR_OK) return NULL;
        if(memstream_readU16(stream,&exception->handler_pc) != MSERR_OK) return NULL;
        if(memstream_readU16(stream,&exception->catch_type) != MSERR_OK) return NULL;
    }

    uint16_t attributes_count = 0;
    if(memstream_readU16(stream,&attributes_count) != MSERR_OK) return NULL;

    INIT_LIST_HEAD(&code->attributes);
    for(unsigned i = 0; i < attributes_count; i++){
        if(parse_attributes(&code->attributes, current_class, arena, stream) != JERR_OK) return NULL;
    }   

    return code;
}

static void* constantvalue_parse(bump_allocator_t* arena, JRawClass_t* current_class, memstream_t* stream){
    uint16_t* constant_index = bumper_alloc(arena,sizeof(*constant_index));
    if(!constant_index) return NULL;
    if(memstream_readU16(stream, constant_index) != MSERR_OK) return NULL;

    return constant_index;
}

static AttributeInfoParserDesc_t attribute_parsers[] = {
    {"Code",EJAT_CODE,code_parse},
    {"ConstantValue",EJAT_CONSTANTVALUE,constantvalue_parse},
    //TODO: StackMap from cldc
};

static JError_t parse_attributes(struct list_head* output_list,JRawClass_t* current_class, bump_allocator_t* arena, memstream_t* stream){
    JError_t err = JERR_OK;

    uint16_t name_index = 0;
    uint32_t length = 0;
    FAIL_SET_JUMP(memstream_readU16(stream,&name_index) == MSERR_OK,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(memstream_readU32(stream,&length) == MSERR_OK,err,JERR_BADPARAM,exit)

    JConstant_t* name_constant = JConstantPool_get(&current_class->constantpool, name_index);
    FAIL_SET_JUMP(name_constant && name_constant->type == EJCT_UTF8,err,JERR_BADPARAM,exit);

    char* name = (char*)((JRawUTF8_t*)name_constant->value)->string;

    AttributeInfoParserDesc_t* attribute_parser = NULL;
    for(unsigned i = 0; i < sizeof(attribute_parsers) / sizeof(attribute_parsers[0]); i++){
        if(strcmp(name,attribute_parsers[i].name) == 0){
            attribute_parser = &attribute_parsers[i];
            break;
        }
    }

    if(attribute_parser){
        JRawAttribute_t* new_attribute = bumper_calloc(arena,1,sizeof(*new_attribute));
        FAIL_SET_JUMP(new_attribute,err,JERR_OOM,exit);

        INIT_LIST_HEAD(&new_attribute->list);

        new_attribute->type = attribute_parser->type;
        new_attribute->info = attribute_parser->parser(arena,current_class,stream);
        FAIL_SET_JUMP(new_attribute->info,err,JERR_UNKNOWN,exit);

        list_add(&new_attribute->list,output_list);
    } else FAIL_SET_JUMP(memstream_skip(stream,length) == MSERR_OK,err,JERR_BADPARAM,exit);

exit:
    return err;
}




JError_t JLoader_load(JLoader_t* loader, memstream_t* stream){
    JError_t err = JERR_OK;

    uint32_t magic = 0;
    FAIL_SET_JUMP(memstream_readU32(stream,&magic) == MSERR_OK,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(magic == JMAGIC, err, JERR_BADPARAM,exit);

    JRawClass_t* new_class = bumper_calloc(loader->arena,1,sizeof(*new_class));
    INIT_LIST_HEAD(&new_class->list);

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->version.minor) == MSERR_OK,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->version.major) == MSERR_OK,err,JERR_BADPARAM,exit);

    uint16_t constantpool_size = 0;
    FAIL_SET_JUMP(memstream_readU16(stream,&constantpool_size) == MSERR_OK,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(JConstantPool_init(&new_class->constantpool, constantpool_size, loader->arena) == JERR_OK,err,JERR_OOM,exit);

    JConstantPool_t* constant_pool = &new_class->constantpool;
    for(unsigned i = 1; i < constantpool_size; i++){
        JConstant_t* constant = JConstantPool_get(constant_pool, i);
        assert(constant);

        uint8_t constant_type = EJCT_NULL;
        FAIL_SET_JUMP(memstream_readU8(stream,&constant_type) == MSERR_OK,err,JERR_BADPARAM,exit);

        constant->type = constant_type;
        
        ConstantParser_t parser = constant_parsers[constant_type];
        FAIL_SET_JUMP(parser,err,JERR_BADPARAM,exit);
        FAIL_SET_JUMP((constant->value = parser(loader->arena,stream)),err,JERR_BADPARAM,exit);

        if(constant->type == EJCT_DOUBLE || constant->type == EJCT_LONG) i++;
    }

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->flags) == MSERR_OK,err,JERR_BADPARAM,exit);

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->this_class) == MSERR_OK,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->super_class) == MSERR_OK,err,JERR_BADPARAM,exit);

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->interfaces_count) == MSERR_OK,err,JERR_BADPARAM,exit);

    new_class->interfaces = bumper_calloc(loader->arena,new_class->interfaces_count,sizeof(*new_class->interfaces));
    FAIL_SET_JUMP(new_class->interfaces,err,JERR_OOM,exit);

    for(unsigned i = 0; i < new_class->interfaces_count; i++){
        FAIL_SET_JUMP(memstream_readU16(stream,&new_class->interfaces[i]) == MSERR_OK,err,JERR_BADPARAM,exit);
    }

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->fields_count) == MSERR_OK,err,JERR_BADPARAM,exit);

    new_class->fields = bumper_calloc(loader->arena,new_class->fields_count,sizeof(*new_class->fields));
    FAIL_SET_JUMP(new_class->fields,err,JERR_OOM,exit);    

    for(unsigned i = 0; i < new_class->fields_count; i++){
        JRawField_t* field = &new_class->fields[i];
        FAIL_SET_JUMP(memstream_readU16(stream,&field->flags) == MSERR_OK,err,JERR_BADPARAM,exit);
        FAIL_SET_JUMP(memstream_readU16(stream,&field->name_index) == MSERR_OK,err,JERR_BADPARAM,exit);
        FAIL_SET_JUMP(memstream_readU16(stream,&field->descriptor_index) == MSERR_OK,err,JERR_BADPARAM,exit);

        uint16_t attributes_count = 0;
        FAIL_SET_JUMP(memstream_readU16(stream,&attributes_count) == MSERR_OK,err,JERR_BADPARAM,exit);

        INIT_LIST_HEAD(&field->attributes);
        for(unsigned i = 0; i < attributes_count; i++){
            FAIL_SET_JUMP(parse_attributes(&field->attributes, new_class, loader->arena, stream) == JERR_OK,err,JERR_BADPARAM,exit);
        }
    }

    FAIL_SET_JUMP(memstream_readU16(stream,&new_class->methods_count) == MSERR_OK,err,JERR_BADPARAM,exit);

    new_class->methods = bumper_calloc(loader->arena,new_class->methods_count,sizeof(*new_class->methods));
    FAIL_SET_JUMP(new_class->methods,err,JERR_OOM,exit);    

    for(unsigned i = 0; i < new_class->methods_count; i++){
        JRawMethod_t* method = &new_class->methods[i];
        FAIL_SET_JUMP(memstream_readU16(stream,&method->flags) == MSERR_OK,err,JERR_BADPARAM,exit);
        FAIL_SET_JUMP(memstream_readU16(stream,&method->name_index) == MSERR_OK,err,JERR_BADPARAM,exit);
        FAIL_SET_JUMP(memstream_readU16(stream,&method->descriptor_index) == MSERR_OK,err,JERR_BADPARAM,exit);

        uint16_t attributes_count = 0;
        FAIL_SET_JUMP(memstream_readU16(stream,&attributes_count) == MSERR_OK,err,JERR_BADPARAM,exit);

        INIT_LIST_HEAD(&method->attributes);
        for(unsigned i = 0; i < attributes_count; i++){
            FAIL_SET_JUMP(parse_attributes(&method->attributes, new_class, loader->arena, stream) == JERR_OK,err,JERR_BADPARAM,exit);
        }
    }

    uint16_t attributes_count = 0;
    FAIL_SET_JUMP(memstream_readU16(stream,&attributes_count) == MSERR_OK,err,JERR_BADPARAM,exit);

    INIT_LIST_HEAD(&new_class->attributes);
    for(unsigned i = 0; i < attributes_count; i++){
        FAIL_SET_JUMP(parse_attributes(&new_class->attributes, new_class, loader->arena, stream) == JERR_OK,err,JERR_BADPARAM,exit);
    }

    loader->num_loaded++;
    list_add(&new_class->list,&loader->classes);
exit:
    return err;
}