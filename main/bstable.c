#include "bstable.h"
#include "bumper.h"

int bstable_init(bstable_t* bstable, bump_allocator_t* arena, size_t size, int (*cmp)(const void* a, const void* b),
                void* (*find)(bstable_t* bstable, const void* key)){
    bstable->arena = arena;
    bstable->size = size;
    bstable->last_sorted = 0;
    bstable->cmp = cmp;
    bstable->find = find;
    bstable->elements = bumper_calloc(bstable->arena,bstable->size, sizeof(*bstable->elements));

    return bstable->elements ? 0 : 1;
}

int bstable_insert(bstable_t* bstable, const void* element){
    if(bstable->count >= bstable->size)
        return 1;

    bstable->elements[bstable->count++] = element;
    return 0;
}

void* bstable_find(bstable_t* bstable, const void* key){
    if(bstable->last_sorted != bstable->count){
        qsort(bstable->elements, bstable->count, sizeof(*bstable->elements), bstable->cmp);
        bstable->last_sorted = bstable->count;
    }

    return bstable->find(bstable,key);
}

void* bstable_find_raw(bstable_t* bstable, const void* find_template){
    return bsearch(find_template, bstable->elements,bstable->count,sizeof(*bstable->elements), bstable->cmp);
}

/*
struct example_search{
    char* name;
    void* abcd;
    int def;
};

#include <string.h>
#include <stdio.h>
int test_cmp(const void* a, const void* b){
    struct example_search* EA = *(struct example_search**)a;
    struct example_search* EB = *(struct example_search**)b;

    return strcmp(EA->name, EB->name);
}

void* test_find(bstable_t* bstable, const void* key){
    struct example_search tmp = {
        .name = (char*)key,
    };

    void* p = &tmp;
    void** output = bstable_find_raw(bstable, &p);
    return output ? *output : NULL;
}

int main(){
    bump_allocator_t arena = {0};
    bumper_create(&arena, 1024 * 1024);

    struct example_search a = {.name = "abcd"};
    struct example_search b = {.name = "efgh"};
    struct example_search c = {.name = "йошкин кот!"};

    bstable_t bst = {0};
    bstable_init(&bst, &arena, 3, test_cmp, test_find);

    bstable_insert(&bst, &a);
    bstable_insert(&bst, &b);
    bstable_insert(&bst, &c);

    struct example_search* find = bstable_find(&bst, "йошкин кот!");
    printf("find: %s\n", find ? find->name : "(nil)");
}
*/