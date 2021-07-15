#ifndef REQUEST_H
#define REQUEST_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>
#include <curl/curl.h>

#define REQUEST_INTERVAL 5
#define DOMAIN_LEN 31
#define REQUEST_LEN 7

const char domain[DOMAIN_LEN];
const char request[REQUEST_LEN];

int handle_request(struct req_data *data);

void *request_main(void *arg);

#endif // REQUEST_H
