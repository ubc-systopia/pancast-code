#include "client.h"
#include "request.h"
#include "uart.h"
#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>

int main(int argc, char *argv[]) {

    pthread_t id[2];
    struct risk_data r_data;

    int p = pthread_mutex_init(&r_data.mutex, NULL);

    // handle request once at beginning for now, until mutex is implemented
    handle_request(&r_data.data);

    //  pthread_create(&id[REQ_THREAD_ID], NULL, &request_main, (void*)&r_data);
    pthread_create(&id[UART_THREAD_ID], NULL,  &uart_main, (void*)&r_data);

    pthread_join(id[REQ_THREAD_ID],NULL);
    pthread_join(id[UART_THREAD_ID],NULL); 

    return 0;
}

