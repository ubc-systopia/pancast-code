#ifndef COMMON_UTIL__H
#define COMMON_UTIL__H

#include "log.h"

// Some utility macros

#include <stdint.h>

#define log_bytes(logf, data, len, name)                           \
  {                                                                     \
    uint32_t time_ms = (uint32_t) (sl_sleeptimer_get_tick_count64()     \
        * 1000 / sl_sleeptimer_get_timer_frequency());                  \
        \
        logf("%02ld:%02ld:%02ld.%03ld %s: 0x ", (time_ms / 3600000),    \
            ((time_ms / 60000) % 60), ((time_ms / 1000) % 60),          \
            (time_ms % 1000), name);                                    \
        \
    for (unsigned int i = 0; i < len; i++)                              \
    {                                                                   \
        if (i != 0 && !(i % 16))                                        \
        {                                                               \
            logf("%s", "\r\n");                                         \
        }                                                               \
        logf("%.2x ", ((uint8_t *)data)[i]);                            \
    }                                                                   \
    logf("%s", "\r\n");                                                 \
  }

#define log_en_bytes(logf, data, len, name, arg1, arg2, arg3, arg4, arg5, \
               arg6, arg7, arg8, arg9)                                    \
  {                                                                     \
    uint32_t time_ms = (uint32_t) (sl_sleeptimer_get_tick_count64()     \
        * 1000 / sl_sleeptimer_get_timer_frequency());                  \
        \
        logf("%02ld:%02ld:%02ld.%03ld %s: ", (time_ms / 3600000),       \
            ((time_ms / 60000) % 60), ((time_ms / 1000) % 60),          \
            (time_ms % 1000), name);                                    \
        \
    for (unsigned int i = 0; i < len; i++)                              \
    {                                                                   \
        if (i != 0 && !(i % 16))                                        \
        {                                                               \
            logf("%s", "\r\n");                                         \
        }                                                               \
        logf("%.2x", ((uint8_t *)data)[i]);                             \
    }																	\
    logf(" 0x%lx 0x%lx %u %lu %u %lu %u %d [%lu]\r\n", arg1, arg2, arg3, \
        arg4, arg5, arg6, arg7, arg8, arg9);      \
  }

#define print_bytes(data, len, name) \
  log_bytes(log_debugf, data, len, name)

#define hexdumpn(data, len, name)   \
  log_bytes(printf, data, len, name)

#define hexdumpen(data, len, name, arg1, arg2, arg3, arg4, arg5, arg6,  \
                  arg7, arg8, arg9)                                     \
  log_en_bytes(printf, data, len, name, arg1, arg2, arg3, arg4, arg5,   \
		  arg6, arg7, arg8, arg9)


#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

static inline void bitdump(uint8_t *buf, int buflen, char *name)
{
	if (!buf || !buflen)
		return;

  log_infof("%s\r\n", name);

	int i;
	for (i = 0; i < buflen; i++) {
		if (i > 0 && (i % 16) == 0)
			printf("\r\n");

		printf(PRINTF_BINARY_PATTERN_INT8 " ", PRINTF_BYTE_TO_BINARY_INT8(buf[i]));
	}
	printf("\r\n");
}

#endif
