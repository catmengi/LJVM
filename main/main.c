#include "config.h"
#include "stringpool.h"
#include "class.h"
#include "thread.h"

#include <assert.h>
#include "loader.h"


Thread_t* thread_alloc();
void thread_free(Thread_t* thread);

int app_main(){
    JEspresso_init();

    loader_set_loadpath("java_src");

    Class_t* out = NULL;
    assert(class_load_bynameid(stringpool_add("dummy"), &out) == JERR_OK);

    Method_t* method = class_find_method(out, stringpool_add("main@([Ljava/lang/String;)V"));
    assert(method);

    java_method_invoke(method,  (int32_t[1]){0}, NULL);

    thread_start(thread_alloc(),method, (int32_t[1]){0});
    thread_schedule();

    return 0;
}

int main(){
    return app_main();
}