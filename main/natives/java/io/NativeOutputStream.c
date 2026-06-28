#include "../../../native_methods_service.h"
#include "../../../heap.h"
#include "../../../stringpool.h"

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/select.h>

static NativeMethodReturnValue_t ns_open(Thread_t* thread, Method_t* method, int32_t* args){
    Object_t* path = (Object_t*)args[0];
    //int32_t flags = args[1];

    if(path == NULL){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL; 
        assert(class_load_bynameid(stringpool_add("java/lang/NullPointerException"), &exception_class) == JERR_OK);
        assert(heap_class_object_alloc(exception_class, &exception) == JERR_OK);
        assert(interpreter_method_invoke(class_find_method(exception_class, stringpool_add("<init>@()V")), (int32_t[1]){(int32_t)exception}, NULL) == JERR_OK); 

        NativeMethodReturnValue_t retval = {0};
        retval.err = JERR_EXCEPTION;
        *(Object_t**)retval.value = exception;
        
        return retval;            
    }

    int32_t* storage = NULL;
    assert(heap_class_object_get_fields(path, &storage) == JERR_OK);
    
    Field_t *value = NULL, *count = NULL, *offset = NULL;
    assert((value = class_find_instance_field(path->class, stringpool_add("value@[C"))));
    assert((count = class_find_instance_field(path->class, stringpool_add("count@I"))));
    assert((offset = class_find_instance_field(path->class, stringpool_add("offset@I"))));

    Object_t* char_array = (Object_t*)storage[value->offset];

    int16_t* chars = NULL;
    assert(heap_array_object_get_elements(char_array, (void**)&chars) == JERR_OK);

    int32_t real_length = storage[count->offset] - storage[offset->offset];
    int16_t* start = chars + storage[offset->offset];

    char path_str[real_length + 1];
    //wcstombs(path_str, (wchar_t*)start, real_length > sizeof(path_str) ? real_length : sizeof(path_str));

    //TODO: UTF8
    for(unsigned i = 0; i < real_length; i++){
        path_str[i] = start[i];
    }

    path_str[real_length] = '\0';

    NativeMethodReturnValue_t return_value = {0};
    return_value.err = JERR_OK;
    *(int*)return_value.value = open(path_str, O_WRONLY | O_CREAT, 0644);

    return return_value;
}

static NativeMethodReturnValue_t ns_close(Thread_t* thread, Method_t* method, int32_t* args){
    Object_t* self = (Object_t*)args[0];
    int32_t* storage = NULL;
    assert(heap_class_object_get_fields(self, &storage) == JERR_OK);

    Field_t *fd = NULL;
    assert((fd = class_find_instance_field(self->class, stringpool_add("fd@I"))));

    close(storage[fd->offset]);

    return (NativeMethodReturnValue_t){JERR_OK, {0}};
}

static NativeMethodReturnValue_t ns_write(Thread_t* thread, Method_t* method, int32_t* args){
    Object_t* self = (Object_t*)args[0];
    Object_t* buf = (Object_t*)args[1];

    static Field_t *fd_field = NULL;
    if(!fd_field) assert((fd_field = class_find_instance_field(self->class, stringpool_add("fd@I"))));

    int32_t* storage = NULL;
    assert(heap_class_object_get_fields(self, &storage) == JERR_OK);

    uint8_t* bytes = NULL;
    int32_t length = 0;

    assert(heap_array_object_get_elements(buf, (void**)&bytes) == JERR_OK);
    assert(heap_array_object_get_length(buf, &length) == JERR_OK);

    assert(write(storage[fd_field->offset], bytes, length) == length);
    return (NativeMethodReturnValue_t){JERR_OK, {0}};
}

NativeClass_t java_io_NativeOutputStream = {
    .name = "java/io/NativeOutputStream",
    .methods_count = 4,
    .methods = (NativeMethodDescriptor_t[]){
        {"open@(Ljava/lang/String;I)I", ns_open},
        {"close@()V",ns_close},
        {"flush@()V",(void*)0x123},
        {"write@([B)V", ns_write},
    },
};