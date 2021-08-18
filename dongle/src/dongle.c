//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "./dongle.h"

#include <string.h>

#ifdef DONGLE_PLATFORM__ZEPHYR
#include <zephyr.h>
#include <sys/util.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>
#include <drivers/flash.h>
#else
#include "app_assert.h"
#include "app_log.h"
#endif

#include "storage.h"
#include "test.h"
#include "access.h"
#include "encounter.h"
#include "telemetry.h"
#include "cuckoofilter-gadget/cf-gadget.h"

#include "../../common/src/log.h"
#include "../../common/src/pancast.h"
#include "../../common/src/test.h"
#include "../../common/src/util.h"

//
// GLOBAL MEMORY
// All of the variables used throughout the main program are allocated here

// 1. Mutual exclusion
// Access for both central updates and scan interrupts
// is given on a FIFO basis
#ifdef DONGLE_PLATFORM__ZEPHYR
struct k_mutex dongle_mu;
#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);
#else
// In the Gecko Platform, locks are no-ops until a firmware implementation
// can be made to work.
#define LOCK DONGLE_NO_OP;
#define UNLOCK DONGLE_NO_OP;
#endif

void dongle_lock()
{
    LOCK
}

void dongle_unlock(){
    UNLOCK}

// 2. Config
dongle_config_t config;

// 3. Operation
dongle_storage storage;
dongle_storage *get_dongle_storage()
{
    return &storage;
}
dongle_epoch_counter_t epoch;
#ifdef DONGLE_PLATFORM__ZEPHYR
struct k_timer kernel_time;
#endif
dongle_timer_t dongle_time; // main dongle timer
dongle_timer_t report_time;
beacon_eph_id_t
    cur_id[DONGLE_MAX_BC_TRACKED]; // currently observed ephemeral id
dongle_timer_t
    obs_time[DONGLE_MAX_BC_TRACKED]; // time of last new id observation
size_t cur_id_idx;
dongle_epoch_counter_t epoch; // current epoch
uint64_t signal_id;           // to identify scan records received, resets each epoch

// 3. Reporting
enctr_entry_counter_t non_report_entry_count;

// 4. Testing
// Allocation is conditioned on the compile setting
#ifdef MODE__TEST
int test_errors = 0;
#define TEST_MAX_ENCOUNTERS 64
int test_encounters = 0;
int total_test_encounters = 0;
dongle_encounter_entry test_encounter_list[TEST_MAX_ENCOUNTERS];
#endif

// 5. Telemetry
// Global high-precision timer, in milliseconds
float dongle_hp_timer = 0.0;

#ifdef MODE__STAT

#include "../../common/src/stats.h"

typedef struct
{

    uint8_t storage_checksum; // zero for valid stat data

    uint32_t num_obs_ids;
    uint32_t num_scan_results;
    uint32_t num_periodic_data;
    uint32_t num_periodic_data_error;
    uint32_t total_periodic_data_size; // bytes
    double total_periodic_data_time;   // seconds

    stat_t scan_rssi;
    stat_t encounter_rssi;
    stat_t periodic_data_size;
    stat_t periodic_data_rssi;

    double periodic_data_avg_thrpt;
} stats_t;

stats_t stats;

void stat_compute_thrpt(stats_t *st)
{
    st->periodic_data_avg_thrpt =
        ((double)st->total_periodic_data_size * 0.008) /
        st->total_periodic_data_time;
}

#endif

#ifdef MODE__PERIODIC_FIXED_DATA

typedef struct
{
    int is_active;
    double time;
    uint32_t n_syncs_lost;
    uint32_t n_total_packets;
    uint32_t n_corrupt_packets;
    struct {
      // map of sequence number to packet count for that number
      // used to track completion of the download
      uint32_t counts[ MAX_NUM_PACKETS_PER_FILTER];

      // number of unique packets seen
      int num_distinct;

      // number of bytes received
      uint32_t received;

      uint32_t chunk_num; // the current chunk being downloaded
      uint32_t chunk_len; // length of current filter being downloaded

      // actual received payload
      uint8_t buf[MAX_FILTER_SIZE];

    } packet_buffer;
} download_t;

typedef struct
{
//  stat_t packets;
//  stat_t duplicates;
//  stat_t duplicate_rate;
  stat_t n_bytes;
  stat_t syncs_lost;
} download_stats_t;

