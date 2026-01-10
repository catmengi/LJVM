#include "../jvm.h"
#include "../jvm_internal.h"
#include "../object.h"

#include <pthread.h>
#include <semaphore.h>


extern classlinker_class_t java_lang_Object;

static jvm_error_t clinit(jvm_frame_t* frame){ //For freeRTOS
    C_TO_JVM_VALUE(classlinker_find_staticfield(frame, frame->method->class, "MAX_PRIORITY")->value,10);
    C_TO_JVM_VALUE(classlinker_find_staticfield(frame, frame->method->class, "MIN_PRIORITY")->value,1);
    C_TO_JVM_VALUE(classlinker_find_staticfield(frame, frame->method->class, "NORM_PRIORITY")->value,5);

    return JVM_OK;
}

static jvm_error_t thread_init_target(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    objectmanager_object_t* target = JVM_TO_C_VALUE(frame->locals[1],typeof(target));

    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);
    if(target && objectmanager_class_object_is_compatible_to(objectmanager_get_class_object_info(target),classlinker_find_class(frame->jvm->linker,"java/lang/Runnable"))){
        C_TO_JVM_VALUE(objectmanager_class_object_get_field(frame, cself, "target")->value,target);
    }

    return JVM_OK;
}

static jvm_error_t thread_init(jvm_frame_t* frame){
    return JVM_OK;
}

static void posix_thread_cleanup(void* params){
    jvm_instance_t* jvm = jvm_current_thread->jvm;
    jvm_lock(jvm);

    C_TO_JVM_VALUE(objectmanager_class_object_get_field(NULL, objectmanager_get_class_object_info(jvm_current_thread->JThread), "thread")->value,(pthread_t)NULL);

    if(jvm_current_thread->bytecode_executor_arguments)
        arena_free_block(jvm_current_thread->bytecode_executor_arguments);

    list_del(&jvm_current_thread->list);
    arena_free_block(jvm_current_thread);
    jvm_current_thread = NULL;
    jvm->thread_count--;

    if(jvm->thread_count == 0){
        objectmanager_gc(jvm,0); //Finalize stuff like files and etc
        pthread_cond_broadcast(&jvm->jvm_exit_wait);
    }

    pthread_detach(pthread_self());
    jvm_unlock(jvm);
}

static void* native_thread(void* params_p){
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); 

    void** params = params_p;
    objectmanager_object_t* self = params[0];
    objectmanager_object_t* runnable = params[1];
    jvm_instance_t* jvm = params[2];
    sem_t* sem = params[3];
    sem_post(sem);

    jvm_lock(jvm);
    jvm_thread_t* new_thread = arena_calloc(jvm->arena,1,sizeof(*new_thread));
    assert(new_thread);

    INIT_LIST_HEAD(&new_thread->list);
    new_thread->JThread = self;
    list_add(&new_thread->list,&jvm->threads);

    new_thread->jvm = jvm;

    jvm_current_thread = new_thread;
    jvm->thread_count++;
    pthread_cleanup_push(posix_thread_cleanup,NULL);
    jvm_unlock(jvm);

    objectmanager_class_object_t* crunnable = objectmanager_get_class_object_info(runnable);
    classlinker_method_t* run = objectmanager_class_object_get_method(&(jvm_frame_t){jvm}, crunnable, "run", "()V");

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //Can cancel it. If it mess something up - whatever, because thread cancelation is triggered
                                                                            //only when JVM exits, so whatever

    jvm_value_t args[1] = {C_TO_NEW_JVM_VALUE(runnable)};

    jvm_error_t err = jvm_invoke(jvm,NULL,run,1,args);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); //Disable cancelation (deadlocks without this)

    jvm_lock(jvm);
    if(err != JVM_OK && err != JVM_METHOD_RETURN){ //Jvm critical error or System.exit, stop all other threads!
        jvm_thread_t* current_thread = NULL, *tmp = NULL;
        list_for_each_entry_safe(current_thread,tmp,&jvm->threads,list){
            if(current_thread != jvm_current_thread){
                objectmanager_object_t* jthread = current_thread->JThread;
                objectmanager_class_object_t* cthread = objectmanager_get_class_object_info(jthread);

                classlinker_method_t* stop = objectmanager_class_object_get_method(&(jvm_frame_t){jvm}, cthread,"os_stop", "()V");
                jvm_value_t args[1] = {C_TO_NEW_JVM_VALUE(jthread)};

                jvm_invoke(jvm,NULL,stop,1,args);
            }
        }
    }
    pthread_cleanup_pop(1);
    jvm_unlock(jvm);

    pthread_exit(NULL);
}

