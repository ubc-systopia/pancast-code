#include "request.h"

#include <curl/curl.h>

const char domain[] = "https://127.0.0.1:8081/"; // TODO: update to server URL when server is online
const char request[] = "update";

/* Write data from stream, from CURLOPT_WRITEFUNCTION example 
    https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    data - pointer to delivered data
    size - size is always 1
    nmemb - size of *data
    userdata - argument set by CURLOPT_WRITEDATA
 */
size_t write_function(char *data, size_t size, size_t nmemb, void *userdata){

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
int handle_request(struct req_data* chunk) {
    unsigned url_len = strlen(domain) + strlen(request);
    char *url;

    if(!(url = malloc(url_len + 1))) return 1;
    strcpy(url,domain);
    strcat(url,request);
    url[url_len] = '\0';

//    printf("\n\rCreating request: \t%s\n\r", url);
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL *curl = curl_easy_init();
    if(curl) {
        
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Disable SSL verification for now
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

        char* response_string;
        FILE* response;
        char* header_string;
 
        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
        
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "error: %s\r\n", curl_easy_strerror(res));
           // return -1;

       // printf("Received data! Size: %d\r\n", (int)chunk->size);
        
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    free(url);
    return 0;
}

// To be started at a thread in client main
void* request_main(void* arg) {

    struct risk_data* r_data = (struct risk_data*)arg;

    while (1) {
        // wait for request timer signal
       //  pthread_mutex_lock(&r_data->mutex);
        int req = handle_request(&r_data->data);

        if (req == 0) {
            //update_risk_data(&response);
            // memcpy(&struct1, &struct2, sizeof(mystruct_t));
        }

      //    pthread_mutex_unlock(&r_data->mutex);

        sleep(REQUEST_INTERVAL);
    }
}

