#ifndef UART_H
#define UART_H

#include "common.h"
#include "request.h"
#include "../../common/src/riskinfo.h"

#include <fcntl.h> 
#include <time.h>
#include <string.h>
#include <termios.h>
#include <pigpio.h>
#include <stdint.h>

#define PIN  24 /* P1-18 */

#define TERMINAL "/dev/ttyACM0"

#define PER_ADV_SIZE 250
#define CHUNK_REPLICATION 1

/*
 * max number of packets that rpi can hold for risk broadcast
 */
#define MAX_PKTS 10000

/*
 * interval in seconds to request new risk data from backend
 */
#define REQUEST_INTERVAL 86400

extern void *uart_main(void *arg);

#endif // UART_H
