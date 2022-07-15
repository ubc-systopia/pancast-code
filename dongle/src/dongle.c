//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "dongle.h"

#include <string.h>

#include "app_assert.h"
#include "app_log.h"

#include "stats.h"
#include "storage.h"
#include "test.h"
#include "upload.h"
#include "encounter.h"
#include "telemetry.h"
#include "download.h"
#include "led.h"
#include "button.h"
#include "nvm3_lib.h"

#include "common/src/util/log.h"
#include "common/src/constants.h"
#include "common/src/test.h"
#include "common/src/util/util.h"
#include "crypto.h"

// in-memory config
dongle_config_t config;

sl_sleeptimer_timer_handle_t led_timer;
int led_state = 0;
dongle_epoch_counter_t epoch; // current epoch
dongle_timer_t dongle_time; // main dongle timer
enctr_list_t *enctr_list;
enctr_bitmap_t enctr_bmap;
dongle_stats_t *stats;
size_t cur_id_idx;
extern download_t download;

// 5. Statistics and Telemetry
// Global high-precision timer, in milliseconds
float dongle_hp_timer = 0.0;

static void dongle_config_init(dongle_config_t *cfg, enctr_list_t **enctr_list_p,
    enctr_bitmap_t *enctr_bmap, dongle_stats_t **stats_p)
{
  if (!cfg)
    return;

  cfg->backend_pk = malloc(PK_MAX_SIZE);
  memset(cfg->backend_pk, 0, PK_MAX_SIZE);

  cfg->dongle_sk = malloc(SK_MAX_SIZE);
  memset(cfg->dongle_sk, 0, SK_MAX_SIZE);

  if (enctr_list_p) {
    enctr_list_t *enctr_list = malloc(DONGLE_MAX_BC_TRACKED *
        sizeof(enctr_list_t));
    memset(enctr_list, 0, DONGLE_MAX_BC_TRACKED * sizeof(enctr_list_t));
    *enctr_list_p = enctr_list;
  }

  if (enctr_bmap) {
    dongle_init_bitmap(enctr_bmap);
  }

  if (stats_p) {
    dongle_stats_t *tmp_stats = malloc(sizeof(dongle_stats_t));
    memset(tmp_stats, 0, sizeof(dongle_stats_t));
    *stats_p = tmp_stats;
  }
}

void dongle_cleanup_config(dongle_config_t *cfg)
{
  if (!cfg)
    return;

  if (cfg->backend_pk)
    free(cfg->backend_pk);

  if (cfg->dongle_sk)
    free(cfg->dongle_sk);
}

/*
 * invoke after kernel has initialized and bluetooth device is booted.
 * load config from flash and nvm3, init or reset variables and init scan
 */
