#ifndef BEACON__H
#define BEACON__H

#include <bluetooth/bluetooth.h>
#include <sha-2/sha-256.h>

#include <assert.h>

#include <settings.h>
#include <stats.h>

// #define BEACON_GAEN_ENABLED

typedef struct
{
  beacon_id_t beacon_id;                   // Beacon ID
  beacon_location_id_t beacon_location_id; // Location ID
  beacon_timer_t t_init;                   // Beacon initial clock
  beacon_timer_t t_cur;                    // Beacon current clock
  key_size_t backend_pk_size;              // Size of backend public key
  pubkey_t *backend_pk;                    // Backend public key
  key_size_t beacon_sk_size;               // Size of secret key
  beacon_sk_t *beacon_sk;                  // Secret Key
} beacon_config_t;

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
  /*
   * zero for valid stat data
   */
  uint8_t storage_checksum;
  beacon_timer_t start;
  /*
   * total number of epochs elapsed
   */
  uint32_t epochs;
  /*
   * total number of legacy adv. packets sent
   */
  uint32_t sent_broadcast_packets;
} beacon_stats_t;

extern beacon_stats_t stats;

void _beacon_broadcast_(int err, int reset);
void pancast_zephyr_beacon(int err);
int _set_adv_data_();
int beacon_clock_increment(beacon_timer_t time);

#endif /* BEACON__H */
