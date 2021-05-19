#include <stdint.h>

#define print_bytes(data, len, name) \
	if (name == NULL) \
		log_debug("data: 0x"); \
	else \
		log_debugf("%s: 0x", name); \
    for (int i = 0; i < len; i++) { \
		if (!(i % 16)) { \
			log_debug("\n"); \
		} \
        log_debugf(" %.2x", ((uint8_t*)data)[i]);        \
    }                               \
    log_debug("\n")