typedef int download_fail_reason;

typedef struct
{
  download_stats_t download_stats;
  stat_t periodic_data_avg_payload_lat; // download time
} complete_download_stats_t;

struct
{
    int payloads_started;
    int payloads_complete;
    int payloads_failed;
    download_t download;
    download_stats_t all_download_stats;
    download_stats_t failed_download_stats;
    complete_download_stats_t complete_download_stats;
    cf_t cf;
} lat_test;
#endif

//
// ROUTINES
//

// MAIN
// Entrypoint of the application
// Initialize bluetooth, then call advertising and scan routines
#ifdef DONGLE_PLATFORM__ZEPHYR
void main(void)
#else
// a.k.a DONGLE START
// Assumes that kernel has initialized and bluetooth device is booted.
void dongle_start()
#endif
{
    log_info("\r\n");
    log_info("Starting Dongle...\r\n");

#ifdef MODE__TEST
    log_info("Test mode enabled\r\n");
#endif
#ifdef MODE__STAT
    log_info("Statistics enabled\r\n");
#endif
#ifdef MODE__PERIODIC
    log_info("Periodic synchronization enabled\r\n");
#endif
#ifdef DONGLE_PLATFORM__ZEPHYR
    int err;

    err = bt_enable(NULL);
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\r\n", err);
        return;
    }

    log_info("Bluetooth initialized\r\n");
#endif

    if (access_advertise())
    {
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
#ifdef DONGLE_PLATFORM__ZEPHYR
    int err = err = bt_le_scan_start(
        BT_LE_SCAN_PARAM(
            BT_LE_SCAN_TYPE_PASSIVE, // passive scan
            BT_LE_SCAN_OPT_NONE,     // no options; in particular, allow duplicates
            DONGLE_SCAN_INTERVAL,    // interval
            DONGLE_SCAN_WINDOW       // window
            ),
        dongle_log);
#else
    int err = 0;
#ifdef MODE__PERIODIC
    sl_status_t sc;
    // Set scanner timing
    app_log_info("Setting scanner timing\r\n");
    sc = sl_bt_scanner_set_timing(SCAN_PHY, SCAN_INTERVAL, SCAN_WINDOW);
    app_assert_status(sc);

    // Set scanner mode
    app_log_info("Setting scanner mode\r\n");
    sc = sl_bt_scanner_set_mode(SCAN_PHY, SCAN_MODE);
    app_assert_status(sc);

    // Set sync parameters
    app_log_info("Setting sync parameters\r\n");
    sc = sl_bt_sync_set_parameters(SYNC_SKIP, SYNC_TIMEOUT, SYNC_FLAGS);
    app_assert_status(sc);

    // Start scanning
    app_log_info("Starting scan\r\n");
    sc = sl_bt_scanner_start(SCAN_PHY, scanner_discover_observation);
    app_assert_status(sc);

    err = sc;
#else
    sl_bt_scanner_set_timing(gap_1m_phy, // Using 1M PHY - is this correct?
                             DONGLE_SCAN_INTERVAL,
                             DONGLE_SCAN_WINDOW);
    sl_bt_scanner_set_mode(gap_1m_phy, 0); // passive scan
    sl_bt_scanner_start(gap_1m_phy,
                        sl_bt_scanner_discover_observation); // scan all devices
#endif
#endif
    if (err)
    {
        log_errorf("Scanning failed to start (err %d)\r\n", err);
        return;
    }
    else
    {
        log_debug("Scanning successfully started\r\n");
        dongle_loop();
    }
}

