#include <stdint.h>

#define print_bytes(prf, data, len) \
    prf("Data: 0x"); \
    for (int i = 0; i < len; i++) { \
        prf(" %x", ((uint8_t*)data)[i]);        \
    }                               \
    printk("\n")
