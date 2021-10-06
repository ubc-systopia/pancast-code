#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

struct req_data {
   char *response;
   size_t size;
};

struct risk_data {
    pthread_mutex_t mutex;
    pthread_cond_t uart_ready_cond;
    pthread_cond_t request_ready_cond;
    int uart_ready;
    int request_ready;
    int data_ready;
    struct req_data data;
};

#endif // COMMON_H