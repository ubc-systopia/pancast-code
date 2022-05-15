#ifndef COMMON_TEST__H
#define COMMON_TEST__H

// TEST DATA

#include <stdint.h>
#include "constants.h"


#define TEST_BEACON_ID ((BROADCAST_SERVICE_ID << 16) + 1)
//12345678900000001
#define TEST_BEACON_LOC_ID TEST_BEACON_ID
#define TEST_BEACON_INIT_TIME 10

//2222222222
#define TEST_DONGLE_ID 0x11110001
#define TEST_DONGLE_INIT_TIME 10

#define TEST_BACKEND_KEY_SIZE 8

#define TEST_BEACON_SK_SIZE 16

#define TEST_DONGLE_SK_SIZE 16

/*
 * =====================
 * Cuckoo filter testing
 * =====================
 */

typedef uint32_t test_filter_size_t;

#define TEST_FILTER_LEN     (CF_SIZE_BYTES + HDR_SIZE_BYTES)
//#define TEST_FILTER_LEN   1736

#define TEST_N_FILTERS_PER_PAYLOAD 1

#endif
