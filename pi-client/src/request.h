#ifndef REQUEST_H
#define REQUEST_H

#include "common.h"

#include <curl/curl.h>

#define REQUEST_INTERVAL 86400 // request to server once per day
#define DOMAIN_LEN 31
#define REQUEST_LEN 7

//const char domain[DOMAIN_LEN];
//const char request[REQUEST_LEN];

int handle_request(struct req_data* data);
int handle_request_chunk(struct req_data* data, int chunk);
int handle_request_count(struct req_data* data);

void* request_main(void* arg);

#endif // REQUEST_H
