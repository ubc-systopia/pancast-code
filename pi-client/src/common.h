#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

struct req_data {
   char *response;
   size_t size;
};

struct risk_data {
    pthread_mutex_t mutex;
    pthread_cond_t uart_ready_cond;
    pthread_cond_t request_ready_cond;
    int uart_ready;
    int request_ready;
    int data_ready;
    struct req_data data;
};

#define LVL_EXP 0
#define LVL_DBG 1

#define LOG_LVL LVL_EXP

#define dprintf(DBG_LVL, F, A...) \
  do {  \
    if (LOG_LVL >= DBG_LVL) { \
      printf("%s:%d " F, __func__, __LINE__, A); \
    } \
  } while (0)

static inline void hexdump(char *buf, int buflen)
{
  if (!buf || !buflen)
    return;

//  char *prtbuf = malloc(buflen*2+1);
//  if (!prtbuf)
//    return;
//
//  memset(prtbuf, 0, buflen*2+1);
  int i;
  for (i = 0; i < buflen; i++) {
    if (i > 0 && (i % 16) == 0)
      printf("\n");
    printf("%02x ", buf[i]);
  }
  printf("\n");
}

/*
 * https://stackoverflow.com/a/3208376
 */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

static inline void bitdump(char *buf, int buflen)
{
  if (!buf || !buflen)
    return;

  int i;
  for (i = 0; i < buflen; i++) {
    if (i > 0 && (i % 16) == 0)
      printf("\n");

    printf(PRINTF_BINARY_PATTERN_INT8 " ", PRINTF_BYTE_TO_BINARY_INT8(buf[i]));
  }
	printf("\n");
}

#endif // COMMON_H
