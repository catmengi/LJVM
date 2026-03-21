#include "hashmap.h"
#include <string.h>
#include <assert.h>

#define HASHMAP_CHUNK_SIZE 64
#define HASHMAP_LOAD_FACTOR_THRESHOLD 0.75

typedef struct hashmap_chunk {
    struct hashmap_chunk* next;
} hashmap_chunk_t;

// --- MurmurHash3 for C-strings ---
static uint32_t murmur3_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static uint32_t hash_key(const char* key, uint32_t seed){
    size_t len = strlen(key);
    uint32_t h = seed;
    uint32_t k;
    const uint8_t* d = (const uint8_t*)key;
    const uint32_t* chunks = (const uint32_t*)(d);
    const uint8_t* tail = (const uint8_t*)(d + (len & ~3));

    for (size_t i = len >> 2; i > 0; i--) {
        k = *chunks++;
        h ^= murmur3_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    k = 0;
    switch (len & 3) {
        case 3: k ^= tail[2] << 16;
        case 2: k ^= tail[1] << 8;
        case 1: k ^= tail[0];
        h ^= murmur3_32_scramble(k);
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}
// --- End MurmurHash3 ---

// Static helper functions (allocate_new_chunk, get_new_entry) are unchanged
// ...
static int allocate_new_chunk(hashmap_t* map) {
    size_t chunk_mem_size = sizeof(hashmap_chunk_t) + HASHMAP_CHUNK_SIZE * sizeof(hashmap_entry_t);
    hashmap_chunk_t* chunk = map->alloc_func(map->alloc_ctx, chunk_mem_size);
    if (!chunk) return -1;
    chunk->next = map->chunks;
    map->chunks = chunk;
    hashmap_entry_t* entries = (hashmap_entry_t*)(chunk + 1);
    for (size_t i = 0; i < HASHMAP_CHUNK_SIZE - 1; ++i) {
        entries[i].next = &entries[i + 1];
    }
    entries[HASHMAP_CHUNK_SIZE - 1].next = NULL;
    map->free_list = entries;
    return 0;
}

static hashmap_entry_t* get_new_entry(hashmap_t* map) {
    if (!map->free_list) {
        if (allocate_new_chunk(map) != 0) {
            return NULL;
        }
    }
    hashmap_entry_t* entry = map->free_list;
    map->free_list = entry->next;
    return entry;
}


static int rehash(hashmap_t* map) {
    size_t old_num_buckets = map->num_buckets;
    hashmap_entry_t** old_buckets = map->buckets;

    size_t new_num_buckets = old_num_buckets * 2;
    hashmap_entry_t** new_buckets = map->alloc_func(map->alloc_ctx, new_num_buckets * sizeof(hashmap_entry_t*));
    if (!new_buckets) return -1;
    
    memset(new_buckets, 0, new_num_buckets * sizeof(hashmap_entry_t*));

    map->buckets = new_buckets;
    map->num_buckets = new_num_buckets;

    for (size_t i = 0; i < old_num_buckets; ++i) {
        hashmap_entry_t* entry = old_buckets[i];
        while (entry) {
            hashmap_entry_t* next = entry->next;
            uint32_t hash = hash_key(entry->key, 0);
            size_t index = hash & (map->num_buckets - 1);

            entry->next = map->buckets[index];
            map->buckets[index] = entry;
            entry = next;
        }
    }
    
    return 0;
}

uint32_t hashmap_string_hash(const void* key){
    return hash_key(key,0);
}

uint32_t hashmap_pointer_hash(const void* key){
    size_t len = sizeof(void*);
    uint32_t h = 0;
    uint32_t k;
    const uint8_t* d = (const uint8_t*)&key;
    const uint32_t* chunks = (const uint32_t*)(d);
    const uint8_t* tail = (const uint8_t*)(d + (len & ~3));

    for (size_t i = len >> 2; i > 0; i--) {
        k = *chunks++;
        h ^= murmur3_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    k = 0;
    switch (len & 3) {
        case 3: k ^= tail[2] << 16;
        case 2: k ^= tail[1] << 8;
        case 1: k ^= tail[0];
        h ^= murmur3_32_scramble(k);
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

int hashmap_string_cmp(const void* key_a, const void* key_b){
    return strcmp(key_a,key_b);
}

int hashmap_pointer_cmp(const void* key_a, const void* key_b){
    return !(key_a == key_b);
}

int hashmap_init(hashmap_t* map, size_t initial_size, hashmap_alloc_func_t alloc_func, hashmap_hash_func_t hash_func, hashmap_cmp_func_t cmp_func, void* alloc_ctx){
    assert(map && alloc_func && hash_func && cmp_func);
    assert((initial_size > 0) && ((initial_size & (initial_size - 1)) == 0)); 

    map->alloc_func = alloc_func;
    map->hash_func = hash_func;
    map->cmp_func = cmp_func;
    map->alloc_ctx = alloc_ctx;
    map->num_buckets = initial_size;
    map->num_entries = 0;
    map->chunks = NULL;
    map->free_list = NULL;
    
    map->buckets = map->alloc_func(map->alloc_ctx, map->num_buckets * sizeof(hashmap_entry_t*));
    if (!map->buckets) return -1;
    
    memset(map->buckets, 0, map->num_buckets * sizeof(hashmap_entry_t*));
    return 0;
}

void hashmap_destroy(hashmap_t* map) {
    (void)map;
}

int hashmap_set(hashmap_t* map, const void* key, void* value) {
    if ((float)map->num_entries / map->num_buckets > HASHMAP_LOAD_FACTOR_THRESHOLD) {
        if (rehash(map) != 0) return -1;
    }

    uint32_t hash = map->hash_func(key);
    size_t index = hash & (map->num_buckets - 1);

    hashmap_entry_t* entry = map->buckets[index];
    while (entry) {
        if (map->cmp_func(key, entry->key) == 0) {
            entry->value = value;
            return 0;
        }
        entry = entry->next;
    }

    hashmap_entry_t* new_entry = get_new_entry(map);
    if (!new_entry) return -1;

    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    
    map->num_entries++;
    return 0;
}

void* hashmap_get(hashmap_t* map, const void* key) {
    uint32_t hash = map->hash_func(key);
    size_t index = hash & (map->num_buckets - 1);

    hashmap_entry_t* entry = map->buckets[index];
    while (entry) {
        if (map->cmp_func(key, entry->key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

bool hashmap_remove(hashmap_t* map, const void* key) {
    uint32_t hash = map->hash_func(key);
    size_t index = hash & (map->num_buckets - 1);

    hashmap_entry_t** p_entry = &map->buckets[index];
    while (*p_entry) {
        if (map->cmp_func(key, (*p_entry)->key) == 0) {
            hashmap_entry_t* entry_to_remove = *p_entry;
            *p_entry = entry_to_remove->next;

            entry_to_remove->next = map->free_list;
            map->free_list = entry_to_remove;

            map->num_entries--;
            return true;
        }
        p_entry = &(*p_entry)->next;
    }
    return false;
}

void hashmap_iterator_init(hashmap_t* map, hashmap_iterator_t* iter) {
    assert(map && iter);
    iter->map = map;
    iter->bucket_index = 0;
    iter->entry = NULL;
}

hashmap_entry_t* hashmap_iterator_next(hashmap_iterator_t* iter) {
    assert(iter && iter->map);

    if (iter->entry) {
        iter->entry = iter->entry->next;
    }

    while (!iter->entry) {
        if (iter->bucket_index >= iter->map->num_buckets) {
            return NULL;
        }
        
        iter->entry = iter->map->buckets[iter->bucket_index];
        iter->bucket_index++;
    }

    return iter->entry;
}