// INIT
// Call load routine, and set variables to their
// initial value. Also initialize timing structs
#ifdef MODE__STAT
void dongle_stats_init()
{
    memset(&stats, 0, sizeof(stats_t));
}
#ifdef MODE__PERIODIC_FIXED_DATA
void dongle_download_init()
{
    memset(&lat_test, 0, sizeof(lat_test));
}
void dongle_download_reset()
{
    memset(&lat_test.download, 0, sizeof(download_t));
}
#endif
#endif
void dongle_init()
{
#ifdef DONGLE_PLATFORM__ZEPHYR
    k_mutex_init(&dongle_mu);
#endif

    dongle_load();

    dongle_time = config.t_init;
    report_time = dongle_time;
    cur_id_idx = 0;
    epoch = 0;
    non_report_entry_count = 0;
    signal_id = 0;

#ifdef MODE__STAT
    // must call dongle_config_load before this
    dongle_storage_read_stat(&storage, &stats, sizeof(stats_t));
    if (!stats.storage_checksum)
    {
        app_log_info("Existing Statistics Found\r\n");
        dongle_stats();
    }
    else
    {
        dongle_stats_init();
    }
#endif

#ifdef MODE__PERIODIC_FIXED_DATA
    dongle_download_init();
#endif

    log_info("Dongle initialized\r\n");

    dongle_info();

    log_telemf("%02x\r\n", TELEM_TYPE_RESTART);

#ifdef DONGLE_PLATFORM__ZEPHYR
    k_timer_init(&kernel_time, NULL, NULL);

// Timer zero point
#define DUR K_MSEC(DONGLE_TIMER_RESOLUTION)
    k_timer_start(&kernel_time, DUR, DUR);
#undef DUR // Initial time
#endif
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
    if (lat_test.download.is_active)
    {
        lat_test.download.time += ms;
    }
    dongle_hp_timer += ms;
}

void dongle_download_start()
{
  lat_test.download.is_active = 1;
  log_info("Download started!\r\n");
  lat_test.payloads_started++;
}

#define dongle_update_download_stats(s, d) \
  stat_add(d.packet_buffer.received, s.n_bytes); \
  stat_add(d.n_syncs_lost, s.syncs_lost); \
//  stat_add(d.n_duplicate_packets, s.duplicates); \
//  stat_add(d.n_received_packets, s.packets); \
//  stat_add(d.n_duplicate_packets / d.n_received_packets, \
//           s.duplicate_rate); \

void dongle_download_fail(download_fail_reason *reason)
{
  if (lat_test.download.is_active) {
    lat_test.payloads_failed++;
    dongle_update_download_stats(lat_test.all_download_stats,
                                 lat_test.download);
    dongle_update_download_stats(lat_test.failed_download_stats,
                                     lat_test.download);
    *reason = *reason + 1;
    dongle_download_info();
    dongle_download_reset();
  }
}

void dongle_download_complete()
{
  log_info("Download complete!\r\n");
  lat_test.payloads_complete++;
  // compute latency
  double lat = lat_test.download.time;
  dongle_update_download_stats(lat_test.all_download_stats,
                                   lat_test.download);
  dongle_update_download_stats(lat_test.complete_download_stats.download_stats,
                                   lat_test.download);
  stat_add(lat, lat_test.complete_download_stats.periodic_data_avg_payload_lat);

  // check the content
  cf_gadget_init(&lat_test.cf, lat_test.download.packet_buffer.buf,
                 lat_test.download.packet_buffer.received);

  int status = 0;

  // these should exist
  if (!cf_gadget_lookup(&lat_test.cf, TEST_ID_EXIST_1)) {
      log_errorf("Cuckoofilter test failed: %s should exist\r\n",
                 TEST_ID_EXIST_1);
      status += 1;
  }
  if (!cf_gadget_lookup(&lat_test.cf, TEST_ID_EXIST_2)) {
      log_errorf("Cuckoofilter test failed: %s should exist\r\n",
                 TEST_ID_EXIST_2);
      status += 1;
  }

  // these shouldn't
  if (cf_gadget_lookup(&lat_test.cf, TEST_ID_NEXIST_1)) {
      log_errorf("Cuckoofilter test failed: %s should NOT exist\r\n",
                 TEST_ID_NEXIST_1);
      status += 1;
  }
  if (cf_gadget_lookup(&lat_test.cf, TEST_ID_NEXIST_2)) {
      log_errorf("Cuckoofilter test failed: %s should NOT exist\r\n",
                 TEST_ID_NEXIST_2);
      status += 1;
  }

  if (!status) {
      log_infof("Cuckoofilter test passed\r\n");
  }

  dongle_download_reset();
}

void dongle_on_sync_lost()
{
  log_telemf("%02x,%.0f\r\n", TELEM_TYPE_PERIODIC_SYNC_LOST, dongle_hp_timer);
  if (lat_test.download.is_active) {
      log_info("Download failed - lost sync.\r\n");
      lat_test.download.n_syncs_lost++;
  }
}