void dongle_init()
{
  dongle_config_t sto_cfg;
  dongle_stats_t sto_stats;

  // init flash storage
  dongle_storage_init();

  // reset downloaded space
  dongle_download_init();

  // init configs
  dongle_config_init(&sto_cfg, NULL, NULL, NULL);
  dongle_config_init(&config, &enctr_list, &enctr_bmap, &stats);

  // load config
  dongle_storage_load_config(&sto_cfg);
  nvm3_load_config(&config);
  nvm3_load_enctr_bmap(&enctr_bmap);
  dongle_print_bitmap_all(&enctr_bmap);
//  nvm3_save_enctr_bmap(&enctr_bmap);
//  nvm3_load_enctr_bmap(&enctr_bmap);

  config.id = sto_cfg.id;
  config.t_init = sto_cfg.t_init;
  config.backend_pk_size = sto_cfg.backend_pk_size;
  config.dongle_sk_size = sto_cfg.dongle_sk_size;
  memcpy(config.backend_pk, sto_cfg.backend_pk, sto_cfg.backend_pk_size);
  memcpy(config.dongle_sk, sto_cfg.dongle_sk, sto_cfg.dongle_sk_size);

  // load stats
  dongle_storage_read_stat(&sto_stats, sizeof(dongle_stats_t));
  nvm3_load_stat(stats);

  log_expf("=== [C, H, T] nvm: %u %u %u sto: %u %u %u csum: 0x%0x ===\r\n",
      config.t_cur, config.en_head, config.en_tail, sto_cfg.t_cur,
      sto_cfg.en_head, sto_cfg.en_tail, sto_stats.storage_checksum);

  // reset nvm3 state and stats if checksum on dongle flash does not match
  // we reset checksum when flashing image through config scripts
  if (sto_stats.storage_checksum != DONGLE_STORAGE_STAT_CHKSUM) {
    sto_stats.storage_checksum = DONGLE_STORAGE_STAT_CHKSUM;
    dongle_storage_save_stat(&sto_cfg, &sto_stats, sizeof(dongle_stats_t));

//    config.t_cur = config.en_head = config.en_tail = 0;
    config.t_cur = sto_cfg.t_cur;
    config.en_head = sto_cfg.en_head;
    config.en_tail = sto_cfg.en_tail;
    nvm3_save_config(&config);

    // intent to reset everything
    if (sto_cfg.en_head == 0 && sto_cfg.en_tail == 0) {
      dongle_reset_bitmap_all(&enctr_bmap);
      nvm3_save_enctr_bmap(&enctr_bmap);
    }

    dongle_stats_reset(stats);
    nvm3_save_stat(stats);
  }

//  dongle_load();

  // set dongle time to current saved time
  dongle_time = config.t_cur > config.t_init ? config.t_cur : config.t_init;
  cur_id_idx = 0;
  epoch = 0;

  // print basic device info and state
  dongle_info();
  dongle_encounter_report(&config, stats);
  dongle_stats(stats);

  // update last report time and save to nvm3
  stats->stat_ints.last_report_time = dongle_time;
  nvm3_save_stat(stats);

#if TEST_DONGLE
  // test
  dongle_test_enctr_storage();

  config.en_tail = 0;
  config.en_head = MAX_LOG_COUNT-1;
#endif

  log_expf("==== UPLOAD LOG START [%u:%u] ===\r\n",
      config.en_tail, config.en_head);
  dongle_storage_load_encounter(0, MAX_LOG_COUNT,
      dongle_print_encounter, 0);
//  dongle_storage_load_encounter(config.en_tail,
//      num_encounters_current(config.en_head, config.en_tail),
//      dongle_print_encounter, 0);
  log_expf("%s", "==== UPLOAD LOG END ====\r\n");

  //===========

//  // reset stats
//  dongle_stats_init();

  // set up LED
  configure_blinky();
  configure_button();

#if DONGLE_CRYPTO
  // run crypto benchmark
  run_mbedtls_benchmark();
  run_psa_benchmark();
#endif

  dongle_cleanup_config(&sto_cfg);

  // initialize periodic advertisement scanning
  dongle_init_scan();
}

/* START
   Start scan
 */
void dongle_start_scan()
{
  sl_status_t sc = sl_bt_scanner_start(SCAN_PHY, SCAN_DISCOVER_MODE);
  log_infof("=== Starting Dongle: 0x%0x... ===\r\n", sc);
  if (sc != SL_STATUS_OK) {
    log_errorf("error starting scan, sc: 0x%x\r\n", sc);
  }
}

void dongle_stop_scan()
{
  sl_status_t sc = sl_bt_scanner_stop();
  log_infof("=== Stopping Dongle: 0x%0x... ===\r\n", sc);
  if (sc != SL_STATUS_OK) {
    log_errorf("error stopping scan, sc: 0x%x\r\n", sc);
  }
}

