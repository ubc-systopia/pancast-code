//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//
#include "settings.h"
#include "beacon.h"
#include "test.h"
#include <include/stats.h>
#include <include/constants.h>

#include <stdio.h>

#define APPL_VERSION "0.1.1"

#define MODE__STAT

#define LOG_LEVEL__INFO

#include <string.h>

#include <zephyr.h>
#include <stddef.h>

#include <sys/printk.h>
#include <sys/util.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include <drivers/flash.h>

#include "storage.h"

#include "constants.h"
#include "settings.h"
#include "test.h"
#include "led.h"

#include <include/util.h>
#include <include/log.h>

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
static struct k_timer kernel_time_lp;         // periodic timer for new ephid gen
static struct k_timer kernel_time_alternater; // alternate Pancast and GAEN packets
static struct k_timer led_timer;      // periodic LED blinking
uint32_t timer_freq = 0;
/*
 * ENTRY POINT
 */
void main(void)
{
  int err = 0;
  log_infof("=== Starting %s ===\r\n", CONFIG_BT_DEVICE_NAME);
  timer_freq = sys_clock_hw_cycles_per_sec();
  err = bt_enable(_beacon_broadcast_);
  log_debugf("beacon enable, ret: %d\r\n", err);
}

//
// Bluetooth
static bt_data_t adv_res[] = {
  { BT_DATA_NAME_COMPLETE,
  (sizeof(CONFIG_BT_DEVICE_NAME) - 1),
  CONFIG_BT_DEVICE_NAME,
  },
}; // Advertising response

#ifdef BEACON_GAEN_ENABLED
// fields for constructing packet
static uint8_t service_data_type = 0x16;
static bt_gaen_wrapper_t gaen_payload = {
  .flags = BT_DATA_BYTES(0x01, 0x1a),
  .serviceUUID = BT_DATA_BYTES(0x03, 0x6f, 0xfd)
}; // container for gaen broadcast data
static uint32_t num_ms_in_min = 60000;
#endif /* BEACON_GAEN_ENABLED */

#if MODE__NRF_BEACON_TEST_CONFIG

static pubkey_t TEST_BACKEND_PK = {
  {0xad, 0xf4, 0xca, 0x6c, 0xa6, 0xd9, 0x11, 0x22}
};

static beacon_sk_t TEST_BEACON_SK = {
  {0xcb, 0x43, 0xf7, 0x56, 0x16, 0x25, 0xb3, 0xd0,
   0xd0, 0xbe, 0xad, 0xf4, 0x55, 0x66, 0x77, 0x88}
};

#endif /* MODE__NRF_BEACON_TEST_CONFIG */

static bt_wrapper_t payload; // container for actual blutooth payload
//
// Statistics
#ifdef MODE__STAT
static beacon_timer_t stat_start;
static beacon_timer_t stat_cycles;
static beacon_timer_t stat_epochs;
#endif

//
// ROUTINES
//

static void beacon_load()
{
  memset(&config, 0, sizeof(beacon_config_t));
  beacon_storage_init(&storage);
  // Load data
  beacon_storage_load_config(&storage, &config);
#if MODE__NRF_BEACON_TEST_CONFIG
  config.beacon_id = TEST_BEACON_ID;
  config.beacon_location_id = TEST_BEACON_LOC_ID;
  config.t_init = TEST_BEACON_INIT_TIME;
  config.backend_pk_size = TEST_BACKEND_KEY_SIZE;
  memset(&config.backend_pk, 0, config.backend_pk_size);
  config.backend_pk = TEST_BACKEND_PK;
  config.beacon_sk_size = TEST_BEACON_SK_SIZE;
  memset(&config.beacon_sk, 0, config.beacon_sk_size);
  config.beacon_sk = TEST_BEACON_SK;
  storage.test_filter_size = TEST_FILTER_LEN;
  beacon_storage_save_config(&storage, &config);
#endif
}

