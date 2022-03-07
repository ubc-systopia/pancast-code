#ifndef UART_H
#define UART_H

#include "common.h"
#include "request.h"

#include <fcntl.h> 
#include <time.h>
#include <string.h>
#include <termios.h>
#include <pigpio.h>
#include <stdint.h>

#define PIN  24 /* P1-18 */

#define TERMINAL "/dev/ttyACM0"

#define PAYLOAD_SIZE 250
#define TEST_SIZE 1000
#define PACKET_HEADER_LEN (2*sizeof(uint32_t) + sizeof(uint64_t))
#define MAX_PACKET_SIZE (PAYLOAD_SIZE - PACKET_HEADER_LEN)  
#define PACKET_REPLICATION 1
#define CHUNK_REPLICATION 1
#define MAX_PAYLOAD_SIZE 100000

#define REQ_HEADER_SIZE 8

#define REQUEST_INTERVAL 86400 // how often in seconds to request data from backend

extern void* uart_main(void* arg);

#endif // UART_H
