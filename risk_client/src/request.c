#include "request.h"

const char domain[] = "https://10.0.0.117:8081/";
const char request[] = "update";

/* Write data from stream, from CURLOPT_WRITEFUNCTION example 
    https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    data - pointer to delivered data
    size - size is always 1
    nmemb - size of *data
    userdata - argument set by CURLOPT_WRITEDATA
 */
size_t write_function(char *data, size_t size, size_t nmemb, void *userdata) {

  size_t realsize = size * nmemb;
  struct req_data *mem = (struct req_data*)userdata;
 
  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if(ptr == NULL) {
    printf("out of memory!");
    return 0;  /* out of memory! */
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
 
  return realsize;
}

/* Handle HTTP request to pancast server
    adapted from https://github.com/CedricFauth/c-client-flask-server-test
 */
int handle_request(struct req_data *data) {
  unsigned url_len = strlen(domain) + strlen(request);
  char *url;

  if(!(url = malloc(url_len + 1))) {
    fprintf(stderr, "error in request malloc\r\n");	  
    return -1;
  }
 
  strncpy(url, domain, DOMAIN_LEN);
  strncat(url, request, REQUEST_LEN);
  url[url_len] = '\0';

  printf("Making request to server: %s\r\n", url);
    
  curl_global_init(CURL_GLOBAL_ALL);
    
  CURL *curl = curl_easy_init();
  if (curl) {
        
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Disable SSL verification for now
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
 
    // send all data to write function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
        
    // pass 'data' struct to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)data);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {	
      fprintf(stderr, "error: %s\r\n", curl_easy_strerror(res));
      return -1;
    }

    printf("Request success! Data size: %d\r\n", (int)data->size);
        
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
  free(url);
  return 0;
}

// To be started at a thread in client main
void *request_main(void *arg) { 
  struct risk_data *r_data = (struct risk_data *) arg;
  int err = 0;

  printf("Starting request main\r\n");

  while (1) {
    struct req_data request = {0};

    // TODO check for errors and retry if necessary
    int req = handle_request(&request); 
    if (req == 0) {
      r_data->data_ready = 1;
      err = pthread_mutex_lock(&r_data->mutex);
      if (err != 0) {
        fprintf(stderr, "pthread_mutex_lock error\r\n");
      }
      while (!r_data->uart_ready) {
        err = pthread_cond_wait(&r_data->uart_ready_cond, &r_data->mutex);
        if (err != 0) {
          fprintf(stderr, "pthread_cond_wait error\r\n");
	}
      }
      r_data->uart_ready = 0; 
      printf("Request copying data...\r\n");

      memcpy(r_data->data.response, &request.response, request.size);
	    
      r_data->request_ready = 1;
      r_data->data_ready = 0;

      printf("Copy complete!\r\n");
	    
      err = pthread_cond_signal(&r_data->request_ready_cond);
      if (err != 0) {
        fprintf(stderr, "pthread_cond_signal error\r\n");
      }

      err = pthread_mutex_unlock(&r_data->mutex);
      if (err != 0) {
        fprintf(stderr, "pthread_mutex_unlock error\r\n");
      }
    }
  sleep(REQUEST_INTERVAL);
  }
}
