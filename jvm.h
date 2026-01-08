#pragma once
#include "arena.h"
#include "list.h"
#include "os_support.h"

#include "jvm_method.h"

#include <stdatomic.h>

typedef enum{
    EJVT_BYTE = 'B',
    EJVT_CHAR = 'C',
    EJVT_DOUBLE = 'D',
    EJVT_FLOAT = 'F',
    EJVT_INT = 'I',
    EJVT_LONG = 'J',
    EJVT_REFERENCE = 'L',
    EJVT_SHORT = 'S',
    EJVT_BOOL = 'Z',
    EJVT_ARRAYDIM = '[',
    EJVT_VOID = 'V',
    EJVT_CODEADDR = 'A',
    EJVT_NATIVEPTR = '*',
}jvm_value_type_t;

typedef struct{
    jvm_value_type_t type;
    uint8_t value[sizeof(uint64_t)];
}jvm_value_t;

typedef struct{
    uint16_t sp;
    jvm_value_t* stack;
}jvm_stack_t;

typedef struct objectmanager_object_t objectmanager_object_t;
typedef struct{
    struct list_head list;
    objectmanager_object_t* exception_object;
}jvm_native_exception_t;

typedef struct jvm_instance_t jvm_instance_t;
typedef struct jvm_frame_t jvm_frame_t;
typedef struct classlinker_method_t classlinker_method_t;
typedef struct classlinker_instance_t classlinker_instance_t;
typedef struct jvm_frame_t{
    jvm_instance_t* jvm;

    struct list_head native_exceptions; //Linked list for handling exception in native methods
    classlinker_method_t* method;

    int64_t pc; //Negative because of some for loop issues with gotos
    jvm_stack_t stack;
    jvm_value_t* locals;

    jvm_frame_t* previous_frame;
}jvm_frame_t;

typedef struct{
    struct list_head list;
    jvm_frame_t* topmost_frame;
    objectmanager_object_t* JThread;
}jvm_thread_t;

typedef struct{ //I hate C include system sometimes
    struct list_head object_list;
    Arena* gc_heap;
    uint32_t gc_heapsize;
}objectmanager_heap_t;

typedef struct jvm_instance_t{
    Arena* arena;
    classlinker_instance_t* linker;

    objectmanager_heap_t heap;

    atomic_uint thread_count;
    struct list_head threads;

    pthread_mutex_t lock;
    pthread_cond_t jvm_exit_wait;
}jvm_instance_t;

jvm_instance_t* jvm_new(classlinker_instance_t* linker, uint32_t heap_size); //Creates new jvm
jvm_error_t jvm_launch_class(jvm_instance_t* instance, char* class, int nargs, char** args); //Launches specified class
void jvm_wait_exit(jvm_instance_t* instance);

//NOTE ABOUT LJNI: Allocations done by native function and doesnt stored in classes, objects, frame locals, frame stack are invisible to GC, and it will treat them as unused!
//So if you need to allocate something from native function, please store it to local variable immeadiatly after you allocate. like this: C_TO_JVM_VALUE(frame->locals[N],objectmanager_new_*_object)!!!!
//Or lock the jvm!
jvm_error_t jvm_invoke(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]);
                        //Invokes function, bytecode or native, from passed method with passed arguments. Pass 'this' as args[0]!

jvm_error_t jvm_invokestatic(jvm_instance_t* instance, jvm_frame_t* previous_frame, classlinker_method_t* callable_method, unsigned nargs, jvm_value_t args[]);
                        //Same but check that this function is static

jvm_error_t jvm_throw(jvm_frame_t* frame, objectmanager_object_t* expection_object); //Throw exception object to previous java or native function

objectmanager_object_t* jvm_native_catch_exception(jvm_frame_t* frame); //Catches exceptions in native function. Warning: if you call something that might throw
                                                                        //And you dont want to catch, you MUST re-throw thoose exceptions

void jvm_native_return(jvm_frame_t* frame, jvm_value_t value); //Use this function to return value from LJNI to java function or other native function. Or to get value from stack

jvm_value_t jvm_native_get_return(jvm_frame_t* frame); //Gets function return value or any value from stack

#define C_TO_JVM_TYPE(x) _Generic((x), \
    int8_t: EJVT_BYTE, \
    uint8_t: EJVT_BYTE, \
    char: EJVT_CHAR, \
    int16_t: EJVT_SHORT, \
    uint16_t: EJVT_CHAR, \
    int32_t: EJVT_INT, \
    uint32_t: EJVT_INT, \
    int64_t: EJVT_LONG, \
    uint64_t: EJVT_LONG, \
    float: EJVT_FLOAT, \
    double: EJVT_DOUBLE, \
    bool: EJVT_BOOL, \
    objectmanager_object_t*: EJVT_REFERENCE, \
    void*: EJVT_NATIVEPTR, \
    default: EJVT_NATIVEPTR)


#define C_TO_JVM_VALUE(jvm_value,C_value) {(jvm_value).type = C_TO_JVM_TYPE((C_value)); *(typeof(C_value)*)(jvm_value).value = (C_value);} //Macro to convert C to jvm_value_t
#define C_TO_NEW_JVM_VALUE(C_value) ({jvm_value_t new_value = {0}; C_TO_JVM_VALUE(new_value,C_value); (new_value);})
#define JVM_TO_C_VALUE(jvm_value,type) *(type*)(jvm_value).value //Macro to convert jvm_value_t to native value, require type. For example to get EJVT_INT you pass uint32_t,
                                                                 //For EJVT_REFERENCE: objectmanager_object_t* and likewise for other types