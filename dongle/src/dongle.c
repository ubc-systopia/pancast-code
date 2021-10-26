//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "./dongle.h"

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

#include "common/src/util/log.h"
#include "common/src/constants.h"
#include "common/src/test.h"
#include "common/src/util/util.h"

//
// GLOBAL MEMORY
// All of the variables used throughout the main program are allocated here

// 1. Mutual exclusion
// Access for both central updates and scan interrupts
// is given on a FIFO basis
// In the Gecko Platform, locks are no-ops until a firmware implementation
// can be made to work.
#define LOCK DONGLE_NO_OP;
#define UNLOCK DONGLE_NO_OP;

void dongle_lock()
{
  LOCK
}

void dongle_unlock(){
  UNLOCK
}

// 2. Config
dongle_config_t config;

// 3. Operation
dongle_storage storage;
dongle_storage *get_dongle_storage()
{
  return &storage;
}

dongle_epoch_counter_t epoch;
dongle_timer_t dongle_time; // main dongle timer
dongle_timer_t report_time;
beacon_eph_id_t cur_id[DONGLE_MAX_BC_TRACKED]; // currently observed ephemeral id
dongle_timer_t obs_time[DONGLE_MAX_BC_TRACKED]; // time of last new id observation
size_t cur_id_idx;
dongle_epoch_counter_t epoch; // current epoch
uint64_t signal_id;           // to identify scan records received, resets each epoch
extern download_t download;

// 3. Reporting
enctr_entry_counter_t non_report_entry_count;

// 5. Statistics and Telemetry
// Global high-precision timer, in milliseconds
float dongle_hp_timer = 0.0;
extern dongle_stats_t stats;
extern download_stats_t download_stats;

//
// ROUTINES
//

// MAIN
// Entrypoint of the application
// Initialize bluetooth, then call advertising and scan routines
// Assumes that kernel has initialized and bluetooth device is booted.
void dongle_start()
{
  log_infof("%s", "=== Starting Dongle... ===\r\n");
  log_infof("Config test: %d, periodic sync: %d\r\n", TEST_DONGLE, MODE__PERIODIC);

  if (access_advertise()) {
    return;
  }

  dongle_scan();
}

// SCAN
// Call init routine, begin advertising with fixed parameters,
// and enter the main loop.
void dongle_scan(void)
{

  dongle_init();

  // Scan Start
  int err = 0;

#if MODE__PERIODIC
  sl_status_t sc;
  // Set scanner timing
  log_debugf("Setting scanner timing\r\n");
  sc = sl_bt_scanner_set_timing(SCAN_PHY, SCAN_INTERVAL, SCAN_WINDOW);
  app_assert_status(sc);

  // Set scanner mode
  log_debugf("Setting scanner mode\r\n");
  sc = sl_bt_scanner_set_mode(SCAN_PHY, SCAN_MODE);
  app_assert_status(sc);

  // Set sync parameters
  log_debugf("Setting sync parameters\r\n");
  sc = sl_bt_sync_set_parameters(SYNC_SKIP, SYNC_TIMEOUT, SYNC_FLAGS);
  app_assert_status(sc);

  // Start scanning
  log_debugf("Starting scan\r\n");
  sc = sl_bt_scanner_start(SCAN_PHY, scanner_discover_observation);
  app_assert_status(sc);

  err = sc;
#else
  sl_bt_scanner_set_timing(gap_1m_phy, // Using 1M PHY - is this correct?
     DONGLE_SCAN_INTERVAL, DONGLE_SCAN_WINDOW);
  sl_bt_scanner_set_mode(gap_1m_phy, 0); // passive scan
  sl_bt_scanner_start(gap_1m_phy,
    sl_bt_scanner_discover_observation); // scan all devices
#endif

  if (err) {
    log_errorf("Scanning failed to start (err %d)\r\n", err);
    return;
  } else {
    log_debugf("%s", "Scanning successfully started\r\n");
  }
}

// INIT
// Call load routine, and set variables to their
// initial value. Also initialize timing structs
void dongle_init()
{
  dongle_load();

  dongle_time = config.t_init;
  report_time = dongle_time;
  cur_id_idx = 0;
  epoch = 0;
  non_report_entry_count = 0;
  signal_id = 0;

  dongle_stats_init(&storage);

  dongle_download_init();

  dongle_download_stats_init();

  log_infof("%s", "Dongle initialized\r\n");

  dongle_info();
  dongle_storage_info(&storage);

  log_telemf("%02x\r\n", TELEM_TYPE_RESTART);
}

