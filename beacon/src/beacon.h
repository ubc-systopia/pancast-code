#ifndef BEACON__H
#define BEACON__H

#include <assert.h>

#define BEACON_MODE__NETWORK // comment to run as non-network beacon
#define PERIODIC_TEST // uncomment to send test data

#define BEACON_PLATFORM__ZEPHYR

#define BEACON_GAEN_ENABLED

#define BEACON_NO_OP assert(1);

#include "../../common/src/pancast.h"

#ifdef BEACON_PLATFORM__ZEPHYR
#include <tinycrypt/sha256.h>
#include <bluetooth/bluetooth.h>
#define LOG_LEVEL__INFO
#else
#include "./sha256/sha-256.h"
#endif

typedef struct
{
    beacon_id_t beacon_id;                   // Beacon ID
    beacon_location_id_t beacon_location_id; // Location ID
    beacon_timer_t t_init;                   // Beacon Clock Start
    key_size_t backend_pk_size;              // size of backend public key
    pubkey_t backend_pk;                     // Backend public key
    key_size_t beacon_sk_size;               // size of secret key
    beacon_sk_t beacon_sk;                   // Secret Key
} beacon_config_t;

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

typedef union
{
    bt_data_t flags;
    bt_data_t serviceUUID;
    char service_data_internals[22];
} bt_gaen_wrapper_t;

#ifdef BEACON_PLATFORM__ZEPHYR
void main(void);
#else
void beacon_start();
#endif
#ifdef BEACON_PLATFORM__ZEPHYR
void _beacon_broadcast_(int);
#else
void beacon_broadcast();
#endif
void _beacon_info_();
int _set_adv_data_();
int beacon_clock_increment(beacon_timer_t time);

#endif