void dongle_init_scan()
{
  sl_status_t sc = 0;

  sc = sl_bt_scanner_set_timing(SCAN_PHY, SCAN_INTERVAL, SCAN_WINDOW);
  if (sc != SL_STATUS_OK) {
    log_errorf("error setting scanner timing, sc: 0x%x\r\n", sc);
  }

  sc = sl_bt_scanner_set_mode(SCAN_PHY, SCAN_MODE);
  if (sc != SL_STATUS_OK) {
    log_errorf("error setting scanner mode, sc: 0x%x\r\n", sc);
  }

  sc = sl_bt_sync_set_parameters(SYNC_SKIP, SYNC_TIMEOUT, SYNC_FLAGS);
  if (sc != SL_STATUS_OK) {
    log_errorf("error setting sync params, sc: 0x%x\r\n", sc);
  }
}

#if 0
/*
 * Load state and configuration from flash
 */
void dongle_load()
{
  dongle_config_init(&config);
  dongle_storage_init();
  // Config load is required to properly set up the memory maps
  dongle_storage_load_config(&config);
  enctr_entry_counter_t sto_en_head = config.en_head;
  enctr_entry_counter_t sto_en_tail = config.en_tail;
  dongle_timer_t sto_t_cur = config.t_cur;

  nvm3_load_config(&config);

#if 0
  if (sto_en_head == 0)
    config.en_head = 0;

  if (sto_en_tail == 0)
    config.en_tail = 0;

  if (sto_t_cur == 0)
    config.t_cur = 0;
#endif

  // if device re-flashed, all these fields would be zero, reset in the NVM too
  if (sto_en_head == 0 || sto_en_tail == 0 || sto_t_cur == 0)
    nvm3_save_clock_cursor(&config);
}
#endif

void dongle_clock_increment()
{
  dongle_time++;
  dongle_on_clock_update();
}

void dongle_hp_timer_add(uint32_t ticks)
{
  double ms = ((double) ticks * PREC_TIMER_TICK_MS);
  stats->stat_ints.total_periodic_data_time += (ms / 1000.0);
  if (download.is_active) {
    download.time += ms;
  }
  dongle_hp_timer += ms;
}

// UPDATE
// For callback-based timers. This is called with the mutex
// locked whenever the application clock obtains a new value.
void dongle_on_clock_update()
{
  // update epoch
  dongle_epoch_counter_t new_epoch = epoch_i(dongle_time, config.t_init);
  if (new_epoch != epoch) {
    epoch = new_epoch;
  }
  // update dongle time in config and save to flash
  config.t_cur = dongle_time;
//  dongle_storage_save_config(&config);
  nvm3_save_clock_cursor(&config);
  dongle_save_encounters();
  dongle_report();
}

/* Log system counters */
void dongle_log_counters()
{
  uint16_t    tx_packets;
  uint16_t    rx_packets;
  uint16_t    crc_errors;
  uint16_t    failures;

  sl_bt_system_get_counters(1, &tx_packets, &rx_packets, &crc_errors, &failures);

  stats->stat_ints.total_hw_rx += (uint32_t) rx_packets;
  stats->stat_ints.total_hw_crc_fail += (uint32_t)crc_errors;

//  log_debugf("tx_packets: %lu, rx_packets: %lu, crc_errors: %lu, failures: %lu\r\n",
//      tx_packets, rx_packets, crc_errors, failures);
}

void dongle_reset_counters()
{
  sl_bt_system_get_counters(1, 0, 0, 0, 0);
}

static int
decode_payload(uint8_t *data)
{
  data[0] = data[1];
  data[1] = data[MAX_BROADCAST_SIZE - 1];
  return 0;
}

// matches the schema defined by encode
static int decode_encounter(encounter_broadcast_t *dat,
                            encounter_broadcast_raw_t *raw)
{
  uint8_t *src = (uint8_t *)raw;
  size_t pos = 0;
#define link(dst, type)          \
  (dst = (type *)(src + pos)); \
  pos += sizeof(type)

  link(dat->t, beacon_timer_t);
  link(dat->b, beacon_id_t);
  link(dat->loc, beacon_location_id_t);
  link(dat->eph, beacon_eph_id_t);
#undef link
  return 0;
}

