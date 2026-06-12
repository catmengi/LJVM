#include "config.h"

#include "parser.h"
#include "bumper.h"
#include "list.h"

#include "jerror.h"
#include "stream.h"

#include <assert.h>
#include <string.h>

#define JMAGIC 0xCAFEBABE

static bump_allocator_t s_parser_arena = {0}; 
static bool s_initalised = false;

void parser_init(){
    if(!s_initalised){
        assert(bumper_create(&s_parser_arena, PARSER_ARENA) == 0);
        s_initalised = true;
    } else bumper_reset(&s_parser_arena);
}

static Error_t init_constantpool(JConstantPool_t* constantpool, unsigned size, bump_allocator_t* allocate_from){
    assert(size > 0);

    constantpool->count = size;
    constantpool->constants = bumper_calloc(allocate_from,constantpool->count,sizeof(*constantpool->constants));
    if(!constantpool->constants) return JERR_OOM;

    return JERR_OK;
}

JConstant_t* parser_constantpool_get(JConstantPool_t* constantpool, unsigned index){
    return index < constantpool->count ? &constantpool->constants[index] : NULL;
}

//Parser errors are fatal, so this can be considered OK
typedef void* (*ConstantParser_t)(ClassStream_t* stream);

static void* Cparse_utf8(ClassStream_t* stream){
    void* ret = NULL;

    JRawUTF8_t* utf8_constant = bumper_alloc(&s_parser_arena,sizeof(*utf8_constant));
    FAIL_SET_JUMP(utf8_constant,ret,NULL,exit);

    FAIL_SET_JUMP(classstream_readU16(stream,&utf8_constant->length) == 0,ret,NULL,exit);

    utf8_constant->string = bumper_alloc(&s_parser_arena,utf8_constant->length + 1);
    FAIL_SET_JUMP(utf8_constant->string,ret,NULL,exit);

    FAIL_SET_JUMP(classstream_readBUF(stream,utf8_constant->string,utf8_constant->length) == 0,ret,NULL,exit);
    utf8_constant->string[utf8_constant->length] = '\0'; //Null terminating it

    ret = utf8_constant;
exit:
    return ret;
}

static void* Cparse_class(ClassStream_t* stream){
    void* ret = NULL;

    uint16_t name_index = 0;
    FAIL_SET_JUMP(classstream_readU16(stream,&name_index) == 0,ret,NULL,exit);

    uint16_t* constant_info = bumper_alloc(&s_parser_arena,sizeof(name_index));
    FAIL_SET_JUMP(constant_info,ret,NULL,exit);

    *constant_info = name_index;
    ret = constant_info;
exit:
    return ret;
}

static void* Cparse_string(ClassStream_t* stream){
    void* ret = NULL;

    uint16_t index = 0;
    FAIL_SET_JUMP(classstream_readU16(stream,&index) == 0,ret,NULL,exit);

    uint16_t* string_index = bumper_alloc(&s_parser_arena,sizeof(index));
    FAIL_SET_JUMP(string_index,ret,NULL,exit);

    *string_index = index;
    ret = string_index;
exit:
    return ret;
}

static void* Cparse_u32(ClassStream_t* stream){
    void* ret = NULL;

    uint32_t* u32 = bumper_alloc(&s_parser_arena,sizeof(*u32));
    FAIL_SET_JUMP(u32,ret,NULL,exit);

    FAIL_SET_JUMP(classstream_readU32(stream,u32) == 0,ret,NULL,exit);
    ret = u32;
exit:
    return ret;
}

static void* Cparse_u64(ClassStream_t* stream){
    void* ret = NULL;

    uint64_t* u64 = bumper_alloc(&s_parser_arena,sizeof(*u64));
    FAIL_SET_JUMP(u64,ret,NULL,exit);

    FAIL_SET_JUMP(classstream_readU64(stream,u64) == 0,ret,NULL,exit);
    ret = u64;
exit:
    return ret;
}

static void* Cparse_fmim(ClassStream_t* stream){
    void* ret = NULL;

    JRaw_FMIM_ref_t* fmim_ref = bumper_alloc(&s_parser_arena,sizeof(*fmim_ref));
    FAIL_SET_JUMP(fmim_ref,ret,NULL,exit);

    FAIL_SET_JUMP(classstream_readU16(stream,&fmim_ref->class_index) == 0,ret,NULL,exit);
    FAIL_SET_JUMP(classstream_readU16(stream,&fmim_ref->nameandtype_index) == 0,ret,NULL,exit);

    ret = fmim_ref;
exit:
    return ret;
}

static void* Cparse_nameandtype(ClassStream_t* stream){
    void* ret = NULL;

    JRawNameAndType_t* nameandtype = bumper_alloc(&s_parser_arena,sizeof(*nameandtype));
    FAIL_SET_JUMP(nameandtype, ret, NULL, exit);

    FAIL_SET_JUMP(classstream_readU16(stream,&nameandtype->name_index) == 0,ret,NULL,exit);
    FAIL_SET_JUMP(classstream_readU16(stream,&nameandtype->descriptor_index) == 0,ret,NULL,exit);
    
    ret = nameandtype;
exit:
    return ret;
}