// LOAD
// Load state and configuration into memory
void dongle_load()
{
  dongle_storage_init(&storage);
  // Config load is required to properly set up the memory maps
  dongle_storage_load_config(&storage, &config);
#ifdef MODE__TEST_CONFIG
  config.id = TEST_DONGLE_ID;
  config.t_init = TEST_DONGLE_INIT_TIME;
  config.backend_pk_size = TEST_BACKEND_KEY_SIZE;
  config.backend_pk = TEST_BACKEND_PK;
  config.dongle_sk_size = TEST_DONGLE_SK_SIZE;
  config.dongle_sk = TEST_DONGLE_SK;
  dongle_storage_save_config(&storage, &config);
  dongle_storage_save_otp(&storage, TEST_OTPS);
#endif
}

void dongle_clock_increment()
{
  dongle_lock();
  dongle_time++;
  log_debugf("Dongle clock: %lu\r\n", dongle_time);
  dongle_on_clock_update();
  dongle_unlock();
}

void dongle_hp_timer_add(uint32_t ticks)
{
  double ms = ((double)ticks * PREC_TIMER_TICK_MS);
  stats.total_periodic_data_time += (ms / 1000.0);
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
    log_debugf("EPOCH STARTED: %lu\r\n", new_epoch);
    epoch = new_epoch;
    signal_id = 0;
    // TODO: log time to flash
  }
  dongle_report();
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


static void _dongle_encounter_(encounter_broadcast_t *enc, size_t i)
{
  // when a valid encounter is detected
  // log the encounter
//    log_infof("Beacon Encounter (id=%lu, t_b=%lu, t_d=%lu)\r\n", *en.b, *en.t,
//               dongle_time);
  //log_infof("%s", "Encounter! ");
  //display_eph_id(enc->eph);
  hexdumpn(enc->eph->bytes, BEACON_EPH_ID_HASH_LEN, "eph ID");

  // Write to storage
  dongle_storage_log_encounter(&storage, enc->loc, enc->b, enc->t, &dongle_time,
                               enc->eph);
#if TEST_DONGLE
  dongle_test_encounter(enc);
#endif
  // reset the observation time
  obs_time[i] = dongle_time;
}

static uint64_t dongle_track(encounter_broadcast_t *enc, int8_t rssi, uint64_t signal_id)
{
  // Check the broadcast UUID
  beacon_id_t service_id = (*(enc->b) & BEACON_SERVICE_ID_MASK) >> 16;
  if (service_id != BROADCAST_SERVICE_ID) {
    log_telemf("%02x,%u,%u,%lu\r\n",
               TELEM_TYPE_BROADCAST_ID_MISMATCH,
               dongle_time, epoch,
               signal_id);
    return signal_id;
  }

#ifdef MODE__STAT
  stats.num_scan_results++;
  stat_add(rssi, stats.scan_rssi);
#endif

//    hexdumpn(enc->eph->bytes, BEACON_EPH_ID_HASH_LEN, "eph ID");

  // determine which tracked id, if any, is a match
  size_t i = DONGLE_MAX_BC_TRACKED;
  for (size_t j = 0; j < DONGLE_MAX_BC_TRACKED; j++) {
    if (!compare_eph_id(enc->eph, &cur_id[j])) {
      i = j;
    }
  }

  if (i == DONGLE_MAX_BC_TRACKED) {
    // if no match was found, start tracking the new id, replacing the oldest
    // one currently tracked
    i = cur_id_idx;
    log_debugf("new ephemeral id observed (beacon=%lu), tracking at index %d\r\n",
               enc->b, i);
    print_bytes(enc->eph->bytes, BEACON_EPH_ID_HASH_LEN, "eph_id");
    cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;
    memcpy(&cur_id[i], enc->eph->bytes, BEACON_EPH_ID_HASH_LEN);
    obs_time[i] = dongle_time;

#ifdef MODE__STAT
    stats.num_obs_ids++;
#endif
    log_telemf("%02x,%u,%u,%lu,%u,%u\r\n", TELEM_TYPE_BROADCAST_TRACK_NEW,
       dongle_time, epoch, signal_id, enc->b, enc->t);
  } else {
    // when a matching ephemeral id is observed
    log_telemf("%02x,%u,%u,%lu,%u,%u\r\n", TELEM_TYPE_BROADCAST_TRACK_MATCH,
       dongle_time, epoch, signal_id, enc->b, enc->t);
    // check conditions for a valid encounter
    dongle_timer_t dur = dongle_time - obs_time[i];
    if (dur >= DONGLE_ENCOUNTER_MIN_TIME) {
      _dongle_encounter_(enc, i);
#ifdef MODE__STAT
      stat_add(rssi, stats.encounter_rssi);
#endif
      log_telemf("%02x,%u,%u,%lu,%u,%u,%u\r\n", TELEM_TYPE_ENCOUNTER,
         dongle_time, epoch, signal_id, enc->b, enc->t, dur);
    }
  }
  return signal_id;
}

