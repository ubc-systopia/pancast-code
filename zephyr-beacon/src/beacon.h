#ifndef BEACON__H
#define BEACON__H

#include <assert.h>

#include "settings.h"
#include "constants.h"
#include "test.h"

#include <include/stats.h>


#define BEACON_PLATFORM__ZEPHYR

// #define BEACON_GAEN_ENABLED

#include <bluetooth/bluetooth.h>
#include <sha-2/sha-256.h>

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

/*
 * Advertising interval settings
 * Zephyr-recommended values are used
 */
#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1

typedef struct bt_data bt_data_t;

typedef struct
{
  uint8_t bytes[32];
} digest_t;

typedef struct Sha_256 hash_t;

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

typedef struct
{
  uint8_t storage_checksum; // zero for valid stat data
  beacon_timer_t duration;
  beacon_timer_t start;
  beacon_timer_t end;
  uint32_t cycles;
  uint32_t epochs;
} beacon_stats_t;

extern beacon_stats_t stats;

void _beacon_broadcast_(int);
void _beacon_info_();
int _set_adv_data_();
int beacon_clock_increment(beacon_timer_t time);

#endif /* BEACON__H */