static ConstantParser_t constant_parsers[] = {
    [EJCT_UTF8] = Cparse_utf8, 
    [EJCT_CLASS] = Cparse_class,
    [EJCT_FIELDREF] = Cparse_fmim,
    [EJCT_METHODREF] = Cparse_fmim,
    [EJCT_INTERFACE_METHODREF] = Cparse_fmim,
    [EJCT_STRING] = Cparse_string,
    [EJCT_INT] = Cparse_u32,
    [EJCT_FLOAT] = Cparse_u32,
    [EJCT_LONG] = Cparse_u64,
    [EJCT_DOUBLE] = Cparse_u64,
    [EJCT_NAMEANDTYPE] = Cparse_nameandtype,
};

typedef void* (*AttributeInfoParserFN_t)(JRawClass_t* current_class, ClassStream_t* stream);
typedef struct{
    char* name;
    JRawAttributeType_t type;
    AttributeInfoParserFN_t parser;
}AttributeInfoParserDesc_t;

static Error_t parse_attributes(struct list_head* output_list,JRawClass_t* current_class, ClassStream_t* stream);
static void* code_parse(JRawClass_t* current_class, ClassStream_t* stream){
    JCodeAttribute_t* code = bumper_calloc(&s_parser_arena,1,sizeof(*code));
    if(!code) return NULL;

    if(classstream_readU16(stream,&code->max_stack) != 0) return NULL;
    if(classstream_readU16(stream,&code->max_locals) != 0) return NULL;
    if(classstream_readU32(stream,&code->code_length) != 0) return NULL;

    code->code = bumper_alloc(&s_parser_arena,code->code_length);
    if(!code->code) return NULL;

    if(classstream_readBUF(stream, code->code, code->code_length) != 0) return NULL;
    if(classstream_readU16(stream,&code->exception_table_length) != 0) return NULL;

    code->exception_table = bumper_calloc(&s_parser_arena,code->exception_table_length,sizeof(*code->exception_table));
    if(!code->exception_table) return NULL;

    for(unsigned i = 0; i < code->exception_table_length; i++){
        typeof(*code->exception_table)* exception = &code->exception_table[i];
        if(classstream_readU16(stream,&exception->start_pc) != 0) return NULL;
        if(classstream_readU16(stream,&exception->end_pc) != 0) return NULL;
        if(classstream_readU16(stream,&exception->handler_pc) != 0) return NULL;
        if(classstream_readU16(stream,&exception->catch_type) != 0) return NULL;
    }

    uint16_t attributes_count = 0;
    if(classstream_readU16(stream,&attributes_count) != 0) return NULL;

    INIT_LIST_HEAD(&code->attributes);
    for(unsigned i = 0; i < attributes_count; i++){
        if(parse_attributes(&code->attributes, current_class, stream) != JERR_OK) return NULL;
    }   

    return code;
}

static void* constantvalue_parse(JRawClass_t* current_class, ClassStream_t* stream){
    uint16_t* constant_index = bumper_alloc(&s_parser_arena,sizeof(*constant_index));
    if(!constant_index) return NULL;
    if(classstream_readU16(stream, constant_index) != 0) return NULL;

    return constant_index;
}

static AttributeInfoParserDesc_t attribute_parsers[] = {
    {"Code",EJAT_CODE,code_parse},
    {"ConstantValue",EJAT_CONSTANTVALUE,constantvalue_parse},
    //TODO: StackMap from cldc
};

static Error_t parse_attributes(struct list_head* output_list,JRawClass_t* current_class, ClassStream_t* stream){
    Error_t err = JERR_OK;

    uint16_t name_index = 0;
    uint32_t length = 0;
    FAIL_SET_JUMP(classstream_readU16(stream,&name_index) == 0,err,JERR_BADPARAM,exit);
    FAIL_SET_JUMP(classstream_readU32(stream,&length) == 0,err,JERR_BADPARAM,exit)

    JConstant_t* name_constant = parser_constantpool_get(&current_class->constantpool, name_index);
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
        JRawAttribute_t* new_attribute = bumper_calloc(&s_parser_arena,1,sizeof(*new_attribute));
        FAIL_SET_JUMP(new_attribute,err,JERR_OOM,exit);

        INIT_LIST_HEAD(&new_attribute->list);

        new_attribute->type = attribute_parser->type;
        new_attribute->info = attribute_parser->parser(current_class,stream);
        FAIL_SET_JUMP(new_attribute->info,err,JERR_UNKNOWN,exit);

        list_add(&new_attribute->list,output_list);
    } else FAIL_SET_JUMP(classstream_skip(stream,length) == 0,err,JERR_BADPARAM,exit);

exit:
    return err;
}




