#include "../../../native_methods_service.h"
#include "../../../monitor.h"
#include "../../../stringpool.h"
#include "../../../converter.h"
#include "../../../heap.h"

#include <stdint.h>
#include <string.h>

static NativeMethodReturnValue_t get_cstr(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(int64_t*)retval.value = (int32_t)stringpool_get(args[0]);

    return retval;
}

static NativeMethodReturnValue_t cstr_utf16_length(Interpreter_t* ctx, Method_t* self, int32_t* args){
    char* str = (char*)(int32_t)((int64_t*)args)[0];

    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(int32_t*)retval.value = utf8_to_utf16((utf8_t*)str, strlen(str), NULL, 0);

    return retval;
}

static NativeMethodReturnValue_t cstr_utf16_convert(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {.err = JERR_OK};

    char* str = (char*)(int32_t)((int64_t*)args)[0];
    Object_t* char_array = (Object_t*)args[2];

    int32_t utf16_length = 0;
    utf16_t* utf16_string = NULL;
    FAIL_SET_JUMP((retval.err = heap_array_object_get_elements(char_array, (void**)&utf16_string)) == JERR_OK, retval, retval, exit);
    FAIL_SET_JUMP((retval.err = heap_array_object_get_length(char_array, &utf16_length)) == JERR_OK, retval, retval, exit);

    utf8_to_utf16((utf8_t*)str, strlen(str), utf16_string, utf16_length);

exit:
    return retval;
}

static NativeMethodReturnValue_t utf8_length(Interpreter_t* ctx,  Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {.err = JERR_OK};

    utf16_t* utf16 = NULL;
    int32_t utf16_length = 0;

    FAIL_SET_JUMP((retval.err = heap_array_object_get_elements((Object_t*)args[0], (void**)&utf16)) == JERR_OK, retval, retval, exit);
    FAIL_SET_JUMP((retval.err = heap_array_object_get_length((Object_t*)args[0], &utf16_length)) == JERR_OK, retval, retval, exit);

    *(int32_t*)retval.value = utf16_to_utf8(utf16, utf16_length, NULL, 0);

exit:
    return retval;
}

static NativeMethodReturnValue_t utf16_utf8_convert(Interpreter_t* ctx,  Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {.err = JERR_OK};

    utf16_t* utf16 = NULL;
    int32_t utf16_length = 0;

    utf8_t* utf8 = NULL;
    int32_t utf8_length = 0;

    FAIL_SET_JUMP((retval.err = heap_array_object_get_elements((Object_t*)args[0], (void**)&utf16)) == JERR_OK, retval, retval, exit);
    FAIL_SET_JUMP((retval.err = heap_array_object_get_length((Object_t*)args[0], &utf16_length)) == JERR_OK, retval, retval, exit);

    FAIL_SET_JUMP((retval.err = heap_array_object_get_elements((Object_t*)args[1], (void**)&utf8)) == JERR_OK, retval, retval, exit);
    FAIL_SET_JUMP((retval.err = heap_array_object_get_length((Object_t*)args[1], &utf8_length)) == JERR_OK, retval, retval, exit);

    utf16_to_utf8(utf16, utf16_length, utf8, utf8_length);

exit:
    return retval;
}

NativeClass_t java_lang_String = {
    .name = "java/lang/String",
    .methods_count = 5,
    .methods = (NativeMethodDescriptor_t[]){
        {"get_cstr@(I)J", get_cstr},
        {"cstr_utf16_length@(J)I", cstr_utf16_length},
        {"cstr_utf16_convert@(J[C)V", cstr_utf16_convert},
        {"utf8_length@([C)I", utf8_length},
        {"utf16_utf8_convert@([C[B)V", utf16_utf8_convert},
    },
};