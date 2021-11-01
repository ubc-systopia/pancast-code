//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include "./beacon.h"
#include "app.h"

#define APPL_VERSION "0.1.1"

#define MODE__STAT

//#define MODE__DISABLE_LEGACY_DATA

#define LOG_LEVEL__DEBUG

#include <string.h>

#ifdef BEACON_PLATFORM__ZEPHYR
#include <zephyr.h>
#include <stddef.h>

#include <sys/printk.h>
#include <sys/util.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include <drivers/flash.h>
#else
#include "sl_bluetooth.h"
#include "app_log.h"
#endif

#include "storage.h"

#include "common/src/constants.h"
#include "common/src/settings.h"
#include "common/src/util/util.h"
#include "common/src/util/log.h"
#include "common/src/test.h"

//
// ENTRY POINT
//

#ifdef BEACON_PLATFORM__ZEPHYR
void main(void)
#else
void beacon_start()
#endif
{
#ifdef BEACON_PLATFORM__ZEPHYR
  int err = bt_enable(_beacon_broadcast_);
  if (err) {
    log_errorf("Bluetooth Enable Failure: error code = %d\r\n", err);
  }
#else
  beacon_broadcast();
#endif
}

//
// GLOBAL MEMORY
//

// Config
beacon_config_t config;

// Default Operation
beacon_storage storage;
static beacon_timer_t beacon_time;    // Beacon Clock
static beacon_eph_id_t beacon_eph_id; // Ephemeral ID
static beacon_epoch_counter_t epoch;  // track the current time epoch
static beacon_timer_t cycles;         // total number of updates.
#ifdef BEACON_PLATFORM__ZEPHYR
static struct k_timer kernel_time_lp;         // low-precision kernel timer
static struct k_timer kernel_time_hp;         // high-precision kernel timer
static struct k_timer kernel_time_alternater; // medium-precision kernel timer used to alternate packet generation
#endif
//
// Bluetooth
#ifdef BEACON_PLATFORM__ZEPHYR
static bt_data_t adv_res[] = {
  BT_DATA(BT_DATA_NAME_COMPLETE,
  CONFIG_BT_DEVICE_NAME,
  sizeof(CONFIG_BT_DEVICE_NAME) - 1)
}; // Advertising response
// fields for constructing packet
static uint8_t service_data_type = 0x16;
static bt_gaen_wrapper_t gaen_payload = {
  .flags = BT_DATA_BYTES(0x01, 0x1a),
  .serviceUUID = BT_DATA_BYTES(0x03, 0x6f, 0xfd)
}; // container for gaen broadcast data
static uint32_t num_ms_in_min = 60000;
static uint32_t alt_time_in_ms = 1000; // alt clock tick time (currently 1s)
#else
// Advertising handle
static uint8_t legacy_set_handle = 0xf1;
#endif
static bt_wrapper_t payload; // container for actual blutooth payload
//
// Reporting
static beacon_timer_t report_time; // Report tracking clock
//
// Statistics
#ifdef MODE__STAT
static beacon_timer_t stat_start;
static beacon_timer_t stat_cycles;
static beacon_timer_t stat_epochs;
uint32_t stat_sent_broadcast_packets;
uint32_t stat_total_packets_sent;
#endif

//
// ROUTINES
//

static void _beacon_load_()
{
  beacon_storage_init(&storage);
  // Load data
#ifdef MODE__TEST_CONFIG
  config.beacon_id = TEST_BEACON_ID;
  config.beacon_location_id = TEST_BEACON_LOC_ID;
  config.t_init = TEST_BEACON_INIT_TIME;
  config.backend_pk_size = TEST_BEACON_BACKEND_KEY_SIZE;
  memcpy(&config.backend_pk, &TEST_BACKEND_PK, config.backend_pk_size);
  config.beacon_sk_size = TEST_BEACON_SK_SIZE;
  memcpy(&config.beacon_sk, &TEST_BEACON_SK, config.beacon_sk_size);
  storage.test_filter_size = TEST_FILTER_LEN;
#else
  beacon_storage_load_config(&storage, &config);
#endif
}

void _beacon_info_()
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
  log_infof("    Test Filter Length:       %lu\r\n", storage.test_filter_size);
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
typedef struct
{
  uint8_t storage_checksum; // zero for valid stat data
  beacon_timer_t duration;
  beacon_timer_t start;
  beacon_timer_t end;
  uint32_t cycles;
  uint32_t epochs;
  uint32_t sent_broadcast_packets;
  uint32_t total_packets_sent;
} beacon_stats_t;

beacon_stats_t stats;

void beacon_stats_init()
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
}