void beacon_info()
{
  char addr_s[BT_ADDR_LE_STR_LEN];
  bt_addr_le_t addr = {0};
  size_t count = 1;

  bt_id_get(&addr, &count);
  bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

  log_infof("%s", "=== Beacon Info: ===\r\n");
  log_infof("  Platform:                 %s\r\n", "Zephyr OS");
  log_infof("  Board:                    %s\r\n", CONFIG_BOARD);
  log_infof("  Bluetooth device name:    %s\r\n", CONFIG_BT_DEVICE_NAME);
  log_infof("  Application version:      %s\r\n", APPL_VERSION);
  log_infof("  Beacon ID:                0x%x\r\n", config.beacon_id);
  log_infof("  Location ID:              0x%lx\r\n",
      (unsigned long) config.beacon_location_id);
  log_infof("  MAC addr:                 %s\r\n", addr_s);
  log_infof("  Initial clock:            %u\r\n", config.t_init);
  log_infof("  Timer frequency:          %u Hz\r\n", timer_freq);
  log_infof("  Backend public key size:  %u bytes\r\n",
      config.backend_pk_size);
  log_infof("  Secret key size:          %u bytes\r\n",
      config.beacon_sk_size);
  log_infof("  Timer resolution:         %u ms\r\n",
      BEACON_TIMER_RESOLUTION);
  log_infof("  Epoch length:             %u ms\r\n",
      BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);
  log_infof("  Report interval:          %u ms\r\n",
      BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
  log_infof("  Legacy adv interval:      %x-%x ms\r\n",
      BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL);
  log_infof("  Test Filter Length:       %u\r\n",
      storage.test_filter_size);
#ifdef MODE__STAT
  log_infof("%s", "  Statistics mode enabled\r\n");
#endif
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
}

static void beacon_stats()
{
  log_infof("[%u] last report time: %u, #cycles: %u, #epochs: %u chksum: %u\r\n",
      beacon_time, stats.start, stats.cycles, stats.epochs, stats.storage_checksum);
}
#endif

static void _beacon_report_()
{
  if (beacon_time - stat_start < BEACON_REPORT_INTERVAL)
    return;

#ifdef MODE__STAT
  beacon_stat_update();
  beacon_stats();
  beacon_storage_save_stat(&storage, &stats, sizeof(beacon_stats_t));
  beacon_stats_reset();
  stat_start = beacon_time;
  stat_cycles = 0;
  stat_epochs = 0;
#endif
}

/*
 * set transmission power (zephyr).
 * Ref: (https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/hci_pwr_ctrl/src/main.c)
 */
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
    log_errorf("set Tx power err: %d reason 0x%02x\n", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  log_debugf("actual Tx Power: %d\n", rp->selected_tx_power);

  net_buf_unref(rsp);
}

/*
 * Intermediary transformer to create a well-formed BT data type
 * for using the high-level APIs. Becomes obsolete once advertising
 * routine supports a full raw payload
 */
static void _form_payload_()
{
  const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
#define bt (payload.bt_data)
  uint8_t tmp;
  tmp = bt->data_len;
  bt->data_len = len;
#define en (payload.en_data)
  en.bytes[MAX_BROADCAST_SIZE - 1] = tmp;
#undef en

  static uint8_t of[MAX_BROADCAST_SIZE - 2];
  memcpy(of, ((uint8_t *)bt) + 2, MAX_BROADCAST_SIZE - 2);
  bt->data = (uint8_t *)&of;
#undef bt
}

/*
 * pack a raw byte payload by copying from the high-level type
 * order is important here so as to avoid unaligned access on the
 * receiver side
 */
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
  hexdumpen(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "eph ID",
      config.beacon_id, (uint32_t) config.beacon_location_id, beacon_time);
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
  // Little endian so these are the least significant
  memcpy(&beacon_eph_id, &d, BEACON_EPH_ID_HASH_LEN);
#undef complete
#undef add
#undef init
}

