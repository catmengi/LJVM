#pragma once
#include <pthread.h>

#define mutex_t pthread_mutex_t
#define mutex_init(mutex) ({pthread_mutexattr_t attr; pthread_mutexattr_init(&attr); pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(mutex,&attr);})
#define mutex_lock(mutex) pthread_mutex_lock(mutex);
#define mutex_unlock(mutex) pthread_mutex_unlock(mutex);
#define mutex_destroy(mutex) pthread_mutex_destroy(mutex);

#define condvar_t int //TODO: condvars