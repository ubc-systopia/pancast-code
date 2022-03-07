#include "request.h"

const char domain[] = "https://pancast.cs.ubc.ca:443/";
const char request[] = "update";

/* Write data from stream, from CURLOPT_WRITEFUNCTION example 
    https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    data - pointer to delivered data
    size - size is always 1
    nmemb - size of *data
    userdata - argument set by CURLOPT_WRITEDATA
 */
size_t write_function(char *data, size_t size, size_t nmemb, void *userdata) 
{
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
int handle_request(struct req_data *data) 
{
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

    printf("about to perform curl\r\n");
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

/* Handle HTTP request to pancast server
    adapted from https://github.com/CedricFauth/c-client-flask-server-test
 */
int handle_request_chunk(struct req_data *data, int chunk) 
{
#define REQ_ARGS_LEN 9
  unsigned url_len = strlen(domain) + strlen(request);
  char *url;

  if(!(url = malloc(url_len + 1 + REQ_ARGS_LEN))) {
    fprintf(stderr, "error in request malloc\r\n");	  
    return -1;
  }

  char req_args[REQ_ARGS_LEN];
  url_len+= REQ_ARGS_LEN;

  sprintf(req_args, "?chunk=%d", chunk);
 
  strncpy(url, domain, DOMAIN_LEN);
  strncat(url, request, REQUEST_LEN);
  strncat(url, req_args, REQ_ARGS_LEN);
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


/* Handle HTTP request to pancast server
    adapted from https://github.com/CedricFauth/c-client-flask-server-test
 */
int handle_request_count(struct req_data *data) 
{
#define COUNT_LEN 6
  unsigned url_len = strlen(domain) + strlen(request);
  char *url;

  if(!(url = malloc(url_len + 1 + COUNT_LEN))) {
    fprintf(stderr, "error in request malloc\r\n");	  
    return -1;
  }

  char* count = "/count";
  url_len+=COUNT_LEN;
 
  strncpy(url, domain, DOMAIN_LEN);
  strncat(url, request, REQUEST_LEN);
  strncat(url, count, COUNT_LEN);
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