/* Same functionality as _dongle_encounter_ but takes duration into account
 */
static void dongle_save_encounter(mem_encounter_entry_t *enc, size_t i)
{
#if 0
  hexdumpen(enc->eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "log enc",
    enc->beacon_id, (uint32_t) enc->location_id, (uint16_t) i,
    (uint32_t) enc->beacon_time_start, enc->beacon_time_int,
    (uint32_t) enc->dongle_time_start, enc->dongle_time_int,
    (int8_t) enc->rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));
#endif

  dongle_encounter_entry_t de;
//  memset(&de, 0, sizeof(dongle_encounter_entry_t));
  de.location_id = enc->location_id;
  de.beacon_id = enc->beacon_id;
  de.beacon_time_start = enc->beacon_time_start;
  de.dongle_time_start = enc->dongle_time_start;
  de.beacon_time_int = enc->beacon_time_int;
  de.dongle_time_int = enc->dongle_time_int;
  de.rssi = (int8_t) enc->rssi;
  memcpy(&de.eph_id, &enc->eph_id, sizeof(beacon_eph_id_t));
  dongle_storage_log_encounter(&config, &dongle_time, &de);
  memset(&enctr_list[i], 0, sizeof(enctr_list_t));
}

static void dongle_track(encounter_broadcast_t *enc, int8_t rssi)
{
  // Check the broadcast UUID
  beacon_id_t service_id = (*(enc->b) & BEACON_SERVICE_ID_MASK) >> 16;
  if (service_id != BROADCAST_SERVICE_ID) {
    return;
  }

#if MODE__STAT
  stat_add(rssi, stats->stat_grp.scan_rssi);
#endif

  // determine which tracked id, if any, is a match
  int found = 0, i;
  for (i = 0; i < (int) DONGLE_MAX_BC_TRACKED; i++) {
    if (!compare_eph_id(enc->eph, &enctr_list[i].e.eph_id)) {
      found = 1;
      break;
    }
  }

  if (found == 0) {
    // if no match was found, start tracking the new id, replacing the oldest
    // one currently tracked
    i = cur_id_idx;
//    print_bytes(enc->eph->bytes, BEACON_EPH_ID_HASH_LEN, "new ID");
    cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;

    // copy eph id bytes into tracking array
    memcpy(&enctr_list[i].e.eph_id, enc->eph->bytes, BEACON_EPH_ID_HASH_LEN);
    enctr_list[i].e.beacon_id = *(enc->b);
    memcpy(&(enctr_list[i].e.location_id), enc->loc, sizeof(beacon_location_id_t));
//    enctr_list[i].e.location_id = *(enc->loc);
    enctr_list[i].e.beacon_time_start = *(enc->t);
    enctr_list[i].e.dongle_time_start = dongle_time;
    enctr_list[i].e.dongle_time_int = 0;
    /*
     * conservatively assume that the first instance of an encounter
     * is observed at the beginning of the timer interval.
     */
    enctr_list[i].e.beacon_time_int = 1;
    enctr_list[i].e.rssi = rssi;

#if 1
    beacon_eph_id_t *id = &enctr_list[i].e.eph_id;
    hexdumpen(id, BEACON_EPH_ID_HASH_LEN, "new enc",
      enctr_list[i].e.beacon_id,
      (uint32_t) enctr_list[i].e.location_id, (uint16_t) i,
      (uint32_t) enctr_list[i].e.beacon_time_start,
      enctr_list[i].e.beacon_time_int,
      (uint32_t) enctr_list[i].e.dongle_time_start,
      enctr_list[i].e.dongle_time_int,
      (int8_t) enctr_list[i].e.rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));
#endif

#if MODE__STAT
    stat_add(rssi, stats->stat_grp.enctr_rssi);
#endif

    return;
  }

  uint8_t dongle_dur = (uint8_t) (dongle_time - enctr_list[i].e.dongle_time_start);
  uint8_t beacon_dur = (uint8_t) (*enc->t - enctr_list[i].e.beacon_time_start + 1);

  enctr_list[i].e.dongle_time_int = dongle_dur;
  enctr_list[i].e.beacon_time_int = beacon_dur;
  enctr_list[i].e.rssi = ((enctr_list[i].e.rssi*enctr_list[i].n) + rssi) /
    (enctr_list[i].n+1);
  enctr_list[i].n += 1;

  return;
}

