#ifndef COMMON_UTIL__H
#define COMMON_UTIL__H

// Some utility macros

#include <stdint.h>

#include "./log.h"

#define log_bytes(log, logf, data, len, name) \
    logf("%s: 0x", name);                     \
    for (unsigned int i = 0; i < len; i++)    \
    {                                         \
        if (!(i % 16))                        \
        {                                     \
            log("\r\n");                      \
        }                                     \
        logf(" %.2x", ((uint8_t *)data)[i]);  \
    }                                         \
    log("\r\n")

#define print_bytes(data, len, name) \
    log_bytes(log_debug, log_debugf, data, len, name)

#define info_bytes(data, len, name) \
    log_bytes(log_info, log_infof, data, len, name)

#define print_ptr(p, name) log_debugf("%s: %p\r\n", name, (void *)p)

#define hexdump(data, len) log_bytes(printf, printf, data, len, "hexdump")

#endif