static void _beacon_epoch_()
{
  static beacon_epoch_counter_t old_epoch;
  old_epoch = epoch;
  epoch = epoch_i(beacon_time, config.t_init);
  if (!cycles || epoch != old_epoch) {
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

/*
 * unused, instead relying on the function that allows for
 * alternating between GAEN and PanCast ephids
 */
void _beacon_update_()
{
  _beacon_epoch_();
  _beacon_encode_();
  _set_adv_data_();
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

#ifdef BEACON_GAEN_ENABLED
/*
 * populates gaen_payload.service_data_internals
 * with procotol specific data fields
 */
static int _set_adv_data_gaen_()
{
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
}

static void _gaen_encode_(bt_data_t *data)
{
  bt_data_t flags = BT_DATA_BYTES(0x01, 0x1a);
  bt_data_t serviceUUID = BT_DATA_BYTES(0x03, 0x6f, 0xfd);
  bt_data_t serviceData = BT_DATA(service_data_type,
      gaen_payload.service_data_internals,
      ARRAY_SIZE(gaen_payload.service_data_internals));
  data[0] = flags;
  data[1] = serviceUUID;
  data[2] = serviceData;
  hexdumpen(gaen_payload.service_data_internals,
      ARRAY_SIZE(gaen_payload.service_data_internals), "gaenID",
      config.beacon_id, (uint64_t) (*(uint16_t *) serviceUUID.data), beacon_time);
}
#endif /* BEACON_GAEN_ENABLED */

/*
 * procedure to change the type of packet being transmitted
 */
void _alternate_advertisement_content_(int type)
{
#ifdef BEACON_GAEN_ENABLED
  if (type == 1) {
    // currently transmitting pancast data, switch to gaen data
    int err = _set_adv_data_gaen_();
    if (err) {
      log_errorf("%s", "Failed to obtain gaen advertisement data");
      return;
    }
    bt_data_t data[3];
    _gaen_encode_((bt_data_t *) &data);
//    bt_data_t data[3] = {flags, serviceUUID, serviceData};

    err = bt_le_adv_update_data(data, ARRAY_SIZE(data), NULL, 0); // has 3 fields
    if (err) {
      log_errorf("Advertising failed to start (err %d)\r\n", err);
      return;
    }
  } else {
#endif
#if 0
    adv_res[0] = { BT_DATA(BT_DATA_NAME_COMPLETE,
                         CONFIG_BT_DEVICE_NAME,
                         sizeof(CONFIG_BT_DEVICE_NAME) - 1) };
#endif
//    _beacon_update_();
    _beacon_epoch_();
    _beacon_encode_();
    // currently transmitting gaen data, switch to pancast data
    int err = bt_le_adv_update_data(payload.bt_data, ARRAY_SIZE(payload.bt_data),
        adv_res, ARRAY_SIZE(adv_res));
    if (err) {
      log_errorf("advertising failed to start (err %d)\r\n", err);
      return;
    }
#ifdef BEACON_GAEN_ENABLED
  }
#endif
  return;
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

int _set_adv_data_()
{
  int err = bt_le_adv_update_data(payload.bt_data, ARRAY_SIZE(payload.bt_data),
      adv_res, ARRAY_SIZE(adv_res));
  if (err) {
    log_errorf("advertising failed to start (err %d)\r\n", err);
    return -1;
  }
  return 0;
}

static int beacon_legacy_advertise()
{
  int err = 0;
  err = bt_le_adv_start(
      BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_USE_IDENTITY, // use random identity address,
        BEACON_ADV_MIN_INTERVAL,
        BEACON_ADV_MAX_INTERVAL,
        NULL), // undirected advertising
      payload.bt_data, ARRAY_SIZE(payload.bt_data),
      adv_res, ARRAY_SIZE(adv_res));

  if (err)
    log_errorf("adv start failed, err: %d\r\n", err);

  return err;
}

int beacon_clock_increment(beacon_timer_t time)
{
  beacon_time += time;
  log_debugf("beacon timer: %u\r\n", beacon_time);
//  _beacon_update_();
  _beacon_pause_();
  return 0;
}

void beacon_loop()
{
  uint32_t lp_timer_status = 0;
  _alternate_advertisement_content_(0);
  while ((lp_timer_status = k_timer_status_sync(&kernel_time_lp))) {
    beacon_clock_increment(lp_timer_status);
    _alternate_advertisement_content_(0);
  }
}

void beacon_gaen_pancast_loop()
{
  uint32_t lp_timer_status = 0, rem_time = 0, alt_timer_status = 0;
  int count = 0;
  lp_timer_status = k_timer_status_get(&kernel_time_lp);
  rem_time = k_timer_remaining_get(&kernel_time_lp);
  log_infof("beacon time: %u, expired: %u, time till next expiry: %u\r\n",
      beacon_time, lp_timer_status, rem_time);
  while (true) {
    while ((rem_time = k_timer_remaining_get(&kernel_time_lp)) > 0) {
      _alternate_advertisement_content_((count % 2));
      count++;
      lp_timer_status = k_timer_status_get(&kernel_time_lp);
      beacon_clock_increment(lp_timer_status);
      alt_timer_status = k_timer_status_sync(&kernel_time_alternater);
//      log_infof("beacon time: %u, expired: %u, time till next expiry: %u\r\n",
//          beacon_time, lp_timer_status, rem_time);
    }
  }
}

/*
 * Primary broadcasting routine
 */
void _beacon_broadcast_(int err)
{
  int8_t tx_power = MAX_TX_POWER;
  set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, tx_power);

  beacon_load();
  beacon_info();

  epoch = 0;
  cycles = 0;

  beacon_time = config.t_init;

  // Timer Start
  k_timer_init(&kernel_time_lp, NULL, NULL);
  k_timer_init(&kernel_time_alternater, NULL, NULL);
  k_timer_init(&led_timer, beacon_led_timeout_handler, NULL);

  k_timer_start(&kernel_time_lp, K_MSEC(BEACON_TIMER_RESOLUTION),
      K_MSEC(BEACON_TIMER_RESOLUTION));
  k_timer_start(&kernel_time_alternater, K_MSEC(PAYLOAD_ALTERNATE_TIMER),
      K_MSEC(PAYLOAD_ALTERNATE_TIMER));
  k_timer_start(&led_timer, K_MSEC(LED_TIMER), K_MSEC(LED_TIMER));

  beacon_stats_init();

  configure_blinky();

  err = beacon_legacy_advertise();
#ifdef BEACON_GAEN_ENABLED
  beacon_gaen_pancast_loop();
#else
  beacon_loop();
#endif
}

#undef MODE__STAT
#undef APPL_VERSION