void dongle_on_periodic_data_error(int8_t rssi)
{
  log_telemf("%02x,%.0f\r\n", TELEM_TYPE_PERIODIC_PKT_ERROR, dongle_hp_timer);
#ifdef MODE__STAT
    stats.num_periodic_data_error++;
    stat_add(rssi, stats.periodic_data_rssi);
    lat_test.download.n_corrupt_packets++;
#endif
}

void dongle_on_periodic_data(uint8_t *data, uint8_t data_len, int8_t rssi)
{
//    log_bytes(printf, printf, data, data_len, "data");
    if (data_len < PACKET_HEADER_LEN) {
        log_telemf("%02x,%.0f,%d,%d\r\n", TELEM_TYPE_PERIODIC_PKT_DATA,
                   dongle_hp_timer, rssi, data_len);
        log_error("not enough data to read sequence numbers\r\n");
        log_error("len: %d\r\n", data_len);
        lat_test.download.n_corrupt_packets++;
        return;
    }

    // extract sequence numbers
    uint32_t seq, chunk, chunk_len;
    memcpy(&chunk, data, sizeof(uint32_t));
    memcpy(&seq, data + sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&chunk_len, data + 2*sizeof(uint32_t), sizeof(uint32_t));

    log_telemf("%02x,%.0f,%d,%d,%lu,%lu,%lu\r\n", TELEM_TYPE_PERIODIC_PKT_DATA,
                       dongle_hp_timer, rssi, data_len, seq, chunk, chunk_len);

#ifdef MODE__STAT
    stats.total_periodic_data_size += data_len;
    stats.num_periodic_data++;
    stat_add(data_len, stats.periodic_data_size);
    stat_add(rssi, stats.periodic_data_rssi);
#endif
#ifdef MODE__PERIODIC_FIXED_DATA
    if (lat_test.download.is_active
          && chunk != lat_test.download.packet_buffer.chunk_num) {
        // forced to switch chunks

        log_infof("Downloading chunk %lu\r\n", chunk);
        dongle_download_reset();
        lat_test.download.packet_buffer.chunk_num = chunk;
    } else if (lat_test.download.is_active
          && chunk_len != lat_test.download.packet_buffer.chunk_len) {
        log_errorf("Chunk length mismatch: previous: %lu, new: %lu\r\n",
                   lat_test.download.packet_buffer.chunk_len, chunk_len);
    }
    if (seq >= MAX_NUM_PACKETS_PER_FILTER) {
        log_errorf("Error: sequence number out of bounds\r\n");
    } else {
        if (!lat_test.download.is_active) {
            dongle_download_start();
            lat_test.download.packet_buffer.chunk_len = chunk_len;
        }
        lat_test.download.n_total_packets++;
        int prev = lat_test.download.packet_buffer.counts[seq];
        lat_test.download.packet_buffer.counts[seq]++;
        if (prev == 0) {
            // this is an unseen packet
//            lat_test.packet_buffer.num_distinct++;
            uint8_t len = data_len - PACKET_HEADER_LEN;
            memcpy(lat_test.download.packet_buffer.buf + (seq * MAX_PACKET_SIZE),
                   data + PACKET_HEADER_LEN, len);
            lat_test.download.packet_buffer.received += len;
            log_infof("download progress: %.2f%%\r\n",
                      ((float) lat_test.download.packet_buffer.received
                       / TEST_FILTER_LEN) * 100);
            if (lat_test.download.packet_buffer.received
                  == lat_test.download.packet_buffer.chunk_len) {
                dongle_download_complete();
            }
        }
    }
#endif
}

// UPDATE
// For callback-based timers. This is called with the mutex
// locked whenever the application clock obtains a new value.
void dongle_on_clock_update()
{
    // update epoch
#define t_init config.t_init
    dongle_epoch_counter_t new_epoch = epoch_i(dongle_time, t_init);
#undef t_init
    if (new_epoch != epoch)
    {
        log_debugf("EPOCH STARTED: %lu\r\n", new_epoch);
        epoch = new_epoch;
        signal_id = 0;
        // TODO: log time to flash
    }
    dongle_report();
}