static jvm_error_t os_stop(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);
    
    pthread_t thread = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself, "thread")->value,pthread_t);
    if(thread){
        jvm_lock(frame->jvm);

        pthread_cancel(thread);

        jvm_unlock(frame->jvm);
    }
    return JVM_OK;
}

static jvm_error_t thread_start(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);

    objectmanager_object_t* runnable = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself,"target")->value,typeof(runnable));
    if(runnable == NULL) runnable = self; //No runnable, guess that our run is overwritten then

    jvm_lock(frame->jvm);
    pthread_t* thread = (pthread_t*)objectmanager_class_object_get_field(frame, cself, "thread")->value.value;
        
    sem_t semaphore;
    sem_init(&semaphore,0,0);

    void* params[] = {self,runnable,frame->jvm,&semaphore};
    assert(pthread_create(thread,NULL,native_thread,params) == 0);
    sem_wait(&semaphore);

    jvm_unlock(frame->jvm);
    return JVM_OK;
}

static jvm_error_t thread_run(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    objectmanager_object_t* runnable = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self),"target")->value,typeof(runnable));
    if(runnable){
        classlinker_method_t* run = objectmanager_class_object_get_method(frame, objectmanager_get_class_object_info(runnable), "run", "()V");
        FAIL_SET_JUMP(run,err,JVM_NOTFOUND,exit);

        jvm_value_t args[] = {C_TO_NEW_JVM_VALUE(runnable)};
        err = jvm_invoke(frame->jvm,frame,run,1,args);
    }
    
exit:
    return err;
}

static jvm_error_t thread_join(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    pthread_t thread = JVM_TO_C_VALUE(objectmanager_class_object_get_field(NULL, objectmanager_get_class_object_info(self), "thread")->value,pthread_t);
    if(thread){
        pthread_join(thread,NULL);
    }
    return JVM_OK;
}

static jvm_error_t thread_activeCount(jvm_frame_t* frame){
    jvm_native_return(frame,C_TO_NEW_JVM_VALUE((uint32_t)frame->jvm->thread_count));
    return JVM_OK;
}

static jvm_error_t thread_currentThread(jvm_frame_t* frame){
    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(jvm_current_thread->JThread));
    return JVM_OK;    
}

static jvm_error_t thread_isAlive(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],typeof(self));
    pthread_t thread = JVM_TO_C_VALUE(objectmanager_class_object_get_field(NULL, objectmanager_get_class_object_info(self), "thread")->value,pthread_t);
    bool is_alive = thread ? true : false;

    jvm_native_return(frame,C_TO_NEW_JVM_VALUE(is_alive));
    
    return JVM_OK;
}

#include <unistd.h>
static jvm_error_t thread_sleep(jvm_frame_t* frame){
    uint64_t millis = JVM_TO_C_VALUE(frame->locals[0],typeof(millis));
    usleep(millis * 1000);

    return JVM_OK;
}

static jvm_error_t thread_yield(jvm_frame_t* frame){
    sched_yield();
    return JVM_OK;
}

