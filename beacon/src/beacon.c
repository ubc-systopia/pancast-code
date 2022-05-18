//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include "./beacon.h"
#include "app.h"
#include "common/src/util/stats.h"

#define APPL_VERSION "0.1.1"

#define MODE__STAT

#define LOG_LEVEL__DEBUG

#include <string.h>

#include "sl_bluetooth.h"
#include "app_log.h"
#include "led.h"
#include "storage.h"

#include "common/src/constants.h"
#include "common/src/settings.h"
#include "common/src/util/util.h"
#include "common/src/util/log.h"
#include "common/src/test.h"

//
// GLOBAL MEMORY
//
extern float adv_start;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;

// Config
beacon_config_t config;

// Default Operation
beacon_storage storage;
sl_sleeptimer_timer_handle_t led_timer;

static beacon_timer_t beacon_time;    // Beacon Clock
static beacon_eph_id_t beacon_eph_id; // Ephemeral ID
static beacon_epoch_counter_t epoch;  // track the current time epoch
static beacon_timer_t cycles;         // total number of updates.
//
// Bluetooth
// Advertising handle
static uint8_t legacy_set_handle = 0xf1;
static bt_wrapper_t payload; // container for actual blutooth payload

// Statistics
#ifdef MODE__STAT
static beacon_timer_t stat_start;
static beacon_timer_t stat_cycles;
static beacon_timer_t stat_epochs;
uint32_t stat_sent_broadcast_packets = 0;
uint32_t stat_total_packets_sent = 0;
uint32_t stat_crc_errors = 0;
uint32_t stat_failures = 0;
#endif

#if MODE__SL_BEACON_TEST_CONFIG
static pubkey_t TEST_BACKEND_PK = {
  {0xad, 0xf4, 0xca, 0x6c, 0xa6, 0xd9, 0x11, 0x22}
};

static beacon_sk_t TEST_BEACON_SK = {
  {0xcb, 0x43, 0xf7, 0x56, 0x16, 0x25, 0xb3, 0xd0,
   0xd0, 0xbe, 0xad, 0xf4, 0x55, 0x66, 0x77, 0x99}
};
#endif

//
// ROUTINES
//

static void beacon_load()
{
  beacon_storage_init(&storage);
  // Load data
  beacon_storage_load_config(&storage, &config);
#if MODE__SL_BEACON_TEST_CONFIG
  config.beacon_id = TEST_BEACON_ID;
  config.beacon_location_id = TEST_BEACON_LOC_ID;
  config.t_init = TEST_BEACON_INIT_TIME;
  config.backend_pk_size = TEST_BACKEND_KEY_SIZE;
  memcpy(&config.backend_pk, &TEST_BACKEND_PK, config.backend_pk_size);
  config.beacon_sk_size = TEST_BEACON_SK_SIZE;
  memcpy(&config.beacon_sk, &TEST_BEACON_SK, config.beacon_sk_size);
  storage.test_filter_size = TEST_FILTER_LEN;
  beacon_storage_save_config(&storage, &config);
#endif
}

