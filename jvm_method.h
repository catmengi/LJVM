#pragma once

typedef struct jvm_frame_t jvm_frame_t;
typedef enum{
    JVM_UNKNOWN = -1,
    JVM_OK = 0,
    JVM_OOM,
    JVM_NOTFOUND,
    JVM_OPCODE_INVALID,
    JVM_OPPARAM_INVALID,
    JVM_METHOD_RETURN,
    JVM_ARRAY_OUTOFBOUNDS,
}jvm_error_t;
typedef jvm_error_t (*jvm_method_t)(jvm_frame_t* frame);
