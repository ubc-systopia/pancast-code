#ifndef COMMON_UTIL__H
#define COMMON_UTIL__H

#include "log.h"

// Some utility macros

#include <stdint.h>

#define log_bytes(log, logf, data, len, name, arg1, arg2, arg3, arg4, arg5)   \
  {                                                                  \
    uint32_t time_ms = (uint32_t) (sl_sleeptimer_get_tick_count64()     \
        * 1000 / sl_sleeptimer_get_timer_frequency());                  \
        \
        logf(APP_LOG_TIME_FORMAT APP_LOG_SEPARATOR                      \
            "%s: 0x ", (time_ms / 3600000),                             \
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
    logf("0x%x %lu %u %u %d\r\n", arg1, arg2, arg3, arg4, arg5);                   \
  }

#define log_en_bytes(log, logf, data, len, name, arg1, arg2, arg3, arg4, arg5, \
                         arg6, arg7, arg8)                                    \
  {                                                                     \
    uint32_t time_ms = (uint32_t) (sl_sleeptimer_get_tick_count64()     \
        * 1000 / sl_sleeptimer_get_timer_frequency());                  \
        \
        logf(APP_LOG_TIME_FORMAT APP_LOG_SEPARATOR                      \
            "%s: 0x ", (time_ms / 3600000),                             \
            ((time_ms / 60000) % 60), ((time_ms / 1000) % 60),          \
            (time_ms % 1000), name);                                    \
        \
    for (unsigned int i = 0; i < len; i++)                              \
    {                                                                   \
        if (i != 0 && !(i % 16))                                        \
        {                                                               \
            logf("%s", "\r\n");                                         \
        }                                                               \
        logf("%.2x ", ((uint8_t *)data)[i]);                             \
    }																	\
    logf("0x%x %u %u %u %u %u %u %d\r\n", arg1, arg2, arg3, arg4, arg5, arg6, \
      arg7, arg8);         \
  }

#define print_bytes(data, len, name) \
  log_bytes(log_debugf, log_debugf, data, len, name, 0, 0, 0, 0, 0)

#define info_bytes(data, len, name) \
  log_bytes(log_infof, log_infof, data, len, name, 0, 0, 0, 0, 0)

#define print_ptr(p, name) log_debugf("%s: %p\r\n", name, (void *)p)

#define hexdumpn(data, len, name, arg1, arg2, arg3, arg4, arg5)   \
  log_bytes(printf, printf, data, len, name, arg1, arg2, arg3, arg4, arg5)

#define hexdumpen(data, len, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)   \
  log_en_bytes(printf, printf, data, len, name, arg1, arg2, arg3, arg4, arg5,  \
		  arg6, arg7, arg8)

#endif