void beacon_info()
{
  log_infof("%s", "=== Beacon Info: ===\r\n");
  log_infof("    Platform:                 %s\r\n", "Zephyr OS");
#ifndef BEACON_PLATFORM__ZEPHYR
#define UK "Unknown"
#define CONFIG_BOARD UK
#define CONFIG_BT_DEVICE_NAME UK
#endif
  log_infof("    Board:                    %s\r\n", CONFIG_BOARD);
  log_infof("    Bluetooth device name:    %s\r\n", CONFIG_BT_DEVICE_NAME);
  log_infof("    Application version:      %s\r\n", APPL_VERSION);
  log_infof("    Beacon ID:                0x%x\r\n", config.beacon_id);
  log_infof("    Location ID:              0x%lx\r\n", config.beacon_location_id);
  log_infof("    Initial clock:            %u\r\n", config.t_init);
  log_infof("    Current clock:            %u\r\n", config.t_cur);
  log_infof("    Timer frequency:          %u Hz\r\n",
      sl_sleeptimer_get_timer_frequency());
  log_infof("    Backend public key size:  %u bytes\r\n", config.backend_pk_size);
  log_infof("    Secret key size:          %u bytes\r\n", config.beacon_sk_size);
  log_infof("    Timer resolution:         %u ms\r\n", BEACON_TIMER_RESOLUTION);
  log_infof("    Epoch length:             %u ms\r\n",
      BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);
  log_infof("    Report interval:          %u ms\r\n",
      BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
  log_infof("    Legacy adv interval:      %x-%x ms\r\n",
      BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL);
  log_infof("    Periodic adv pkt size:    %u bytes\r\n", PER_ADV_SIZE);

  log_infof("    Flash page size, count:   %u B, %u\r\n",
      storage.page_size, storage.num_pages);
  log_infof("    Flash offset:             %u\r\n", storage.map.config);
  log_infof("    Total size:               %u\r\n", storage.total_size);
  log_infof("    Test filter offset:       %u\r\n", storage.map.test_filter);

  log_infof("    Test filter length:       %lu\r\n", storage.test_filter_size);
  log_infof("    Config offset:            %u\r\n", storage.map.config);
  log_infof("    Stat offset:              %u\r\n", storage.map.stat);
#ifdef MODE__STAT
  log_infof("%s", "    Statistics mode enabled\r\n");
#endif
}

void _beacon_periodic_info()
{
  log_infof("    Periodic Interval:                %u\r\n", PER_ADV_INTERVAL);
  log_infof("    Min Sync Advertising Interval:    %u\r\n", MIN_ADV_INTERVAL);
  log_infof("    Max Sync Advertising Interval:    %u\r\n", MAX_ADV_INTERVAL);
  log_infof("    Tx power:                         %u\r\n", PER_TX_POWER);
}

#ifdef MODE__STAT
beacon_stats_t stats;

void beacon_stats_reset()
{
  memset(&stats, 0, sizeof(beacon_stats_t));
}

void beacon_stat_update()
{
  // Copy data
  // TODO use the stats containers from the start
  stats.duration = beacon_time - stat_start;
  stats.start = stat_start;
  stats.end = beacon_time;
  stats.cycles = stat_cycles;
  stats.epochs = stat_epochs;
  stats.sent_broadcast_packets = stat_sent_broadcast_packets;
  stats.total_packets_sent = stat_total_packets_sent;
  stats.crc_errors = stat_crc_errors;
  stats.failures = stat_failures;
}

static void beacon_stats()
{
  log_infof("[%lu] last report time: %lu, #cycles: %u, "
      "#epochs: %u chksum: %u\r\n",
      beacon_time, stats.start, stats.cycles,
      stats.epochs, stats.storage_checksum);
  log_infof("sent broadcast packets: %u, total sent packets: %u, "
		  "crc errors: %u, failures: %u\r\n",
		  stats.sent_broadcast_packets, stats.total_packets_sent,
		  stats.crc_errors, stats.failures);
  stat_show(stats.broadcast_payload_update_duration,
      "[broadcast payload] Update duration", "ms");
}

#endif

static void _beacon_report_()
{
#ifdef MODE__STAT
  if (beacon_time - stat_start < BEACON_REPORT_INTERVAL)
    return;

  beacon_stat_update();
  beacon_stats();
  beacon_storage_save_stat(&storage, &stats, sizeof(beacon_stats_t));
  beacon_stats_reset();
  stat_start = beacon_time;
  stat_cycles = 0;
  stat_epochs = 0;
#endif
}

/* Log system counters */
void beacon_log_counters()
{
  uint16_t tx_packets;
  uint16_t rx_packets;
  uint16_t crc_errors;
  uint16_t failures;

  sl_bt_system_get_counters(1, &tx_packets, &rx_packets,
      &crc_errors, &failures);

  stat_total_packets_sent += (uint32_t) tx_packets;
  stat_crc_errors += (uint32_t) crc_errors;
  stat_failures += (uint32_t) failures;

  stat_sent_broadcast_packets++;
  // log_debugf("tx_packets: %lu, rx_packets: %lu, crc_errors: %lu, failures: %lu\r\n", tx_packets, rx_packets, crc_errors, failures);
}

void beacon_reset_counters()
{
  sl_bt_system_get_counters(1, 0, 0, 0, 0);
}

// Set transmission power (zephyr).
// Yoinked from (https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/hci_pwr_ctrl/src/main.c)


// Intermediary transformer to create a well-formed BT data type
// for using the high-level APIs. Becomes obsolete once advertising
// routine supports a full raw payload
static void _form_payload_()
{
  const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
#define bt (payload.bt_data)
  uint8_t tmp;
  tmp = bt->data_len;
  bt->data_len = bt->type;
  bt->type = tmp;
  tmp = bt->data_len;
  bt->data_len = len;
#define en (payload.en_data)
  en.bytes[MAX_BROADCAST_SIZE - 1] = tmp;
#undef en
#undef bt
}

// pack a raw byte payload by copying from the high-level type
// order is important here so as to avoid unaligned access on the
// receiver side
static void _encode_encounter_()
{
  uint8_t *dst = (uint8_t *) &payload.en_data;
  size_t pos = 0;
#define copy(src, size)           \
  memcpy(dst + pos, src, size); \
  pos += size
  copy(&beacon_time, sizeof(beacon_timer_t));
  copy(&config.beacon_id, sizeof(beacon_id_t));
  copy(&config.beacon_location_id, sizeof(beacon_location_id_t));
  copy(&beacon_eph_id, sizeof(beacon_eph_id_t));
#undef copy
  hexdumpen(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "eph ID",
      config.beacon_id, (uint32_t) config.beacon_location_id, 0,
      (uint32_t) beacon_time, 0, (uint32_t) 0, 0, 0);
}

static void _beacon_encode_()
{
  // Load broadcast into bluetooth payload
  _encode_encounter_();
  _form_payload_();
}

static void _gen_ephid_()
{
  hash_t h;
  digest_t d;
#define init() sha_256_init(&h, d.bytes)
#define add(data, size) sha_256_write(&h, data, size)
#define complete() sha_256_close(&h)

  // Initialize hash
  init();
  // Add relevant data
  add(&config.beacon_sk, config.beacon_sk_size);
  add(&config.beacon_location_id, sizeof(beacon_location_id_t));
  add(&epoch, sizeof(beacon_epoch_counter_t));
  // finalize and copy to id
  complete();
  memcpy(&beacon_eph_id, &d, BEACON_EPH_ID_HASH_LEN); // Little endian so these are the least significant
#undef complete
#undef add
#undef init
}

static void beacon_stats_init()
{
#ifdef MODE__STAT
  stat_epochs = 0;
  stat_start = beacon_time;
  beacon_storage_read_stat(&storage, &stats, sizeof(beacon_stats_t));
  if (!stats.storage_checksum) {
    log_infof("%s", "Existing Statistics Found\r\n");
    beacon_stats();
  } else {
    log_errorf("init stats checksum: 0x%0x\r\n", stats.storage_checksum);
    beacon_stats_reset();
  }
#endif
}

static void _beacon_epoch_()
{
  static beacon_epoch_counter_t old_epoch;
  old_epoch = epoch;
  epoch = epoch_i(beacon_time, config.t_init);
  if (!cycles || epoch != old_epoch) {
    log_debugf("EPOCH STARTED: %u\r\n", epoch);
    // When a new epoch has started, generate a new ephemeral id
    _gen_ephid_();
    if (epoch != old_epoch) {
#ifdef MODE__STAT
      stat_epochs++;
#endif
    }
    // TODO: log time to flash
  }
}

int _set_adv_data_()
{
  log_debugf("%s", "Setting legacy adv data...\r\n");
  print_bytes(payload.en_data.bytes, MAX_BROADCAST_SIZE, "adv_data");
  sl_status_t sc = sl_bt_advertiser_set_data(legacy_set_handle,
      0, 31, payload.en_data.bytes);
  if (sc != 0) {
    log_errorf("Error, sc: 0x%lx\r\n", sc);
    return -1;
  }
  log_debugf("%s", "Success!\r\n");
  return 0;
}

sl_status_t beacon_legacy_advertise()
{
  sl_status_t sc;
  log_debugf("%s", "Creating legacy advertising set\r\n");
  sc = sl_bt_advertiser_create_set(&legacy_set_handle);
  if (sc != 0) {
    log_errorf("Error, sc: 0x%lx\r\n", sc);
    return sc;
  }

  // Set Power Level
  int16_t set_power;
  sc = sl_bt_advertiser_set_tx_power(legacy_set_handle,
      LEGACY_TX_POWER, &set_power);

  log_infof("Tx power: %d\r\n", set_power);

  log_infof("%s", "=== Starting legacy advertising... ===\r\n");
  // Set advertising interval to 100ms.
  sc = sl_bt_advertiser_set_timing(legacy_set_handle,
      BEACON_ADV_MIN_INTERVAL, // min. adv. interval (milliseconds * 1.6)
      BEACON_ADV_MAX_INTERVAL, // max. adv. interval (milliseconds * 1.6) next: 0x4000
      0,                       // adv. duration, 0 for continuous advertising
      0);                      // max. num. adv. events
  if (sc != 0) {
    log_errorf("Error, sc: 0x%lx\r\n", sc);
    return sc;
  }
  // Start legacy advertising
  sc = sl_bt_advertiser_start(legacy_set_handle, advertiser_user_data,
      advertiser_non_connectable);
  if (sc != 0) {
    log_errorf("Error starting advertising, sc: 0x%lx\r\n", sc);
    return sc;
  }

  return sc;
#if 0
  // TODO: check if this needed, given payload set in beacon_on_clock_update()
  err = _set_adv_data_();
  log_infof("set adv data, err: %d\r\n", err);
  return err;
#endif
}

static int _beacon_pause_()
{
  cycles++;
#ifdef MODE__STAT
  stat_cycles++;
#endif
  _beacon_report_();
  return 0;
}

void beacon_on_clock_update()
{
  _beacon_epoch_();
  _beacon_encode_();
  _set_adv_data_();
}

int beacon_clock_increment(beacon_timer_t time)
{
  beacon_time += time;
  log_debugf("beacon timer: %u\r\n", beacon_time);
#if BEACON_MODE__NETWORK == 0
  beacon_on_clock_update();
#endif
  // update beacon time in config and save to flash
  config.t_cur = beacon_time;
  beacon_storage_save_config(&storage, &config);
  _beacon_pause_();
  return 0;
}

/*
 * Initialize Pancast-specific beacon configs
 */
void beacon_init()
{
  log_debugf("%s", "Init beacon\r\n");

  beacon_load();
  beacon_info();

  epoch = 0;
  cycles = 0;

  beacon_time = config.t_cur > config.t_init ? config.t_cur : config.t_init;

  beacon_stats_init();

  configure_blinky();
}

beacon_storage *get_beacon_storage()
{
  return &storage;
}

void beacon_periodic_advertise()
{
  int sc = 0;

  /*
   * create an advertising set object
   */
  sc = sl_bt_advertiser_create_set(&advertising_set_handle);
  if (sc != 0) {
    log_errorf("error creating advertising set, sc: 0x%X", sc);
  }

#if 0
  /*
   * set PHY config
   */
  sc = sl_bt_advertiser_set_phy(advertising_set_handle,
      sl_bt_gap_1m_phy, sl_bt_gap_2m_phy);
  app_assert_status(sc);
#endif

  /*
   * set advertising power Level
   */
  int16_t set_power;
  sc = sl_bt_advertiser_set_tx_power(advertising_set_handle,
      PER_TX_POWER, &set_power);
  if (sc != 0) {
    log_errorf("error setting advertiser tx power, sc: 0x%X", sc);
  }

  /*
   * set advertising interval to 100ms.
   */
  sc = sl_bt_advertiser_set_timing(advertising_set_handle,
      MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
      MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
      NO_MAX_DUR,       // adv. duration
      NO_MAX_EVT);      // max. num. adv. events
  if (sc != 0) {
    log_errorf("Error setting channel map, sc: 0x%X", sc);
  }

  log_debugf("%s", "Starting periodic advertising...\r\n");

  sc = sl_bt_advertiser_start_periodic_advertising(
      advertising_set_handle, PER_ADV_INTERVAL, PER_ADV_INTERVAL,
      PER_FLAGS);

  if (sc != 0) {
    log_errorf("Error setting channel map, sc: 0x%X", sc);
  }
}

/*
 * Update risk data after receive from raspberry pi client
 */
void set_risk_data(int len, uint8_t *data)
{
  sl_status_t sc = 0;

  // cannot send more than PER_ADV_SIZE bytes at a time
  // we expect the caller to chunk the data into appropriate size
  if (len > PER_ADV_SIZE)
    return;

  sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, len, data);
  if (sc != 0) {
    log_infof("[periodic adv] set data err, sc: 0x%lx\r\n", sc);
  }
}

#undef MODE__STAT
#undef APPL_VERSION
