#pragma once

#include <pthread.h>

typedef struct{
    pthread_mutex_t mutex;
}recursive_mutex_t;