static void _beacon_stats_()
{
  log_infof("[%lu] last report time: %lu, #cycles: %u, #epochs: %u\r\n",
      beacon_time, stats.start, stats.cycles, stats.epochs);
  beacon_storage_save_stat(&storage, &stats, sizeof(beacon_stats_t));
  beacon_stats_init();
}

static void _beacon_error_rate_stats_()
{
  log_infof("sent broadcast packets: %lu, total sent packets: %lu\r\n",
		  stats.sent_broadcast_packets, stats.total_packets_sent);

  beacon_storage_save_stat(&storage, &stats, sizeof(beacon_stats_t));
  beacon_stats_init();
}
#endif

static void _beacon_report_()
{
  if (beacon_time - report_time < BEACON_REPORT_INTERVAL)
    return;

  report_time = beacon_time;
#ifdef MODE__STAT
  beacon_stat_update();
  _beacon_stats_();
  _beacon_error_rate_stats_();
  stat_start = beacon_time;
  stat_cycles = 0;
  stat_epochs = 0;
#endif
}

/* Log system counters */
void beacon_log_counters()
{
  uint16_t  	tx_packets;
  uint16_t  	rx_packets;
  uint16_t 	    crc_errors;
  uint16_t   	failures;

  sl_bt_system_get_counters(1, &tx_packets, &rx_packets, &crc_errors, &failures);

  stat_total_packets_sent = stat_total_packets_sent + (uint32_t)tx_packets;

  log_debugf("tx_packets: %lu, rx_packets: %lu, crc_errors: %lu, failures: %lu\r\n", tx_packets, rx_packets, crc_errors, failures);
}

void beacon_reset_counters()
{
  sl_bt_system_get_counters(1, 0, 0, 0, 0);
}

void add_sent_packet()
{
	stat_sent_broadcast_packets++;
}