void dongle_save_encounters()
{
  for (int i = 0; i < DONGLE_MAX_BC_TRACKED; i++) {
//    dongle_timer_t end_time = enctr_list[i].e.dongle_time_start
//      + enctr_list[i].e.dongle_time_int;

#if 0
   beacon_eph_id_t *id = &enctr_list[i].e.eph_id;
   hexdumpen(id, BEACON_EPH_ID_HASH_LEN, "chk enc",
     enctr_list[i].e.beacon_id,
     (uint32_t) enctr_list[i].e.location_id, (uint16_t) i,
     (uint32_t) enctr_list[i].e.beacon_time_start,
     enctr_list[i].e.beacon_time_int,
     (uint32_t) enctr_list[i].e.dongle_time_start,
     enctr_list[i].e.dongle_time_int,
     (int8_t) enctr_list[i].e.rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));
#endif

    /*
     * if one epoch has passed and we haven't seen the eph id again,
     * then count this as the end of the duration and log the encounter
     */
    if (enctr_list[i].e.dongle_time_start != 0 &&
        dongle_time - enctr_list[i].e.dongle_time_start > LOG_MIN_WAIT) {
      dongle_save_encounter(&enctr_list[i].e, i);
    }
  }
}

void dongle_on_scan_report(bd_addr *addr __attribute__((unused)), int8_t rssi,
    uint8_t *data, uint8_t data_len)
{
  // Filter mis-sized packets
  if (data_len != ENCOUNTER_BROADCAST_SIZE + 1) {
    // TODO: should check for a periodic packet identifier
    return;
  }

  decode_payload(data);
  encounter_broadcast_t en;
  decode_encounter(&en, (encounter_broadcast_raw_t *)data);
  dongle_track(&en, rssi);
}

// used as callback for dongle_load_encounter
int dongle_print_encounter(enctr_entry_counter_t i,
    dongle_encounter_entry_t *entry,
    uint32_t num_buckets __attribute__((unused)))
{
  beacon_eph_id_t *id = &entry->eph_id;
  hexdumpen(id, BEACON_EPH_ID_HASH_LEN, "read", entry->beacon_id,
    (uint32_t) entry->location_id, (uint16_t) i,
    (uint32_t) entry->beacon_time_start, entry->beacon_time_int,
    (uint32_t) entry->dongle_time_start, entry->dongle_time_int,
    (int8_t) entry->rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));

  return 1;
}

