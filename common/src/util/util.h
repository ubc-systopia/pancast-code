#ifndef COMMON_UTIL__H
#define COMMON_UTIL__H

#include "log.h"

// Some utility macros

#include <stdint.h>

#define log_bytes(log, logf, data, len, name) \
    logf("%s: 0x", name);                     \
    for (unsigned int i = 0; i < len; i++)    \
    {                                         \
        if (!(i % 16))                        \
        {                                     \
            logf("%s", "\r\n");               \
        }                                     \
        logf(" %.2x", ((uint8_t *)data)[i]);  \
    }                                         \
    log("%s", "\r\n")

#define print_bytes(data, len, name) \
    log_bytes(log_debugf, log_debugf, data, len, name)

#define info_bytes(data, len, name) \
    log_bytes(log_infof, log_infof, data, len, name)

#define print_ptr(p, name) log_debugf("%s: %p\r\n", name, (void *)p)

#define hexdumpn(data, len, name) log_bytes(printf, printf, data, len, name)
#define hexdump(data, len) hexdumpn(data, len, "hexdump")

#endif
