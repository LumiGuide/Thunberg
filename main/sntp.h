#pragma once

#include <pthread.h>
#include <time.h>

struct sync_t {
    struct tm *time;
    pthread_mutex_t lock;
};

extern struct sync_t syncdata;

void realtime_checker(void*);