// MAIN LOOP
// timing and control logic is largely the same as the beacon
// application. The main difference is that scanning does not
// require restart for a new epoch.
void dongle_loop()
{
// For the Zephyr platform, an explicit loop is defined. Others
// are configured to set up and call the clock update callback.
#ifdef DONGLE_PLATFORM__ZEPHYR
    uint32_t timer_status = 0;

    int err = 0;
    while (!err)
    {
        // get most updated time
        timer_status = k_timer_status_sync(&kernel_time);
        timer_status += k_timer_status_get(&kernel_time);
        LOCK dongle_time += timer_status;
        dongle_on_clock_update();
        UNLOCK
    }
#else
    DONGLE_NO_OP;
#endif
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

// compares two ephemeral ids
int compare_eph_id(beacon_eph_id_t *a, beacon_eph_id_t *b)
{
#define A (a->bytes)
#define B (b->bytes)
    for (int i = 0; i < BEACON_EPH_ID_HASH_LEN; i++)
    {
        if (A[i] != B[i])
            return 1;
    }
#undef B
#undef A
    return 0;
}

static void _dongle_encounter_(encounter_broadcast_t *enc, size_t i)
{
#define en (*enc)
    // when a valid encounter is detected
    // log the encounter
    log_debugf("Beacon Encounter (id=%lu, t_b=%lu, t_d=%lu)\r\n", *en.b, *en.t,
               dongle_time);
    // Write to storage
    dongle_storage_log_encounter(&storage, enc->loc, enc->b, enc->t, &dongle_time,
                                 enc->eph);
#ifdef MODE__TEST
    if (*en.b == TEST_BEACON_ID)
    {
        test_encounters++;
    }
#define test_en (test_encounter_list[total_test_encounters])
    memcpy(&test_en.location_id, enc->loc, sizeof(beacon_location_id_t));
    test_en.beacon_id = *enc->b;
    test_en.beacon_time = *enc->t;
    test_en.dongle_time = dongle_time;
    test_en.eph_id = *enc->eph;
    log_debugf("Test Encounter: (index=%d)\r\n", total_test_encounters);
    //_display_encounter_(&test_en);
#undef test_en
    total_test_encounters++;
#endif
    // reset the observation time
    obs_time[i] = dongle_time;
#undef en
}

static uint64_t dongle_track(encounter_broadcast_t *enc, int8_t rssi, uint64_t signal_id)
{
#define en (*enc)

    // Check the broadcast UUID
    beacon_id_t service_id = (*en.b & BEACON_SERVICE_ID_MASK) >> 16;
    if (service_id != BROADCAST_SERVICE_ID)
    {
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

    // determine which tracked id, if any, is a match
    size_t i = DONGLE_MAX_BC_TRACKED;
    for (size_t j = 0; j < DONGLE_MAX_BC_TRACKED; j++)
    {
        if (!compare_eph_id(en.eph, &cur_id[j]))
        {
            i = j;
        }
    }
    if (i == DONGLE_MAX_BC_TRACKED)
    {
        // if no match was found, start tracking the new id, replacing the oldest
        // one currently tracked
        i = cur_id_idx;
        log_debugf("new ephemeral id observed (beacon=%lu), tracking at index %d\r\n",
                   *en.b, i);
        print_bytes(en.eph->bytes, BEACON_EPH_ID_HASH_LEN, "eph_id");
        cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;
        memcpy(&cur_id[i], en.eph->bytes, BEACON_EPH_ID_HASH_LEN);
        obs_time[i] = dongle_time;

#ifdef MODE__STAT
        stats.num_obs_ids++;
#endif
        log_telemf("%02x,%u,%u,%lu,%u,%u\r\n",
                   TELEM_TYPE_BROADCAST_TRACK_NEW,
                   dongle_time, epoch,
                   signal_id,
                   *en.b, *en.t);
    }
    else
    {
        // when a matching ephemeral id is observed
        log_telemf("%02x,%u,%u,%lu,%u,%u\r\n",
                   TELEM_TYPE_BROADCAST_TRACK_MATCH,
                   dongle_time, epoch,
                   signal_id,
                   *en.b, *en.t);
        // check conditions for a valid encounter
        dongle_timer_t dur = dongle_time - obs_time[i];
        if (dur >= DONGLE_ENCOUNTER_MIN_TIME)
        {
            _dongle_encounter_(&en, i);
#ifdef MODE__STAT
            stat_add(rssi, stats.encounter_rssi);
#endif
            log_telemf("%02x,%u,%u,%lu,%u,%u,%u\r\n",
                       TELEM_TYPE_ENCOUNTER,
                       dongle_time, epoch,
                       signal_id,
                       *en.b, *en.t, dur);
        }
    }
#undef en
    return signal_id;
}

#ifdef DONGLE_PLATFORM__ZEPHYR
void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
#define len (ad->len)
#define add (addr->a.val)
#define dat (ad->data)
#else
void dongle_log(bd_addr *addr, int8_t rssi, uint8_t *data, uint8_t data_len)
{
#define len (data_len)
#define add (addr->addr)
#define dat (data)
#endif
    // Filter mis-sized packets
    if (len != ENCOUNTER_BROADCAST_SIZE + 1)
    // TODO: should check for a periodic packet identifier
    {
        return;
    }
    LOCK
        log_telemf("%02x,%u,%u,%lu,%02x%02x%02x%02x%02x%02x,%d\r\n",
                   TELEM_TYPE_SCAN_RESULT,
                   dongle_time, epoch,
                   signal_id,
                   add[0], add[1], add[2], add[3], add[4], add[5], rssi);
    //print_bytes(dat, data_len, "scan-data pre-decode");
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

uint8_t compare_encounter_entry(dongle_encounter_entry a, dongle_encounter_entry b)
{
    uint8_t res = 0;
    log_debug("comparing ids\r\n");
#define check(val, idx) res |= (val << idx)
    check(!(a.beacon_id == b.beacon_id), 0);
    log_debug("comparing beacon time\r\n");
    check(!(a.beacon_time == b.beacon_time), 1);
    log_debug("comparing dongle time\r\n");
    check(!(a.dongle_time == b.dongle_time), 2);
    log_debug("comparing location ids\r\n");
    check(!(a.location_id == b.location_id), 3);
    log_debug("comparing eph. ids\r\n");
    check(compare_eph_id(&a.eph_id, &b.eph_id), 4);
#undef check
    return res;
}

void dongle_info()
{
    log_info("\r\n");
    log_info("Info:\r\n");
#ifdef DONGLE_PLATFORM__ZEPHYR
    log_infof("    Platform:                        %s\r\n", "Zephyr OS");
    log_infof("    Board:                           %s\r\n", CONFIG_BOARD);
    log_infof("    Bluetooth device name:           %s\r\n", CONFIG_BT_DEVICE_NAME);
#else
    log_infof("    Platform:                        %s\r\n", "Gecko");
#define CONFIG_UNKOWN "Unkown"
    log_infof("    Board:                           %s\r\n", CONFIG_UNKOWN);
    log_infof("    Bluetooth device name:           %s\r\n", CONFIG_UNKOWN);
#undef CONFIG_UNKOWN
#endif
    log_infof("    Application Version:             %s\r\n", APPL_VERSION);
    log_infof("    Dongle ID:                       %lu\r\n", config.id);
    log_infof("    Initial clock:                   %lu\r\n", config.t_init);
    log_infof("    Backend public key size:         %lu bytes\r\n", config.backend_pk_size);
    log_infof("    Secret key size:                 %lu bytes\r\n", config.dongle_sk_size);
    log_infof("    Timer Resolution:                %u ms\r\n", DONGLE_TIMER_RESOLUTION);
    log_infof("    Epoch Length:                    %u ms\r\n", BEACON_EPOCH_LENGTH * DONGLE_TIMER_RESOLUTION);
    log_infof("    Report Interval:                 %u ms\r\n", DONGLE_REPORT_INTERVAL * DONGLE_TIMER_RESOLUTION);
}

void dongle_report()
{
    // do report
    if (dongle_time - report_time >= DONGLE_REPORT_INTERVAL)
    {

        log_info("\r\n");
        log_info("***          Begin Report          ***\r\n");

        dongle_info();
        dongle_storage_info(&storage);
        dongle_stats();
        dongle_download_test_info();
#ifdef MODE__TEST
        dongle_test();
#endif

        log_info("\r\n");
        log_info("***          End Report            ***\r\n");

        non_report_entry_count = dongle_storage_num_encounters_total(&storage);
        report_time = dongle_time;
    }
}

void dongle_stats()
{
    enctr_entry_counter_t num = dongle_storage_num_encounters_total(&storage);
    enctr_entry_counter_t cur = dongle_storage_num_encounters_current(&storage);
#ifdef MODE__STAT
    log_info("\r\n");
    log_info("Statistics:\r\n");
    log_infof("    Dongle timer:                        %lu\r\n", dongle_time);
    // Large integers here are casted for formatting compatabilty. This may result in false
    // output for large values.
    log_infof("    Encounters logged since last report: %lu\r\n", (uint32_t)(num - non_report_entry_count));
    log_infof("    Total Encounters logged (All-time):  %lu\r\n", (uint32_t)num);

    log_infof("    Total Encounters logged (Stored):    %lu%s\r\n",
              (uint32_t)cur,
              cur == dongle_storage_max_log_count(&storage) ? " (MAX)" : "");
    log_infof("    Distinct Eph. IDs observed:          %d\r\n", stats.num_obs_ids);
    log_infof("    Legacy Scan Results:                 %lu\r\n", stats.num_scan_results);
    log_infof("    Periodic Pkts. Received:             %lu\r\n", stats.num_periodic_data);
    stat_show(stats.periodic_data_size, "Periodic Pkt. Size", "bytes");
    log_infof("    Periodic Pkts. Received (error):     %lu\r\n", stats.num_periodic_data_error);
    log_infof("    Total Bytes Transferred:             %lu\r\n", stats.total_periodic_data_size);
    log_infof("    Total Time (s):                      %f\r\n", stats.total_periodic_data_time);
    stat_compute_thrpt(&stats);
    log_infof("    Avg. Throughput (kb/s)               %f\r\n", stats.periodic_data_avg_thrpt);
    stat_show(stats.scan_rssi, "Legacy Scan RSSI", "");
    stat_show(stats.encounter_rssi, "Logged Encounter RSSI", "");
    stat_show(stats.periodic_data_rssi, "Periodic Data RSSI", "");
    dongle_storage_save_stat(&storage, &stats, sizeof(stats_t));
    dongle_stats_init(); // reset the stats
#endif
}

void dongle_download_show_stats(download_stats_t * stats, char *name)
{
  log_infof("Download Statistics (%s):\r\n", name);
//  stat_show(stats->packets,
//            "    Received Packets", "packets");
//  stat_show(stats->duplicates,
//            "    Duplicated Packets", "packets");
//  stat_show(stats->duplicate_rate,
//            "    Duplicate rate", "duplicate/received");
  stat_show(stats->n_bytes,
            "    Bytes Received", "bytes");
  stat_show(stats->syncs_lost,
            "    Syncs Lost", "syncs");
}

void dongle_download_test_info()
{
#ifdef MODE__PERIODIC_FIXED_DATA
    log_info("\r\n");
    log_info("Risk Broadcast:\r\n");
    log_infof("    Downloads Started: %d\r\n", lat_test.payloads_started);
    log_infof("    Downloads Completed: %d\r\n", lat_test.payloads_complete);
    log_infof("    Downloads Failed: %d\r\n", lat_test.payloads_failed);
//    log_infof("        missing data: %d\r\n", lat_test.fail_missing_data);
//    log_infof("        packet corrupt: %d\r\n", lat_test.fail_packet_corrupt);
//    log_infof("        no sequence number (pkt too short): %d\r\n",
//              lat_test.fail_packet_no_seq);
//    log_infof("        skipped packet: %d\r\n", lat_test.fail_packet_skipped);
//    log_infof("        sync lost: %d\r\n", lat_test.fail_sync_lost);
    dongle_download_show_stats(&lat_test.complete_download_stats.download_stats,
                               "completed");
    stat_show(lat_test.complete_download_stats.periodic_data_avg_payload_lat,
                "    Download time", "ms");
    dongle_download_show_stats(&lat_test.failed_download_stats,
                               "failed");
    dongle_download_show_stats(&lat_test.all_download_stats,
                               "overall");

    dongle_download_init();
#endif
}

void dongle_download_info()
{
#ifdef MODE__PERIODIC_FIXED_DATA
    log_infof("Total time: %.0f ms\r\n", lat_test.download.time);
    log_info("Packets Received: %lu\r\n", lat_test.download.n_total_packets);
    log_infof("Data bytes downloaded: %lu\r\n",
              lat_test.download.packet_buffer.received);
#endif
}

#undef alpha

#ifdef MODE__TEST

//
// TESTING
//

int test_compare_entry_idx(enctr_entry_counter_t i, dongle_encounter_entry *entry)
{
    //log_infof("%.4lu.", i);
    //_display_encounter_(entry);
    log_debug("comparing logged encounter against test record\r\n");
    dongle_encounter_entry test_en = test_encounter_list[i];
    uint8_t comp = compare_encounter_entry(*entry, test_en);
    if (comp)
    {
        log_infof("FAILED: entry mismatch (index=%lu)\r\n", (uint32_t)i);
        log_infof("Comp=%u\r\n", comp);
        test_errors++;
        log_info("Entry from log:\r\n");
        _display_encounter_(entry);
        log_info("Test:\r\n");
        _display_encounter_(&test_en);
        return 0;
    }
    else
    {
        log_debug("Entries MATCH\r\n");
    }
    return 1;
}

int test_check_entry_age(enctr_entry_counter_t i, dongle_encounter_entry *entry)
{
    if ((dongle_time - entry->dongle_time) > DONGLE_MAX_LOG_AGE)
    {
        log_infof("FAILED: Encounter at index %lu is too old (age=%lu)\r\n",
                  (uint32_t)i, (uint32_t)entry->dongle_time);
        test_errors++;
        return 0;
    }
    return 1;
}

void dongle_test()
{
    // Run Tests
    log_info("\r\n");
    log_info("Tests:\r\n");
    test_errors = 0;
#define FAIL(msg)                          \
    log_infof("    FAILURE: %s\r\n", msg); \
    test_errors++

    log_info("    ? Testing that OTPs are loaded\r\n");
    int otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
    if (otp_idx != 7)
    {
        FAIL("Index 7 Not loaded correctly");
    }
    otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[0].val);
    if (otp_idx != 0)
    {
        FAIL("Index 0 Not loaded correctly");
    }
    log_info("    ? Testing that OTP cannot be re-used\r\n");
    otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
    if (otp_idx >= 0)
    {
        FAIL("Index 7 was found again");
    }
    // Restore OTP data
    // Need to re-save config as the shared page must be erased
    dongle_storage_save_config(&storage, &config);
    dongle_storage_save_otp(&storage, TEST_OTPS);

    log_info("    ? Testing that correct number of encounters were logged\r\n");
    int numExpected = (DONGLE_REPORT_INTERVAL / DONGLE_ENCOUNTER_MIN_TIME);
    // Tolerant expectation provided to account for timing differences
    int tolExpected = numExpected + 1;
    if (test_encounters != numExpected && test_encounters != tolExpected)
    {
        FAIL("Wrong number of encounters.");
        log_infof("Encounters logged in window: %d; Expected: %d or %d\r\n",
                  test_encounters, numExpected, tolExpected);
    }

    log_info("    ? Testing that logged encounters are correct\r\n");
    enctr_entry_counter_t num = dongle_storage_num_encounters_total(&storage);
    if (num > non_report_entry_count)
    {
        // There are new entries logged
        dongle_storage_load_encounters_from_time(&storage, report_time,
                                                 test_compare_entry_idx);
    }
    else
    {
        log_error("Cannot test, no new encounters logged.\r\n");
    }

    log_info("    ? Testing that old encounters are deleted\r\n");
    dongle_storage_clean_log(&storage, dongle_time);
    num = dongle_storage_num_encounters_current(&storage);
    if (dongle_time <= DONGLE_MAX_LOG_AGE + DONGLE_ENCOUNTER_MIN_TIME)
    {
        log_error("Cannot test, not enough time has elapsed.\r\n");
    }
    else if (num < 1)
    {
        log_error("Cannot test, no encounters stored.\r\n");
    }
    else
    {
        dongle_storage_load_all_encounter(&storage, test_check_entry_age);
    }

    if (test_errors)
    {
        log_info("\r\n");
        log_infof("    x Tests Failed: status = %d\r\n", test_errors);
    }
    else
    {
        log_info("\r\n");
        log_info("    ✔ Tests Passed\r\n");
    }
#undef FAIL
    test_encounters = 0;
    total_test_encounters = 0;
}

#endif

#undef APPL__DONGLE
#undef APPL_VERSION
#undef LOG_LEVEL__INFO
#undef MODE__TEST
#undef MODE__STAT
