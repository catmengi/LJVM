#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// --- Types and Structs ---

// Forward declarations
typedef struct fht_t fht_t;
typedef struct fht_entry_t fht_entry_t;

// Pointer to a user-provided hash function.
typedef uint32_t (*fht_hash_func)(const void* key);
// Pointer to a user-provided key comparison function.
typedef bool (*fht_key_compare_func)(const void* key1, const void* key2);

// Internal table node. Also used to return results from the iterator.
struct fht_entry_t {
    const void* key;
    void* value;
    struct fht_entry_t* next; // For collision chaining
};

// The main hash table structure. Allocated by the user (e.g., on the stack).
struct fht_t {
    fht_entry_t** buckets;      // Pointer to the buckets array within the memory_buffer
    fht_entry_t* free_list;     // Head of the free nodes list
    size_t capacity;            // Number of buckets (equals max_entries)
    fht_hash_func hash_func;
    fht_key_compare_func compare_func;
};

// --- Iterator API ---

// Iterator struct to hold the state during iteration.
typedef struct {
    fht_t* ht;
    size_t bucket_index;
    fht_entry_t* current_node;
} fht_iterator_t;


// --- Helper functions for common key types ---

// FNV-1a hash function for strings
static inline uint32_t fht_hash_string(const void* key) {
    uint32_t hash = 0x811c9dc5;
    const char* str = (const char*)key;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
}

// Comparison function for strings
static inline bool fht_compare_string(const void* key1, const void* key2) {
    return strcmp((const char*)key1, (const char*)key2) == 0;
}

// Hash function for uint32_t (MurmurHash3-like finalizer)
static inline uint32_t fht_hash_uint32(const void* key) {
    uint32_t x = *(const uint32_t*)key;
    x ^= x >> 16;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    x *= 0xc2b2ae35;
    x ^= x >> 16;
    return x;
}

// Comparison function for uint32_t
static inline bool fht_compare_uint32(const void* key1, const void* key2) {
    return *(const uint32_t*)key1 == *(const uint32_t*)key2;
}


// --- Main API and Implementation ---

// Align a pointer up to the nearest multiple of 'align'
#define FHT_ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))

/**
 * @brief Calculates the exact memory size in bytes required for the hash table.
 * @param max_entries The maximum number of items that can be stored.
 * @return The required buffer size in bytes.
 */
static inline size_t fht_calculate_size(size_t max_entries) {
    size_t buckets_size = sizeof(fht_entry_t*) * max_entries;
    size_t entries_size = sizeof(fht_entry_t) * max_entries;
    return buckets_size + entries_size + sizeof(void*);
}

/**
 * @brief Initializes the hash table within a user-provided buffer.
 * @param ht Pointer to your fht_t struct.
 * @param memory_buffer Pointer to the pre-allocated buffer.
 * @param max_entries The maximum number of items, same as used for size calculation.
 * @param hash_func Pointer to the hash function.
 * @param compare_func Pointer to the key comparison function.
 */
static inline void fht_init(fht_t* ht, void* memory_buffer, size_t max_entries, fht_hash_func hash_func, fht_key_compare_func compare_func) {
    ht->capacity = max_entries;
    ht->hash_func = hash_func;
    ht->compare_func = compare_func;
    ht->buckets = (fht_entry_t**)memory_buffer;
    memset(ht->buckets, 0, sizeof(fht_entry_t*) * ht->capacity);
    uintptr_t entries_start = FHT_ALIGN_UP((uintptr_t)ht->buckets + sizeof(fht_entry_t*) * ht->capacity, sizeof(void*));
    fht_entry_t* entries_pool = (fht_entry_t*)entries_start;
    ht->free_list = entries_pool;
    for (size_t i = 0; i < max_entries - 1; ++i) {
        entries_pool[i].next = &entries_pool[i + 1];
    }
    entries_pool[max_entries - 1].next = NULL;
}

/**
 * @brief Adds or updates an item in the table. The key is NOT copied.
 * @return true on success, false if the table is out of space.
 */
static inline bool fht_set(fht_t* ht, const void* key, void* value) {
    size_t index = ht->hash_func(key) % ht->capacity;
    fht_entry_t* entry = ht->buckets[index];
    while (entry) {
        if (ht->compare_func(entry->key, key)) {
            entry->value = value;
            return true;
        }
        entry = entry->next;
    }
    if (!ht->free_list) return false;
    fht_entry_t* new_entry = ht->free_list;
    ht->free_list = new_entry->next;
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = ht->buckets[index];
    ht->buckets[index] = new_entry;
    return true;
}

/**
 * @brief Retrieves an item from the table.
 * @return A pointer to the value if found, otherwise NULL.
 */
static inline void* fht_get(fht_t* ht, const void* key) {
    size_t index = ht->hash_func(key) % ht->capacity;
    fht_entry_t* entry = ht->buckets[index];
    while (entry) {
        if (ht->compare_func(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief Deletes an item from the table.
 * @return true if the item was found and deleted, otherwise false.
 */
static inline bool fht_delete(fht_t* ht, const void* key) {
    size_t index = ht->hash_func(key) % ht->capacity;
    fht_entry_t* entry = ht->buckets[index];
    fht_entry_t* prev = NULL;
    while (entry) {
        if (ht->compare_func(entry->key, key)) {
            if (prev) prev->next = entry->next;
            else ht->buckets[index] = entry->next;
            entry->next = ht->free_list;
            ht->free_list = entry;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }
    return false;
}

/**
 * @brief Initializes an iterator for traversing the hash table.
 * @param ht The hash table to iterate over.
 * @param iter A pointer to the iterator struct to be initialized.
 */
static inline void fht_iterator_init(fht_t* ht, fht_iterator_t* iter) {
    iter->ht = ht;
    iter->bucket_index = (size_t)-1; // Start "before" the first bucket
    iter->current_node = NULL;
}

/**
 * @brief Advances the iterator and returns the next entry in the table.
 * @param iter A pointer to the iterator.
 * @return A pointer to the next fht_entry_t, or NULL if the iteration is complete.
 */
static inline fht_entry_t* fht_next(fht_iterator_t* iter) {
    // First, try to advance along the current bucket's linked list (chain).
    if (iter->current_node) {
        iter->current_node = iter->current_node->next;
        if (iter->current_node) {
            return iter->current_node; // Found next node in the same chain.
        }
    }

    // If the chain is exhausted, find the next non-empty bucket.
    for (size_t i = iter->bucket_index + 1; i < iter->ht->capacity; ++i) {
        if (iter->ht->buckets[i]) {
            iter->bucket_index = i;
            iter->current_node = iter->ht->buckets[i];
            return iter->current_node; // Found the first node in a new bucket.
        }
    }

    // If no more non-empty buckets are found, the iteration is complete.
    return NULL;
}

