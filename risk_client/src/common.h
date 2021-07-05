#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <stddef.h>

struct req_data {
   char *response;
   size_t size;
};

struct risk_data {
    pthread_mutex_t mutex;
    pthread_cond_t can_update;
    pthread_cond_t update_ready;
    struct req_data data;
};

#endif // COMMON_H