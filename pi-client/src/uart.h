#ifndef UART_H
#define UART_H

#include "common.h"

#include <fcntl.h>  
#include <string.h>
#include <termios.h>
#include <pigpio.h>

#define PIN  24 /* P1-18 */

#define TERMINAL "/dev/ttyACM0"

#define CHUNK_SIZE 250

#define TEST_SIZE 1000

extern void* uart_main(void* arg);

#endif // UART_H