void dongle_info()
{
  log_expf("%s", "=== Dongle Info: ===\r\n");
  log_expf("   Dongle ID:                        0x%x\r\n", config.id);
  log_expf("   Clock init, cur, dongle_time:     %lu, %lu, %lu\r\n",
    config.t_init, config.t_cur, dongle_time);
  log_expf("   Timer frequency:                  %u Hz\r\n",
    sl_sleeptimer_get_timer_frequency());
  log_expf("   Backend public key size:          %lu bytes\r\n",
    config.backend_pk_size);
  log_expf("   Secret key size:                  %lu bytes\r\n",
    config.dongle_sk_size);
  log_expf("   Timer resolution:                 %u ms\r\n",
    DONGLE_TIMER_RESOLUTION);
  log_expf("   Epoch length:                     %.1f min\r\n",
    ((float) BEACON_EPOCH_LENGTH * DONGLE_TIMER_RESOLUTION)/60000);
  log_expf("   Report interval:                  %u min\r\n",
    (DONGLE_REPORT_INTERVAL * DONGLE_TIMER_RESOLUTION)/60000);
  log_expf("   Legacy adv scan [PHY, mode]:      %d, %d\r\n",
    SCAN_PHY, SCAN_MODE);
  log_expf("   Legacy adv scan intvl, wnd, cycle:%.02f ms, %.02f ms, %u ms\r\n",
    SCAN_INTERVAL*0.625, SCAN_WINDOW*0.625,
    SCAN_CYCLE_TIME * DONGLE_TIMER_RESOLUTION);
  log_expf("   Periodic adv sync skip, flags:    %d, %d\r\n",
    SYNC_SKIP, SYNC_FLAGS);
  log_expf("   Periodic adv sync timeout:        %u ms\r\n",
    SYNC_TIMEOUT*10);
  log_expf("   Periodic adv sync fail:           %.1f min\r\n",
    ((float) PERIODIC_SYNC_FAIL_LATENCY * DONGLE_TIMER_RESOLUTION)/60000);
  log_expf("   Periodic adv download lat:        %u min\r\n",
    (DOWNLOAD_LATENCY_THRESHOLD * DONGLE_TIMER_RESOLUTION)/60000);
  log_expf("   Periodic adv retry, new interval: %u min, %u min\r\n",
    (RETRY_DOWNLOAD_INTERVAL * DONGLE_TIMER_RESOLUTION)/60000,
    (NEW_DOWNLOAD_INTERVAL * DONGLE_TIMER_RESOLUTION)/60000);

  log_expf("   Flash page size, count, total:    %u B, %u, %u B\r\n",
    FLASH_DEVICE_PAGE_SIZE, FLASH_DEVICE_NUM_PAGES,
    (FLASH_DEVICE_NUM_PAGES * FLASH_DEVICE_PAGE_SIZE));
  log_expf("   Flash offset:                     0x%0x\r\n", FLASH_OFFSET);
  log_expf("   Log range, size:                  0x%0x-0x%0x, %u\r\n",
    ENCOUNTER_LOG_START, ENCOUNTER_LOG_END,
    (ENCOUNTER_LOG_END - ENCOUNTER_LOG_START));
  log_expf("   Encounter size (storage, mem):    %u, %u\r\n",
    sizeof(dongle_encounter_entry_t), sizeof(mem_encounter_entry_t));
  log_expf("   Max enctr entries:                %lu\r\n", MAX_LOG_COUNT);
  log_expf("   Log head, tail:                   %u, %u\r\n",
    config.en_head, config.en_tail);
  log_expf("   Config offset:                    0x%0x\r\n",
    DONGLE_CONFIG_OFFSET);
  log_expf("   OTP offset, stat offset:          0x%0x, 0x%0x\r\n",
    DONGLE_OTPSTORE_OFFSET, DONGLE_STATSTORE_OFFSET);
  log_expf("   Stat obj size, nvm3 obj size:     %u, %u\r\n",
    sizeof(dongle_stats_t), NVM3_DEFAULT_MAX_OBJECT_SIZE);
  log_expf("   NVM3 #bitmap keys, #logs/bitmap:  %u, %u\r\n",
      NUM_NVM3_BITMAP_KEYS, NUM_LOG_ENTRIES_PER_NVM3_BITMAP_KEY);
}

void dongle_report()
{
#if MODE__STAT
  // do report
  if (((int) (dongle_time - stats->stat_ints.last_report_time)) <
      (int) DONGLE_REPORT_INTERVAL)
    return;

  dongle_encounter_report(&config, stats);
  dongle_stats(stats);
  stats->stat_ints.last_report_time = dongle_time;
//  dongle_storage_save_stat(&config, stats, sizeof(dongle_stats_t));
  nvm3_save_stat(stats);

#endif
}
