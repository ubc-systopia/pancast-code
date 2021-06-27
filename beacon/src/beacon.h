#ifndef BEACON__H
#define BEACON__H

#include "../../common/src/pancast.h"

#include <tinycrypt/sha256.h>
#include <bluetooth/bluetooth.h>

#define BEACON_REPORT_INTERVAL 2

// Advertising interval settings
// Zephyr-recommended values are used
#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1

// sha256-hashing

typedef struct tc_sha256_state_struct hash_t;

typedef struct
{
    uint8_t bytes[TC_SHA256_DIGEST_SIZE];
} digest_t;

typedef struct bt_data bt_data_t;

typedef union
{
    encounter_broadcast_raw_t en_data;
    bt_data_t bt_data[1];
} bt_wrapper_t;

#ifdef APPL__BEACON
void main(void);
#else
void _beacon_main_();
#endif
static void _beacon_broadcast_(int);
static void _beacon_info_();

#endif