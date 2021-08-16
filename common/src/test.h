#ifndef COMMON_TEST__H
#define COMMON_TEST__H

// TEST DATA

#include <stdint.h>
#include "./pancast.h"


#define TEST_BEACON_ID ((BROADCAST_SERVICE_ID << 16) + 1)
#define TEST_BEACON_LOC_ID 12345678987654321
#define TEST_BEACON_INIT_TIME 10

#define TEST_DONGLE_ID 2222222222
#define TEST_DONGLE_INIT_TIME 10

#define TEST_BACKEND_KEY_SIZE 6
#define TEST_BEACON_BACKEND_KEY_SIZE TEST_BACKEND_KEY_SIZE

static pubkey_t TEST_BACKEND_PK = {{0xad, 0xf4, 0xca, 0x6c, 0xa6, 0xd9}};

#define TEST_BEACON_SK_SIZE 16

static beacon_sk_t TEST_BEACON_SK = {{0xcb, 0x43, 0xf7, 0x56, 0x16, 0x25, 0xb3,
                                      0xd0, 0xd0, 0xbe, 0xad, 0xf4, 0xca, 0x6c,
                                      0xa6, 0xd9}};

#define TEST_DONGLE_SK_SIZE 16

static beacon_sk_t TEST_DONGLE_SK = {{0xdc, 0x34, 0x6a, 0xdd, 0xa3, 0x41, 0xf4,
                                      0x23, 0x33, 0xc0, 0xf0, 0xcf, 0x61, 0xc6,
                                      0xd6, 0xd5}};

#define PERIODIC_TEST_NUM_PACKETS 50

// Cuckoo filter testing

typedef uint32_t test_filter_size_t;

#define TEST_FILTER_LEN 55304

#define TEST_PAYLOAD_SIZE TEST_FILTER_LEN                                   // P
#define TEST_PACKET_SIZE 246                                                // S
#define TEST_NUM_PACKETS (1 + ((TEST_PAYLOAD_SIZE - 1) / TEST_PACKET_SIZE)) // N

// Ephemeral IDs known to be in the test filter
static char *TEST_ID_EXIST_1 = "1dad8ea6111f7db";
static char *TEST_ID_EXIST_2 = "5f8465db3b720c8";
// not in filter
static char *TEST_ID_NEXIST_1 = "blablablablabla";
static char *TEST_ID_NEXIST_2 = "tralalalalalala";

#endif
