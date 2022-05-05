#include "request.h"

//const char domain[] = "https://127.0.0.1:8081/";
const char domain[] = "https://pancast.cs.ubc.ca:443/";
const char request[] = "update";
#define DOMAIN_LEN strnlen(domain, 64)
#define REQUEST_LEN strnlen(request, 16)

/*
 * Write data from stream, from CURLOPT_WRITEFUNCTION example
 * https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 * data - pointer to delivered data
 * size - size is always 1
 * nmemb - size of *data
 * userdata - argument set by CURLOPT_WRITEDATA
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
int handle_request_chunk(struct req_data *data, int chunk)
{
#define MAX_URL_LEN 256
  char url[MAX_URL_LEN];
  memset(url, 0, MAX_URL_LEN);
  sprintf(url, "%s%s?chunk=%d", domain, request, chunk);

  dprintf(LVL_DBG, "Making request to server: %s\r\n", url);

  curl_global_init(CURL_GLOBAL_ALL);

  CURL *curl = curl_easy_init();
  if (!curl)
    return -EINVAL;

  CURLcode res;
  curl_easy_setopt(curl, CURLOPT_URL, url);

  // disable SSL verification for now
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

  // send all data to write function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);

  // pass 'data' struct to the callback function
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) data);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "error: %s\r\n", curl_easy_strerror(res));
    return -1;
  }

  dprintf(LVL_EXP, "res: %d, data size: %d buf hdr: %llu\r\n", res,
      (int) data->size, ((uint64_t *) data->response)[0]);

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return 0;
}


/*
 * Handle HTTP request to pancast server adapted from
 * https://github.com/CedricFauth/c-client-flask-server-test
 */
int handle_request_count(struct req_data *data)
{
#define COUNT_LEN 6
#define MAX_URL_LEN 256
  char url[MAX_URL_LEN];
  memset(url, 0, MAX_URL_LEN);
  sprintf(url, "%s%s/count", domain, request);

  dprintf(LVL_DBG, "Making request to server: %s\r\n", url);

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
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) data);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {	
      fprintf(stderr, "error: %s\r\n", curl_easy_strerror(res));
      return -1;
    }

    printf("Request success! Data size: %d\r\n", (int) data->size);
    printf("Response: %d\r\n", *(uint32_t *) data->response);

    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
  return 0;
}