void dongle_log(bd_addr *addr, int8_t rssi, uint8_t *data, uint8_t data_len)
{
#define len (data_len)
#define add (addr->addr)
#define dat (data)
  // Filter mis-sized packets
  if (len != ENCOUNTER_BROADCAST_SIZE + 1) {
    // TODO: should check for a periodic packet identifier
    return;
  }
  LOCK
  log_telemf("%02x,%u,%u,%lu,%02x%02x%02x%02x%02x%02x,%d\r\n",
      TELEM_TYPE_SCAN_RESULT, dongle_time, epoch, signal_id,
      add[0], add[1], add[2], add[3], add[4], add[5], rssi);
  //print_bytes(dat, data_len, "scan-data pre-decode");
//    hexdumpn(dat, 31, "raw");
  decode_payload(dat);
  //print_bytes(dat, data_len, "scan-data decoded");
  encounter_broadcast_t en;
  decode_encounter(&en, (encounter_broadcast_raw_t *)dat);
  dongle_track(&en, rssi, signal_id);
  signal_id++;
  UNLOCK
#undef dat
#undef data
#undef add
}

void dongle_info()
{
  log_infof("%s", "=== Dongle Info: ===\r\n");
  log_infof("    Dongle ID:                    0x%lx\r\n", config.id);
  log_infof("    Initial clock:                %lu\r\n", config.t_init);
  log_infof("    Backend public key size:      %lu bytes\r\n",
      config.backend_pk_size);
  log_infof("    Secret key size:              %lu bytes\r\n",
      config.dongle_sk_size);
  log_infof("    Timer resolution:             %u ms\r\n",
      DONGLE_TIMER_RESOLUTION);
  log_infof("    Epoch length:                 %u ms\r\n",
      BEACON_EPOCH_LENGTH * DONGLE_TIMER_RESOLUTION);
  log_infof("    Report interval:              %u ms\r\n",
      DONGLE_REPORT_INTERVAL * DONGLE_TIMER_RESOLUTION);
  log_infof("    Legacy adv scan [PHY, mode]:  %d, %d\r\n", SCAN_PHY, SCAN_MODE);
  log_infof("    Legacy adv scan interval:     %u ms\r\n", SCAN_INTERVAL);
  log_infof("    Legacy adv scan window:       %u ms\r\n", SCAN_WINDOW);
  log_infof("    Periodic adv sync skip:       %d\r\n", SYNC_SKIP);
  log_infof("    Periodic adv sync flags:      %d\r\n", SYNC_FLAGS);
  log_infof("    Periodic adv sync timeout:    %u ms\r\n", SYNC_TIMEOUT);
}

void dongle_encounter_report()
{
  enctr_entry_counter_t num = dongle_storage_num_encounters_total(&storage);
  enctr_entry_counter_t cur = dongle_storage_num_encounters_current(&storage);

  log_infof("[%lu] last report time: %lu, "
      "#encounters [delta, total, stored]: %lu, %lu, %lu\r\n",
      dongle_time, report_time, (num - non_report_entry_count), num, cur);
#if 0
  // Large integers here are casted for formatting compatabilty. This may result in false
  // output for large values.
  log_infof("    Encounters logged since last report: %lu\r\n",
      (uint32_t) (num - non_report_entry_count));
  log_infof("    Total Encounters logged (All-time):  %lu\r\n", (uint32_t) num);

  log_infof("    Total Encounters logged (Stored):    %lu%s\r\n", (uint32_t) cur,
    cur == dongle_storage_max_log_count(&storage) ? " (MAX)" : "");
#endif
}

void dongle_report()
{
  // do report
  if (dongle_time - report_time < DONGLE_REPORT_INTERVAL)
    return;

  dongle_encounter_report();

#ifdef MODE__STAT
  dongle_stats(&storage);
  dongle_download_stats();
#endif

#if TEST_DONGLE
  dongle_test();
#endif

  non_report_entry_count = dongle_storage_num_encounters_total(&storage);
  report_time = dongle_time;
}

#undef alpha
#undef LOG_LEVEL__INFO
//#undef TEST_DONGLE
#undef MODE__STAT
