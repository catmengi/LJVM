#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef void* (*hashmap_alloc_func_t)(void* ctx, size_t size);
typedef uint32_t (*hashmap_hash_func_t)(const void* key);
typedef int (*hashmap_cmp_func_t)(const void* key_a, const void* key_b); //strcmp semantic

typedef struct hashmap_entry {
    struct hashmap_entry* next;
    const void* key;
    void* value;
} hashmap_entry_t;

typedef struct {
    hashmap_alloc_func_t alloc_func;
    hashmap_hash_func_t hash_func;
    hashmap_cmp_func_t cmp_func;

    void* alloc_ctx;

    hashmap_entry_t** buckets;
    size_t num_buckets;
    size_t num_entries;

    struct hashmap_chunk* chunks;
    hashmap_entry_t* free_list;
} hashmap_t;

typedef struct {
    hashmap_t* map;
    size_t bucket_index;
    hashmap_entry_t* entry;
} hashmap_iterator_t;

uint32_t hashmap_string_hash(const void* key);
uint32_t hashmap_pointer_hash(const void* key);
int hashmap_string_cmp(const void* key_a, const void* key_b);
int hashmap_pointer_cmp(const void* key_a, const void* key_b);

/**
 * @brief Initializes a hash map for C-string keys.
 * @return 0 on success, -1 on allocation failure.
 */
int hashmap_init(hashmap_t* map, size_t initial_size, hashmap_alloc_func_t alloc_func, hashmap_hash_func_t hash_func, hashmap_cmp_func_t cmp_func, void* alloc_ctx);

/**
 * @brief Destroys a hash map.
 */
void hashmap_destroy(hashmap_t* map);

/**
 * @brief Sets a value for a given string key.
 * @return 0 on success, -1 on allocation failure.
 */
int hashmap_set(hashmap_t* map, const void* key, void* value);

/**
 * @brief Gets the value for a given string key.
 * @return The value, or NULL if the key is not found.
 */
void* hashmap_get(hashmap_t* map, const void* key);

/**
 * @brief Removes a key-value pair from the hash map.
 * @return true if the key was found and removed, false otherwise.
 */
bool hashmap_remove(hashmap_t* map, const void* key);

/**
 * @brief Initializes a hash map iterator.
 */
void hashmap_iterator_init(hashmap_t* map, hashmap_iterator_t* iter);

/**
 * @brief Advances the iterator and returns the next entry.
 * @return A pointer to the next hashmap_entry_t, or NULL if iteration is finished.
 */
hashmap_entry_t* hashmap_iterator_next(hashmap_iterator_t* iter);
