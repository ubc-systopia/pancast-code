#ifndef UART_H
#define UART_H

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>
#include <string.h>
#include <termios.h>

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PIN  24 /* P1-18 */
#define POUT 4  /* P1-07 */

#define TERMINAL    "/dev/ttyACM0"

#define CHUNK_SIZE 250

#define TEST_SIZE 1000

extern void* uart_main(void* arg);

#endif // UART_H
