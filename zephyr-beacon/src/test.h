#ifndef COMMON_TEST__H
#define COMMON_TEST__H

// TEST DATA

#include <stdint.h>
#include "settings.h"
#include "constants.h"


#define TEST_BEACON_ID ((BROADCAST_SERVICE_ID << 16) + 1)
//12345678987654321
#define TEST_BEACON_LOC_ID TEST_BEACON_ID
#define TEST_BEACON_INIT_TIME 10

#define TEST_DONGLE_ID 2222222222
#define TEST_DONGLE_INIT_TIME 10

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
//#define TEST_PAYLOAD_SIZE (TEST_N_FILTERS_PER_PAYLOAD * TEST_FILTER_LEN) // P

#if 0
#if MODE__NRF_BEACON_TEST_CONFIG
// Ephemeral IDs known to be in the test filter
static char *TEST_ID_EXIST_1 = "\x08\xb5\xec\x97\xaa\x06\xf8\x82\x27\xeb\x4e\x5a\x83\x72\x5b";
static char *TEST_ID_EXIST_2 = "\x3d\xbd\xb9\xc4\xf4\xe0\x9f\x1d\xc4\x30\x66\xda\xb8\x25\x3a";
// not in filter
static char *TEST_ID_NEXIST_1 = "blablablablabla";
static char *TEST_ID_NEXIST_2 = "tralalalalalala";
#endif /* MODE__NRF_BEACON_TEST_CONFIG */
#endif

#endif /* COMMON_TEST__H */
