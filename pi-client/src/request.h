#ifndef REQUEST_H
#define REQUEST_H

#include "common.h"

#include <curl/curl.h>

#define DOMAIN_LEN 31
#define REQUEST_LEN 7

int handle_request(struct req_data* data);
int handle_request_chunk(struct req_data* data, int chunk);
int handle_request_count(struct req_data* data);

#endif // REQUEST_H
