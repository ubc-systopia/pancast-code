#include <stdint.h>

#define log_bytes(log, logf, data, len, name) \
	if (name == NULL) \
		log("data: 0x"); \
	else \
		logf("%s: 0x", name); \
    for (int i = 0; i < len; i++) { \
		if (!(i % 16)) { \
			log("\n"); \
		} \
        logf(" %.2x", ((uint8_t*)data)[i]);        \
    }                               \
    log("\n")

#define print_bytes(data, len, name) log_bytes(log_debug, log_debugf data, len, name)

#define info_bytes(data, len, name) log_bytes(log_info, log_infof, data, len, name)
