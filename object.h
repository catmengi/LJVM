#pragma once

#include "bumper.h"
#include "class.h"
#include "list.h"

typedef struct{ //This structure is basically a header for Java object. To get object itself content = ((uint8_t*)JObject_t* + sizeof(JObject_t))
    uint32_t magic; //Should be random every JVM startup. Will be used for stack scanning
    JClass_t* class;
    size_t object_size; //Raw size in bytes

    union{
        bool value;
        struct{
            bool is_array:1;
            bool is_scanned:1; //GC will mark this byte if it already scanned through this object
            bool is_weakref:1; //If this flags is 1, then GC should abort scanning of this object.
        };
    }flags;
}JObject_t;

typedef struct{
    JValue_type_t type;
    unsigned length;

    uint8_t* items; //offset to item itself is (index * JValue_sizeof(type))
}JArray_t;

typedef struct JHeap_t{
    bump_allocator_t arena;
    void* heap_start, *heap_end;
}JHeap_t;