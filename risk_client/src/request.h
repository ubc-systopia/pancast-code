#ifndef REQUEST_H
#define REQUEST_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>


#define REQUEST_INTERVAL 3000

int handle_request(struct req_data* chunk);

void* request_main(void* arg);

#endif // REQUEST_H