#include "../../../native_methods_service.h"
#include "../../../heap.h"
#include "../../../stringpool.h"
#include "../../../jerror.h"

#include <assert.h>
#include <string.h>

static NativeMethodReturnValue_t arraycopy(Interpreter_t* ctx, Method_t* self, int32_t* args){
    Object_t* src = (Object_t*)args[0];
    int32_t src_pos = args[1];
    Object_t* dst = (Object_t*)args[2];
    int32_t dst_pos = args[3]; 
    int32_t length = args[4];  
    
    if(!src || !dst) return (NativeMethodReturnValue_t){JERR_NULLPOINTER, {0}};

    if(!src->class->flags.is_array || !dst->class->flags.is_array || (src->class->array_type != dst->class->array_type)){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;

        assert(class_load_bynameid(stringpool_add("java/lang/ArrayStoreException"), &exception_class) == JERR_OK);
        assert(heap_class_object_alloc(exception_class, &exception) == JERR_OK);
        assert(interpreter_method_invoke(ctx, class_find_method(exception_class, stringpool_add("<init>@()V")), NULL, NULL) == JERR_OK);

        NativeMethodReturnValue_t retval = {0};
        retval.err = JERR_EXCEPTION;
        *(Object_t**)retval.value = exception;
        
        return retval;
    }

    int32_t src_length = 0;
    int32_t dst_length = 0;
    assert(heap_array_object_get_length(src, &src_length) == JERR_OK);
    assert(heap_array_object_get_length(dst, &dst_length) == JERR_OK);

    if(src_pos < 0 || dst_pos < 0 || length < 0 || src_pos + length > src_length || dst_pos + length > dst_length){
        Class_t* exception_class = NULL;
        Object_t* exception = NULL;

        assert(class_load_bynameid(stringpool_add("java/lang/IndexOutOfBoundsException"), &exception_class) == JERR_OK);
        assert(heap_class_object_alloc(exception_class, &exception) == JERR_OK);
        assert(interpreter_method_invoke(ctx, class_find_method(exception_class, stringpool_add("<init>@()V")), NULL, NULL) == JERR_OK);

        NativeMethodReturnValue_t retval = {0};
        retval.err = JERR_EXCEPTION;
        *(Object_t**)retval.value = exception;
        
        return retval;       
    }

    void* src_data = NULL;
    void* dst_data = NULL;

    assert(heap_array_object_get_elements(src, &src_data) == JERR_OK);
    assert(heap_array_object_get_elements(dst, &dst_data) == JERR_OK);

    size_t element_size = heap_array_type_size(src->class->array_type);
    void* src_data_offseted = (char*)src_data + (src_pos * element_size);
    void* dst_data_offseted = (char*)dst_data + (dst_pos * element_size);

    memmove(dst_data_offseted, src_data_offseted, length * heap_array_type_size(dst->class->array_type));

    return (NativeMethodReturnValue_t){JERR_OK, {0}};
}

static NativeMethodReturnValue_t currentTimeMillis(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(int64_t*)retval.value = thread_time_ns_get() / 1000000;

    return retval;
}

static NativeMethodReturnValue_t gc(Interpreter_t* ctx, Method_t* self, int32_t* args){
    heap_gc_start();
    return (NativeMethodReturnValue_t){JERR_OK, {0}};
}

static NativeMethodReturnValue_t identityHashCode(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(int32_t*)retval.value = ((Object_t*)args[0])->ident;

    return retval;
}
   
static NativeMethodReturnValue_t vm_exit(Interpreter_t* ctx,  Method_t* self, int32_t* args){
    //TODO: change it to proper exit
    exit(args[0]);
}

NativeClass_t java_lang_System = {
    .name = "java/lang/System",
    .methods_count = 5,
    .methods = (NativeMethodDescriptor_t[]){
        {"arraycopy@(Ljava/lang/Object;ILjava/lang/Object;II)V", arraycopy},
        {"currentTimeMillis@()J",currentTimeMillis},
        {"gc@()V",gc},
        {"identityHashCode@(Ljava/lang/Object;)I", identityHashCode},
        {"exit@(I)V", vm_exit},
    },
};