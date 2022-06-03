#ifndef COMMON_TEST__H
#define COMMON_TEST__H

// TEST DATA

#include <stdint.h>
#include "settings.h"

#define TEST_BEACON_ID ((BROADCAST_SERVICE_ID << 16) + 0)
//12345678987654321
#define TEST_BEACON_LOC_ID TEST_BEACON_ID
#define TEST_BEACON_INIT_TIME 10

#define TEST_BACKEND_KEY_SIZE 8

#define TEST_BEACON_SK_SIZE 16

// Cuckoo filter testing

typedef uint32_t test_filter_size_t;

#define CF_SIZE_BYTES       \
  ((NUM_CF_BUCKETS * FINGERPRINT_BITS * ENTRIES_PER_BUCKET) /  \
  (BITS_PER_BYTE))

#define HDR_SIZE_BYTES      8

#define TEST_FILTER_LEN     (CF_SIZE_BYTES + HDR_SIZE_BYTES)
//#define TEST_FILTER_LEN   1736

#define TEST_N_FILTERS_PER_PAYLOAD 1

#endif /* COMMON_TEST__H */
