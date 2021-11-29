#include "client.h"

int main(int argc, char *argv[]) {

  pthread_t id[2];
  struct risk_data r_data;
  int err = 0;

  err = pthread_mutex_init(&r_data.mutex, NULL);
  if (err != 0) {
    fprintf(stderr, "pthread_mutex_init error\r\n");
  }
  err = pthread_cond_init(&r_data.uart_ready_cond, 0);
  if (err != 0) { 
    fprintf(stderr, "pthread_cond_init error\r\n");
  }
  err = pthread_cond_init(&r_data.request_ready_cond, 0);
  if (err != 0) {
    fprintf(stderr, "pthread_cond_init error\r\n");
  }

  // handle request once at beginning for now, until mutex is implemented
  //struct req_data request = {0};
  //int req_err = handle_request(&request); 
  //if (req_err != 0) {
  //  printf("error in handle_request\r\n");
  //  return -1;
  //}

  //r_data.data = request;

  //err = pthread_create(&id[REQ_THREAD_ID], NULL, &request_main, (void*)&r_data);
  //if (err != 0) {
  //  fprintf(stderr, "pthread_create error\r\n");
 // }

  err = pthread_create(&id[UART_THREAD_ID], NULL,  &uart_main, (void*)&r_data);
  if (err != 0) {
    fprintf(stderr, "pthread_create error\r\n");
  }

  //err = pthread_join(id[REQ_THREAD_ID], NULL);
  //if (err != 0) {
  //  fprintf(stderr, "pthread_join error\r\n");
 // }

  err = pthread_join(id[UART_THREAD_ID], NULL); 
  if (err != 0) {
    fprintf(stderr, "pthread_join error\r\n");
  }

  return 0;
}