JRawClass_t* parser_parse_class(ClassStream_t* stream){
    bumper_reset(&s_parser_arena);
    JRawClass_t* ret = NULL;

    FAIL_SET_JUMP(stream, ret, NULL, exit);

    uint32_t magic = 0;
    FAIL_SET_JUMP(classstream_readU32(stream,&magic) == 0,ret, NULL,exit);
    FAIL_SET_JUMP(magic == JMAGIC, ret, NULL,exit);

    JRawClass_t* new_class = bumper_calloc(&s_parser_arena,1,sizeof(*new_class));

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->version.minor) == 0,ret, NULL,exit);
    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->version.major) == 0,ret, NULL,exit);

    uint16_t constantpool_size = 0;
    FAIL_SET_JUMP(classstream_readU16(stream,&constantpool_size) == 0,ret, NULL,exit);
    FAIL_SET_JUMP(init_constantpool(&new_class->constantpool, constantpool_size, &s_parser_arena) == JERR_OK,ret, NULL,exit);

    JConstantPool_t* constant_pool = &new_class->constantpool;
    for(unsigned i = 1; i < constantpool_size; i++){
        JConstant_t* constant = parser_constantpool_get(constant_pool, i);
        assert(constant);

        uint8_t constant_type = EJCT_NULL;
        FAIL_SET_JUMP(classstream_readU8(stream,&constant_type) == 0,ret, NULL,exit);

        constant->type = constant_type;
        
        ConstantParser_t parser = constant_parsers[constant_type];
        FAIL_SET_JUMP(parser, ret, NULL,exit);
        FAIL_SET_JUMP((constant->value = parser(stream)),ret, NULL,exit);

        if(constant->type == EJCT_DOUBLE || constant->type == EJCT_LONG) i++;
    }

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->flags) == 0,ret, NULL,exit);

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->this_class) == 0,ret, NULL,exit);
    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->super_class) == 0,ret, NULL,exit);

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->interfaces_count) == 0,ret, NULL,exit);

    new_class->interfaces = bumper_calloc(&s_parser_arena,new_class->interfaces_count,sizeof(*new_class->interfaces));
    FAIL_SET_JUMP(new_class->interfaces,ret, NULL,exit);

    for(unsigned i = 0; i < new_class->interfaces_count; i++){
        FAIL_SET_JUMP(classstream_readU16(stream,&new_class->interfaces[i]) == 0,ret, NULL,exit);
    }

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->fields_count) == 0,ret, NULL,exit);

    new_class->fields = bumper_calloc(&s_parser_arena,new_class->fields_count,sizeof(*new_class->fields));
    FAIL_SET_JUMP(new_class->fields,ret, NULL,exit);    

    for(unsigned i = 0; i < new_class->fields_count; i++){
        JRawField_t* field = &new_class->fields[i];
        FAIL_SET_JUMP(classstream_readU16(stream,&field->flags) == 0,ret, NULL,exit);
        FAIL_SET_JUMP(classstream_readU16(stream,&field->name_index) == 0,ret, NULL,exit);
        FAIL_SET_JUMP(classstream_readU16(stream,&field->descriptor_index) == 0,ret, NULL,exit);

        uint16_t attributes_count = 0;
        FAIL_SET_JUMP(classstream_readU16(stream,&attributes_count) == 0,ret, NULL,exit);

        INIT_LIST_HEAD(&field->attributes);
        for(unsigned i = 0; i < attributes_count; i++){
            FAIL_SET_JUMP(parse_attributes(&field->attributes, new_class, stream) == JERR_OK,ret, NULL,exit);
        }
    }

    FAIL_SET_JUMP(classstream_readU16(stream,&new_class->methods_count) == 0,ret, NULL,exit);

    new_class->methods = bumper_calloc(&s_parser_arena,new_class->methods_count,sizeof(*new_class->methods));
    FAIL_SET_JUMP(new_class->methods,ret, NULL,exit);    

    for(unsigned i = 0; i < new_class->methods_count; i++){
        JRawMethod_t* method = &new_class->methods[i];
        FAIL_SET_JUMP(classstream_readU16(stream,&method->flags) == 0,ret, NULL,exit);
        FAIL_SET_JUMP(classstream_readU16(stream,&method->name_index) == 0,ret, NULL,exit);
        FAIL_SET_JUMP(classstream_readU16(stream,&method->descriptor_index) == 0,ret, NULL,exit);

        uint16_t attributes_count = 0;
        FAIL_SET_JUMP(classstream_readU16(stream,&attributes_count) == 0,ret, NULL,exit);

        INIT_LIST_HEAD(&method->attributes);
        for(unsigned i = 0; i < attributes_count; i++){
            FAIL_SET_JUMP(parse_attributes(&method->attributes, new_class, stream) == JERR_OK,ret, NULL,exit);
        }
    }

    uint16_t attributes_count = 0;
    FAIL_SET_JUMP(classstream_readU16(stream,&attributes_count) == 0,ret, NULL,exit);

    INIT_LIST_HEAD(&new_class->attributes);
    for(unsigned i = 0; i < attributes_count; i++){
        FAIL_SET_JUMP(parse_attributes(&new_class->attributes, new_class, stream) == JERR_OK,ret, NULL,exit);
    }

    ret = new_class;
exit:
    return ret;
}