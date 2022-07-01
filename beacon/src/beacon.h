#ifndef BEACON__H
#define BEACON__H

#include <assert.h>

#include "sl_bluetooth.h"
#include "common/src/settings.h"
#include "common/src/util/stats.h"

/*
 * config to broadcast test risk data
 * 1 - test risk data
 * 0 - real risk data from backend
 */
#define PERIODIC_TEST   0

/*
 * 1 - init test config
 * 0 - load config from storage
 */
#define MODE__SL_BEACON_TEST_CONFIG  0

/*
 * config to enable periodic advertising in network beacon
 * 1 - enable periodic advertising
 * 0 - disable periodic advertising
 */
#define BEACON_MODE__NETWORK    1

// #define BEACON_GAEN_ENABLED

#include "common/src/constants.h"
#include "common/src/settings.h"
#include "common/src/test.h"
#include "./sha-2/sha-256.h"


typedef struct
{
  beacon_id_t beacon_id;                   // Beacon ID
  beacon_location_id_t beacon_location_id; // Location ID
  beacon_timer_t t_init;                   // Beacon initial clock
  beacon_timer_t t_cur;                    // Beacon current clock
  key_size_t backend_pk_size;              // Size of backend public key
  pubkey_t *backend_pk;                     // Backend public key
  key_size_t beacon_sk_size;               // Size of secret key
  beacon_sk_t *beacon_sk;                   // Secret Key
} beacon_config_t;

/*
 * beacon Tx power
 * units: 0.1 dbm, i.e.,
 * if min_tx_power = -3, power config is -0.3 dbm
 */
#define GLOBAL_TX_POWER 5
#define MIN_TX_POWER -3
#define MAX_TX_POWER 10 // device maximum is 8.5 dbm

/*
 * Advertising interval settings
 *
 * unit is 0.625 ms, i.e,
 * if min_interval = 0x320, actual interval is 500ms
 */
#define BEACON_ADV_MIN_INTERVAL 0x300 // 480ms
#define BEACON_ADV_MAX_INTERVAL 0x320 // 500ms
#define LEGACY_TX_POWER GLOBAL_TX_POWER

/* Periodic Advertising */
#define PER_ADV_HANDLE 0xff
#define PER_ADV_MIN_INTERVAL 0x20  // min. adv. interval (milliseconds * 1.6)
#define PER_ADV_MAX_INTERVAL 0x20  // max. adv. interval (milliseconds * 1.6)
#define PER_ADV_INTERVAL 10    // per. adv. interval (units of 1.25ms)
#define PER_FLAGS 0            // no periodic advertising flags
#define PER_TX_POWER GLOBAL_TX_POWER

/* Timers */
#define LED_TIMER_MS 1000 // one second in ms, used for timer
#define MAIN_TIMER_HANDLE 0
#define MAIN_TIMER_PRIORT 1

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
  uint32_t total_packets_sent;
  uint32_t crc_errors;
  uint32_t failures;
  stat_t broadcast_payload_update_duration;
} beacon_stats_t;

extern beacon_stats_t stats;

void beacon_init();
void beacon_broadcast();
int _set_adv_data_();
void beacon_on_clock_update(void);
int beacon_clock_increment(beacon_timer_t time);
sl_status_t beacon_legacy_advertise();
void beacon_periodic_advertise();
void set_risk_data(int len, uint8_t *data);

#endif
