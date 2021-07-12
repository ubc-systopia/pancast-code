#ifndef BEACON__H
#define BEACON__H

#include <assert.h>

//#define BEACON_MODE__NETWORK // remove to run as non-network beacon

#define BEACON_PLATFORM__GECKO

#define BEACON_NO_OP assert(1);

#include "../../common/src/pancast.h"

#ifdef BEACON_PLATFORM__ZEPHYR
#include <tinycrypt/sha256.h>
#include <bluetooth/bluetooth.h>
#else
#include "./sha256/sha-256.h"
#endif


// Advertising interval settings
// Zephyr-recommended values are used
#ifdef BEACON_PLATFORM__ZEPHYR
#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1

// sha256-hashing

typedef struct tc_sha256_state_struct hash_t;

typedef struct
{
    uint8_t bytes[TC_SHA256_DIGEST_SIZE];
} digest_t;

typedef struct bt_data bt_data_t;

#else
#define BEACON_ADV_MIN_INTERVAL 0x30
#define BEACON_ADV_MAX_INTERVAL 0x60

typedef struct
{
    uint8_t bytes[32];
} digest_t;

typedef struct Sha_256 hash_t;

typedef struct
{
    uint8_t data_len;
    uint8_t type;
    uint8_t data[29];
} bt_data_t;

#endif

typedef union
{
    encounter_broadcast_raw_t en_data;
    bt_data_t bt_data[1];
} bt_wrapper_t;

#ifdef BEACON_PLATFORM__ZEPHYR
void main(void);
#else
void beacon_start();
#endif
#ifdef BEACON_PLATFORM__ZEPHYR
static void _beacon_broadcast_(int);
#else
void beacon_broadcast();
#endif
static void _beacon_info_();
int _set_adv_data_();

#endif