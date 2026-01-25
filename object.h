#pragma once

#include "bumper.h"
#include "class.h"
#include "list.h"

#define JHEAP_DWORD_PTR

typedef struct{ //This structure is basically a header for Java object. To get object itself content = ((uint8_t*)JObject_t* + sizeof(JObject_t))
    uint32_t magic; //0xCABEBABE
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
}JArray_t;

typedef struct JHeap_t JHeap_t;
typedef struct JHeap_t{
    bump_allocator_t heap_arena;
    bump_allocator_t gc_scratchpad; 
    void* heap_start, *heap_end;

    bool (*gc)(JHeap_t* heap, unsigned required_memory);
}JHeap_t;

JHeap_t* object_heap_create();
JObject_t* object_create(JClass_t* class);
JObject_t* array_create(unsigned length, JValue_type_t type,...);


JClass_t* object_get_class(JObject_t* object);
JError_t object_get_field(JObject_t* object, JField_t* field, void* output);
JError_t object_set_field(JObject_t* object, JField_t* field, void* value);

#ifdef JHEAP_DWORD_PTR

//SLOP CODE BELOW

/**
 * Compresses a 64-bit real pointer into a 32-bit representation.
 * This function correctly handles the case where the pointer might be the
 * same as the heap start, avoiding a collision with the NULL representation (0).
 *
 * @param heap_start The real 64-bit start address of the Java heap.
 * @param ptr The real 64-bit pointer to an object. Can be NULL.
 * @return The 32-bit compressed pointer. Returns 0 for NULL.
 */
static inline uint32_t ptr_compress(const uint8_t* heap_start, const void* ptr) {
    if (ptr == NULL) {
        return 0; // 0 is always reserved for NULL.
    }

    // THE FIX: Create a logical base that is 4 bytes *before* the actual heap start.
    // This ensures that the offset for the very first object (at heap_start) is 4,
    // which compresses to 1, thus avoiding a collision with NULL.
    const uintptr_t compression_base = (uintptr_t)heap_start - 4;

    // Calculate the offset from this logical base.
    uintptr_t offset = (uintptr_t)ptr - compression_base;

    // Scale down by 4 (since we assume 4-byte alignment).
    return (uint32_t)(offset >> 2);
}

/**
 * Decompresses a 32-bit pointer back into a 64-bit real pointer.
 *
 * @param heap_start The real 64-bit start address of the Java heap.
 * @param compressed_ptr The 32-bit compressed pointer.
 * @return The real 64-bit pointer. Can be NULL.
 */
static inline void* ptr_decompress(const uint8_t* heap_start, uint32_t compressed_ptr) {
    if (compressed_ptr == 0) {
        return NULL; // 0 is always reserved for NULL.
    }

    // THE FIX: Re-create the same logical base used during compression.
    const uintptr_t compression_base = (uintptr_t)heap_start - 4;

    // Scale up by 4 to get the real byte offset.
    uintptr_t offset = (uintptr_t)compressed_ptr << 2;

    // Add the offset to the logical base to reconstruct the real pointer.
    return (void*)(compression_base + offset);
}
#endif
