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

  pthread_mutex_init(&r_data.mutex, NULL);
  pthread_cond_init(&r_data.uart_ready_cond, 0);
  pthread_cond_init(&r_data.request_ready_cond, 0);

  // handle request once at beginning for now, until mutex is implemented
  struct req_data request = {0};
  int req_err = handle_request(&request);
  r_data.data = request;

  // printf("size: %d\r\n", r_data.data.size);

  pthread_create(&id[REQ_THREAD_ID], NULL, &request_main, (void*)&r_data);
  pthread_create(&id[UART_THREAD_ID], NULL,  &uart_main, (void*)&r_data);

  pthread_join(id[REQ_THREAD_ID],NULL);
  pthread_join(id[UART_THREAD_ID],NULL); 

  return 0;
}