// Set transmission power (zephyr).
// Yoinked from (https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/hci_pwr_ctrl/src/main.c)
#ifdef BEACON_PLATFORM__ZEPHYR
static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
  struct bt_hci_cp_vs_write_tx_power_level *cp;
  struct bt_hci_rp_vs_write_tx_power_level *rp;
  struct net_buf *buf, *rsp = NULL;
  int err;

  buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
                          sizeof(*cp));
  if (!buf) {
    printk("Unable to allocate command buffer\n");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);
  cp->handle_type = handle_type;
  cp->tx_power_level = tx_pwr_lvl;

  err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
  if (err) {
    uint8_t reason = rsp ?
      ((struct bt_hci_rp_vs_write_tx_power_level *) rsp->data)->status
      : 0;
    printk("Set Tx power err: %d reason 0x%02x\n", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  printk("Actual Tx Power: %d\n", rp->selected_tx_power);

  net_buf_unref(rsp);
}
#endif

// Intermediary transformer to create a well-formed BT data type
// for using the high-level APIs. Becomes obsolete once advertising
// routine supports a full raw payload
static void _form_payload_()
{
  const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
#define bt (payload.bt_data)
  uint8_t tmp;
#ifdef BEACON_PLATFORM__GECKO
  tmp = bt->data_len;
  bt->data_len = bt->type;
  bt->type = tmp;
#endif
  tmp = bt->data_len;
  bt->data_len = len;
#define en (payload.en_data)
  en.bytes[MAX_BROADCAST_SIZE - 1] = tmp;
#undef en
#ifdef BEACON_PLATFORM__ZEPHYR
  static uint8_t of[MAX_BROADCAST_SIZE - 2];
  memcpy(of, ((uint8_t *)bt) + 2, MAX_BROADCAST_SIZE - 2);
  bt->data = (uint8_t *)&of;
#endif
#undef bt
}

// pack a raw byte payload by copying from the high-level type
// order is important here so as to avoid unaligned access on the
// receiver side
static void _encode_encounter_()
{
  uint8_t *dst = (uint8_t *)&payload.en_data;
  size_t pos = 0;
#define copy(src, size)           \
  memcpy(dst + pos, src, size); \
  pos += size
  copy(&beacon_time, sizeof(beacon_timer_t));
  copy(&config.beacon_id, sizeof(beacon_id_t));
  copy(&config.beacon_location_id, sizeof(beacon_location_id_t));
  copy(&beacon_eph_id, sizeof(beacon_eph_id_t));
#undef copy
  hexdumpn(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "eph ID",
      config.beacon_id, 0, beacon_time);
}

static void _beacon_encode_()
{
  // Load broadcast into bluetooth payload
  _encode_encounter_();
  //print_bytes(payload.en_data.bytes, MAX_BROADCAST_SIZE, "adv_data pre-encode");
  _form_payload_();
}

static void _gen_ephid_()
{
  hash_t h;
  digest_t d;
#ifdef BEACON_PLATFORM__ZEPHYR
#define init() tc_sha256_init(&h)
#define add(data, size) tc_sha256_update(&h, (uint8_t *)data, size)
#define complete() tc_sha256_final(d.bytes, &h)
#else
#define init() sha_256_init(&h, d.bytes)
#define add(data, size) sha_256_write(&h, data, size)
#define complete() sha_256_close(&h)
#endif
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

// populates gaen_payload.service_data_internals with procotol specific data fields
static int _set_adv_data_gaen_()
{
#ifdef BEACON_PLATFORM__ZEPHYR
  char ENS_identifier[2] = "\x6f\xfd";
  char rolling_proximity_identifier[16] = "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
  char associated_encrypted_metadata[4] = "\xaa\xaa\xaa\xaa";
  for (int i = 0; i < 22; i++) {
    if (i < 2) {
      gaen_payload.service_data_internals[i] = ENS_identifier[i];
    } else if (i >= 18) {
      gaen_payload.service_data_internals[i] = associated_encrypted_metadata[i - 18];
    } else {
      gaen_payload.service_data_internals[i] = rolling_proximity_identifier[i - 2];
    }
  }
  return 0;
#endif
  return -1;
}

// procedure to change the type of packet being transmitted
#ifdef BEACON_PLATFORM__ZEPHYR
void _alternate_advertisement_content_(uint32_t timer)
{
  if (timer % 2 == 1) {
    // currently transmitting pancast data, switch to gaen data
    int err = _set_adv_data_gaen_();
    if (err) {
      log_errorf("%s", "Failed to obtain gaen advertisement data");
      return;
    }
    bt_data_t serviceData = BT_DATA(service_data_type,
        gaen_payload.service_data_internals,
        ARRAY_SIZE(gaen_payload.service_data_internals));
    bt_data_t flags = BT_DATA_BYTES(0x01, 0x1a);
    bt_data_t serviceUUID = BT_DATA_BYTES(0x03, 0x6f, 0xfd);
    bt_data_t data[3] = {flags, serviceUUID, serviceData};

    err = bt_le_adv_update_data(data, ARRAY_SIZE(data), NULL, 0); // has 3 fields
    if (err) {
      log_errorf("Advertising failed to start (err %d)\r\n", err);
      return;
    }
  } else {
    // currently transmitting gaen data, switch to pancast data
    int err = bt_le_adv_update_data(payload.bt_data, ARRAY_SIZE(payload.bt_data),
        adv_res, ARRAY_SIZE(adv_res));
    if (err) {
      log_errorf("Advertising failed to start (err %d)\r\n", err);
      return;
    }
  }
  return;
}
#endif

static void _beacon_init_()
{
  epoch = 0;
  cycles = 0;

  beacon_time = config.t_init;
  report_time = beacon_time;

#ifdef MODE__STAT
  stat_epochs = 0;
  stat_start = beacon_time;
  beacon_storage_read_stat(&storage, &stats, sizeof(beacon_stats_t));
  if (!stats.storage_checksum) {
    log_infof("%s", "Existing Statistics Found\r\n");
    _beacon_stats_();
  } else {
    beacon_stats_init();
  }
#endif
  _beacon_info_();

// Timer Start
#ifdef BEACON_PLATFORM__ZEPHYR
  k_timer_init(&kernel_time_lp, NULL, NULL);
  k_timer_init(&kernel_time_hp, NULL, NULL);
  k_timer_init(&kernel_time_alternater, NULL, NULL);
#define DUR_LP K_MSEC(BEACON_TIMER_RESOLUTION)
#define DUR_HP K_MSEC(1)
#define DUR_ALT K_MSEC(alt_time_in_ms)
  k_timer_start(&kernel_time_lp, DUR_LP, DUR_LP);
  k_timer_start(&kernel_time_hp, DUR_HP, DUR_HP);
  k_timer_start(&kernel_time_alternater, DUR_ALT, DUR_ALT);
#undef DUR_HP
#undef DUR_LP
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
#ifndef MODE__DISABLE_LEGACY_DATA
#ifdef BEACON_PLATFORM__GECKO
  log_debugf("%s", "Setting legacy adv data...\r\n");
  print_bytes(payload.en_data.bytes, MAX_BROADCAST_SIZE, "adv_data");
  sl_status_t sc = sl_bt_advertiser_set_data(legacy_set_handle,
                                             0, 31,
                                             payload.en_data.bytes);
  if (sc != 0) {
    log_errorf("Error, sc: 0x%lx\r\n", sc);
    return -1;
  }
  log_debugf("%s", "Success!\r\n");
#else
#endif
#endif
  return 0;
}

static int _beacon_advertise_()
{
  int err = 0;
#ifdef BEACON_PLATFORM__ZEPHYR
  err = bt_le_adv_start(
      BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_USE_IDENTITY |
        BT_LE_ADV_OPT_DISABLE_CHAN_38 |
        BT_LE_ADV_OPT_DISABLE_CHAN_39, // use random identity address,
        BEACON_ADV_MIN_INTERVAL,
        BEACON_ADV_MAX_INTERVAL,
        NULL), // undirected advertising
      payload.bt_data, ARRAY_SIZE(payload.bt_data),
      adv_res, ARRAY_SIZE(adv_res));

  if (err) {
    log_errorf("Advertising failed to start (err %d)\r\n", err);
    return -1;
  } else {
    // obtain and report advertisement address
    char addr_s[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr = {0};
    size_t count = 1;

    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

    log_debugf("advertising started with address %s\r\n", addr_s);
  }
#else
  sl_status_t sc;
  log_debugf("%s", "Creating legacy advertising set\r\n");
  sc = sl_bt_advertiser_create_set(&legacy_set_handle);
  if (sc != 0) {
    log_errorf("Error, sc: 0x%lx\r\n", sc);
    return -1;
  }

  // Set Power Level
  int16_t set_power;
  sc = sl_bt_advertiser_set_tx_power(legacy_set_handle, LEGACY_TX_POWER, &set_power);

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
    return -1;
  }
  // Start legacy advertising
  sc = sl_bt_advertiser_start(legacy_set_handle, advertiser_user_data,
      advertiser_non_connectable);
  if (sc != 0) {
    log_errorf("Error starting advertising, sc: 0x%lx\r\n", sc);
    return -1;
  }
  err = _set_adv_data_();
  if (err) {
    log_errorf("Set adv data, err: %d\r\n", err);
  }
#endif
  return err;
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

void _beacon_update_()
{
  _beacon_epoch_();
  _beacon_encode_();
  _set_adv_data_();
}

int beacon_clock_increment(beacon_timer_t time)
{
  beacon_time += time;
  log_debugf("beacon timer: %u\r\n", beacon_time);
  _beacon_update_();
  _beacon_pause_();
  return 0;
}

#ifdef BEACON_PLATFORM__ZEPHYR
int beacon_loop()
{
  _beacon_update_();

  uint32_t lp_timer_status = 0, hp_timer_status = 0, alt_timer_status = 0;

  int err = 0;
  while (!err) {

#ifdef BEACON_GAEN_ENABLED
    alt_timer_status += k_timer_status_get(&kernel_time_alternater);
    if (alt_timer_status % (num_ms_in_min / alt_time_in_ms) == 0) {
#endif
      // get most updated time
      // Low-precision timer is synced, so accumulate status here
      lp_timer_status += k_timer_status_get(&kernel_time_lp);

      // update beacon clock using kernel. The addition is the number of
      // periods elapsed in the internal timer
      beacon_clock_increment(lp_timer_status);

      // high-precision collects the raw number of expirations
      hp_timer_status = k_timer_status_get(&kernel_time_hp);
#ifdef BEACON_GAEN_ENABLED
    }

    _alternate_advertisement_content_(alt_timer_status);
    alt_timer_status += k_timer_status_sync(&kernel_time_alternater);

#else

    // // Wait for a clock update, this blocks until the internal timer
    // // period expires, indicating that at least one unit of relevant beacon
    // // time has elapsed. timer status is reset here
    lp_timer_status = k_timer_status_sync(&kernel_time_lp);

#endif
  }
  return err;
}
#endif

// Primary broadcasting routine
// Non-zero argument indicates an error setting up the procedure for BT advertising
#ifdef BEACON_PLATFORM__ZEPHYR
void _beacon_broadcast_(int err)
{
  // check initialization
  if (err) {
    log_errorf("Bluetooth init failed (err %d)\r\n", err);
    return;
  }

  log_infof("%s", "Bluetooth initialized - starting broadcast\r\n");
#else
void beacon_broadcast()
{
  log_debugf("%s", "Starting broadcast\r\n");
  int err = 0;
#endif

  _beacon_load_(), _beacon_init_();

#ifdef BEACON_PLATFORM__ZEPHYR
  int8_t tx_power = 4; // has a range of [-40, 4]
  _beacon_update_();
  err = _beacon_advertise_();
  if (err) {
    log_errorf("Broadcasting failed (err %d)\r\n", err);
    return;
  }
  set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, tx_power);
  beacon_loop();
#else
  _beacon_update_();
  err = _beacon_advertise_();
  if (err != 0) {
      log_errorf("%s", "Error starting legacy adv\r\n");
  }
#endif
}

beacon_storage *get_beacon_storage()
{
  return &storage;
}

#undef MODE__STAT
#undef APPL_VERSION
#undef MODE__TEST_CONFIG
