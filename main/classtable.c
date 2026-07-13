#include "classtable.h"
#include "bumper.h"
#include "class.h"
#include "config.h"
#include "memman.h"

#include <stdatomic.h>
#include <assert.h>
#include <stdint.h>

static bump_allocator_t* s_arena = NULL;
static struct list_head s_entry_list = {0};
static atomic_flag s_spinlock = ATOMIC_FLAG_INIT;

#define CLASSTABLE_CRITICAL_ENTER() ({while(atomic_flag_test_and_set(&s_spinlock)){}})
#define CLASSTABLE_CRITICAL_EXIT() atomic_flag_clear(&s_spinlock)

/* MurmurHash2, by Austin Appleby
// Note - This code makes a few assumptions about how your machine behaves -
// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4
//
// And it has a few limitations -
//
// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.    */

uint32_t MurmurHash2 ( const void * key, int len)
{
  /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */

  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  /* Initialize the hash to a 'random' value */

  uint32_t h = 0x5bd1e995 ^ len;

  /* Mix 4 bytes at a time into the hash */

  const unsigned char * data = (const unsigned char *)key;

  while(len >= 4)
  {
    uint32_t k = *(uint32_t*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  /* Handle the last few bytes of the input array  */

  switch(len)
  {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  /* Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.  */

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
} 

static ClasstableEntry_t* insert_entry();
void classtable_init(){
    assert((s_arena = memman_get(VM_PERMA_ARENA_ID)));
    INIT_LIST_HEAD(&s_entry_list);

    s_spinlock = (atomic_flag)ATOMIC_FLAG_INIT;

    assert(insert_entry()); //Add initial entry (other wise it wouldnt work)
}


static ClasstableEntry_t* insert_entry(){
    ClasstableEntry_t* entry = bumper_calloc(s_arena, 1, sizeof(*entry));
    if(!entry){
        return NULL;
    } 

    INIT_LIST_HEAD(&entry->list);
    list_add_tail(&entry->list, &s_entry_list);

    return entry;
}

static Class_t* find_class(ClasstableEntry_t* entry, int32_t name_id){
    uint32_t start_pos = name_id % CLASSTABLE_ENTRY_ITEMS_COUNT;
    
    for(unsigned i = 0; i < CLASSTABLE_ENTRY_ITEMS_COUNT; i++){
        uint32_t index = (start_pos + i) % CLASSTABLE_ENTRY_ITEMS_COUNT;
        Class_t* class = entry->items[index];

        if(class && class->name_id == name_id) return class;
    }
    
    return NULL;
}

static Class_t** find_slot(ClasstableEntry_t* entry, int32_t name_id){
    uint32_t start_pos = name_id % CLASSTABLE_ENTRY_ITEMS_COUNT;
    
    for(unsigned i = 0; i < CLASSTABLE_ENTRY_ITEMS_COUNT; i++){
        uint32_t index = (start_pos + i) % CLASSTABLE_ENTRY_ITEMS_COUNT;
        Class_t* class = entry->items[index];

        if(!class || class->name_id == name_id) return &entry->items[index];
    }
    
    return NULL;    
}

Error_t classtable_put(Class_t* class){
    CLASSTABLE_CRITICAL_ENTER();
    Error_t err = JERR_OK;

    ClasstableEntry_t *entry = NULL;
    list_for_each_entry(entry, &s_entry_list, list){
        Class_t** slot = find_slot(entry, class->name_id);
        if(slot){
            assert(!*slot);
            
            *slot = class;
            goto exit;
        }
    }

    ClasstableEntry_t* new_entry = insert_entry();
    FAIL_SET_JUMP(new_entry, err, JERR_OOM, exit);

    Class_t** slot = find_slot(new_entry, class->name_id);
    assert(slot);

    *slot = class;

exit:
    CLASSTABLE_CRITICAL_EXIT();
    return err;
}

Class_t* classtable_get(int32_t name_id){
    CLASSTABLE_CRITICAL_ENTER();
    Class_t* found = NULL;

    ClasstableEntry_t* entry = NULL;
    list_for_each_entry(entry, &s_entry_list, list){
        found = find_class(entry, name_id);
        if(found) goto exit;
    }

exit:
    CLASSTABLE_CRITICAL_EXIT();
    return found;
}

struct list_head* classtable_entries_get(){
    return &s_entry_list;
}