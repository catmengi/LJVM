#include "object.h"
#include "class.h"
#include "config.h"
#include "jerror.h"

#include <stdlib.h>

static void* s_handle_table[OBJECTS_MAX] = {0};
static uint16_t s_sort_table[OBJECTS_MAX] = {0};
static uint32_t s_alloc_table[OBJECTS_MAX / 32] = {0};

/**
 * Allocate an index from the handle table.
 * Returns the index (0 .. OBJECTS_MAX-1), or -1 if all slots are in use.
 */
static int alloc_handle_index(void){
    size_t num_words = OBJECTS_MAX / 32;

    for (size_t word_idx = 0; word_idx < num_words; ++word_idx) {
        uint32_t word = s_alloc_table[word_idx];

        /* If the word is not completely full, it contains at least one zero bit */
        if (word != 0xFFFFFFFF) {
            /* Invert to make zero bits become ones; then find the lowest set bit */
            uint32_t free_word = ~word;
            int bit = __builtin_ctz(free_word);      // count trailing zeros

            /* Mark the bit as used */
            s_alloc_table[word_idx] = word | (1U << bit);

            int index = (int)(word_idx * 32 + bit);

            /* Optional: initialise the newly allocated slot */
            s_handle_table[index] = NULL;
            s_sort_table[index]   = 0;

            return index;
        }
    }

    return -1;   /* no free slot */
}

/**
 * Free a previously allocated index.
 * Does nothing if the index is out of range.
 */
static void free_handle_index(int index){
    if (index < 0 || index >= OBJECTS_MAX)
        return;

    int word_idx = index / 32;
    int bit      = index % 32;

    /* Clear the bit in the allocation table */
    s_alloc_table[word_idx] &= ~(1U << bit);

    /* Optional: clear the associated data */
    s_handle_table[index] = NULL;
    s_sort_table[index]   = 0;
}


Error_t object_alloc(Class_t* class, uint32_t* output){
    Error_t err = JERR_OK;

    int index = alloc_handle_index();
    FAIL_SET_JUMP(index >= 0, err, JERR_OOM, exit);

    void** handle = &s_handle_table[index];
    void* object_chunk = calloc(1, sizeof(Object_t) + class->object_size);
    assert(object_chunk);

    Object_t* object = object_chunk;
    object->class = class;
    object->type = 0; 
    object->size = sizeof(Object_t) + class->object_size;
    object->data = object_chunk + sizeof(Object_t);

    *handle = object;

exit:
    return err;
}