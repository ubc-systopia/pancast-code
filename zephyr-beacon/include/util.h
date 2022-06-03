#ifndef COMMON_UTIL__H
#define COMMON_UTIL__H

#include "log.h"

/*
 * ==============
 * utility macros
 * ==============
 */

#include <stdint.h>

#define log_bytes(logf, data, len, name)                            \
  {                                                                 \
    uint32_t time_ms = k_uptime_get();                              \
    logf("%02d:%02d:%02d.%03d %s: 0x ", (time_ms / 3600000),        \
        ((time_ms / 60000) % 60), ((time_ms / 1000) % 60),          \
        (time_ms % 1000), name);                                    \
                                                                    \
    for (unsigned int i = 0; i < len; i++)                          \
    {                                                               \
        if (i != 0 && !(i % len))                                   \
        {                                                           \
            logf("%s", "\r\n");                                     \
        }                                                           \
        logf("%.2x ", ((uint8_t *)data)[i]);                        \
    }                                                               \
    logf("\r\n");                                                   \
  }

#define log_en_bytes(logf, data, len, name, arg1, arg2, arg3)       \
  {                                                                 \
    uint32_t time_ms = k_uptime_get();                              \
    logf("%02d:%02d:%02d.%03d %s: ", (time_ms / 3600000),           \
        ((time_ms / 60000) % 60), ((time_ms / 1000) % 60),          \
        (time_ms % 1000), name);                                    \
                                                                    \
    for (unsigned int i = 0; i < len; i++)                          \
    {                                                               \
        if (i != 0 && !(i % 16))                                    \
        {                                                           \
            logf("%s", "\r\n");                                     \
        }                                                           \
        logf("%.2x", ((uint8_t *)data)[i]);                         \
    }																	                              \
    logf(" 0x%x 0x%x %u\r\n", arg1, arg2, arg3);                    \
  }

#define print_bytes(data, len, name) \
  log_bytes(log_debugf, data, len, name, 0, 0, 0)

#define hexdumpn(data, len, name)  \
  log_bytes(printf, data, len, name)

#define hexdumpen(data, len, name, arg1, arg2, arg3)  \
  log_en_bytes(printf, data, len, name, arg1, arg2, arg3)

#endif /* COMMON_UTIL__H */