static classlinker_normalclass_t Thread_info = {
    .fields_count = 2,
    .fields = (classlinker_field_t[]){
        {
            .name = "thread",
            .flags = ACC_PRIVATE,
            .value.type = EJVT_NATIVEPTR, //Dissallow GC to touch it! 
        },
        {
            .name = "target",
            .value.type = EJVT_REFERENCE,
            .flags = ACC_PRIVATE,
        }
    },
    .static_fields_count = 3,
    .static_fields = (classlinker_field_t[]){
        {
            .name = "MAX_PRIORITY",
            .value.type = EJVT_INT,
        },
        {
            .name = "MIN_PRIORITY",
            .value.type = EJVT_INT,
        },
        {
            .name = "NORM_PRIORITY",
            .value.type = EJVT_INT,
        },
    },

    .methods_count = 12,
    .methods = (classlinker_method_t[]){
        {
            .name = "<clinit>",
            .raw_description = "()V",
            .flags = ACC_STATIC | ACC_NATIVE,
            .fn = clinit,
        },
        {
            .name = "<init>",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = thread_init,
        },
        {
            .name = "<init>",
            .raw_description = "(Ljava/lang/Runnable;)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = thread_init_target,
        },
        {
            .name = "os_stop",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = os_stop,
        },
        {
            .name = "start",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = thread_start,
        },
        {
            .name = "join",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = thread_join,
        },
        {
            .name = "isAlive",
            .raw_description = "()Z",
            .flags = ACC_NATIVE,
            .fn = thread_isAlive,
        },
        {
            .name = "activeCount",
            .raw_description = "()I",
            .flags = ACC_NATIVE | ACC_STATIC,
            .fn = thread_activeCount,
        },
        {
            .name = "yield",
            .raw_description = "()V",
            .flags = ACC_NATIVE | ACC_STATIC,
            .fn = thread_yield,
        },
        {
            .name = "sleep",
            .raw_description = "(J)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE | ACC_STATIC,
            .fn = thread_sleep,
        },
        {
            .name = "currentThread",
            .raw_description = "()Ljava/lang/Thread;",
            .flags = ACC_NATIVE | ACC_STATIC,
            .fn = thread_currentThread,
        },
        {
            .name = "run",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = thread_run,
        },
    }
};

classlinker_class_t java_lang_Thread = {
    .this_name = "java/lang/Thread",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &Thread_info,
};

static jvm_error_t class_launcher_init(jvm_frame_t* frame){
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*));
    objectmanager_class_object_get_field(frame, cself,"class_name")->value = frame->locals[1];
    objectmanager_class_object_get_field(frame, cself,"method_name")->value = frame->locals[2];

    return JVM_OK;
}
static jvm_error_t class_launcher(jvm_frame_t* frame){
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*));
    char* class_name = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself,"class_name")->value,char*);
    char* method_name = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself,"method_name")->value,char*);

    classlinker_class_t* class = classlinker_find_class(frame->jvm->linker, class_name);
    if(class){
        classlinker_method_t* main = classlinker_find_method(frame, class, "main", "([Ljava/lang/String;)V");
        if(main){
            C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_array_object(frame,EJVT_REFERENCE, 0));
            objectmanager_object_t* string_args = JVM_TO_C_VALUE(frame->locals[1],objectmanager_object_t*);
            assert(string_args);

            TODO("support of arguments to main() method!");

            jvm_value_t args[1] = {frame->locals[1]};
            jvm_invoke(frame->jvm,frame,main,1,args);
        } else printf("%s: no such method as '%s' in '%s'\n",__PRETTY_FUNCTION__,method_name,class_name);
    } else printf("%s: no such class as '%s'\n",__PRETTY_FUNCTION__,class_name);
    return JVM_OK;
}

static classlinker_normalclass_t ljvm_class_launcher_info = {
    .methods_count = 2,
    .methods = (classlinker_method_t[]){
        {
            .name = "<init>",
            .raw_description = "(***I*)V",
            .frame_descriptor.arguments_count = 5,
            .flags = ACC_NATIVE,
            .fn = class_launcher_init,
        },
        {
            .name = "run",
            .raw_description = "()V",
            .frame_descriptor.locals_count = 1,
            .flags = ACC_NATIVE,
            .fn = class_launcher
        }
    },
    .fields_count = 2,
    .fields = (classlinker_field_t[]){
        {
            .name = "class_name",
            .value.type = EJVT_NATIVEPTR,
            .flags = ACC_PRIVATE,
        },
        {
            .name = "method_name",
            .value.type = EJVT_NATIVEPTR,
            .flags = ACC_PRIVATE,
        },
    }
};

classlinker_class_t ljvm_class_launcher = {
    .this_name = "ljvm/class_launcher",
    .parent = &java_lang_Thread,
    .generation = 2,
    .info = &ljvm_class_launcher_info,
};