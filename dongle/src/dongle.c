//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "./dongle.h"

#define APPL_VERSION "0.1.1"

#define LOG_LEVEL__INFO
#define MODE__TEST
#define MODE__STAT

#include <string.h>

#ifdef DONGLE_PLATFORM__ZEPHYR
#include <zephyr.h>
#include <sys/util.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>
#include <drivers/flash.h>
#endif

#include "storage.h"
#include "test.h"
#include "access.h"
#include "encounter.h"
#include "telemetry.h"

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

#ifdef MODE__STAT
int8_t num_obs_ids = 0;
int8_t avg_rssi = 0;
int8_t avg_encounter_rssi = 0;
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
    log_info("\r\n"), log_info("Starting Dongle...\r\n");

#ifdef MODE__TEST
    log_info("Test mode enabled\r\n");
#endif
#ifdef MODE__STAT
    log_info("Statistics enabled\r\n");
#define alpha 0.1
#define exp_avg(a, x) !a ? x : (alpha * x) + ((1 - alpha) * a);
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
    sl_bt_scanner_set_timing(gap_1m_phy, // Using 1M PHY - is this correct?
                             DONGLE_SCAN_INTERVAL,
                             DONGLE_SCAN_WINDOW);
    sl_bt_scanner_set_mode(gap_1m_phy, 0); // passive scan
    sl_bt_scanner_start(gap_1m_phy,
                        sl_bt_scanner_discover_observation); // scan all devices
    int err = 0;
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

    log_info("Dongle initialized\r\n");

    dongle_info();

    log_telemf("%02x", TELEM_TYPE_RESTART);

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
#ifdef MODE__TEST
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
        num_obs_ids++;
        avg_rssi = exp_avg(avg_rssi, rssi);
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
            avg_encounter_rssi = exp_avg(avg_encounter_rssi, rssi);
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
    {
        return;
    }
    LOCK
        log_telemf("%02x,%u,%u,%lu,%02x%02x%02x%02x%02x%02x,%d\r\n",
                   TELEM_TYPE_SCAN_RESULT,
                   dongle_time, epoch,
                   signal_id,
                   add[0], add[1], add[2], add[3], add[4], add[5], rssi);
    decode_payload(dat);
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
        report_time = dongle_time;

        log_info("\r\n");
        log_info("***          Begin Report          ***\r\n");

        dongle_info();
        dongle_stats();
        dongle_test();

        log_info("\r\n");
        log_info("***          End Report            ***\r\n");

        non_report_entry_count = dongle_storage_num_encounters(&storage);
    }
}

void dongle_stats()
{
    enctr_entry_counter_t num = dongle_storage_num_encounters(&storage);
#ifdef MODE__STAT
    log_info("\r\n");
    log_info("Statistics:\r\n");
    log_infof("    Dongle timer:                        %lu\r\n", dongle_time);
    // Large integers here are casted for formatting compatabilty. This may result in false
    // output for large values.
    log_infof("    Encounters logged since last report: %lu\r\n", (uint32_t)(num - non_report_entry_count));
    log_infof("    Total Encounters logged:             %lu\r\n", (uint32_t)num);
    log_infof("    Distinct Eph. IDs observed:          %d\r\n", num_obs_ids);
    log_infof("    Avg. Broadcast RSSI:                 %d\r\n", avg_rssi);
    log_infof("    Avg. Encounter RSSI (logged):        %d\r\n", avg_encounter_rssi);
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
    dongle_encounter_entry test_en = test_encounter_list[i - non_report_entry_count];
    uint8_t comp = compare_encounter_entry(*entry, test_en);
    if (comp)
    {
        log_info("FAILED: entry mismatch\r\n");
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
#define FAIL(msg) (log_infof("    FAILURE: %s\r\n", msg), test_errors++)

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
    if (test_encounters != numExpected)
    {
        FAIL("Wrong number of encounters.");
        log_infof("Encounters logged in window: %d; Expected: %d\r\n",
                  test_encounters, numExpected);
    }

    log_info("    ? Testing that logged encounters are correct\r\n");
    enctr_entry_counter_t num = dongle_storage_num_encounters(&storage);
    if (num > non_report_entry_count)
    {
        // There are new entries logged
        dongle_storage_load_encounter(&storage, non_report_entry_count,
                                      test_compare_entry_idx);
    }
    else
    {
        log_error("Cannot test, no new encounters logged.\r\n");
    }

    log_info("    ? Testing that old encounters are deleted\r\n");
    if (dongle_time > DONGLE_MAX_LOG_AGE + DONGLE_ENCOUNTER_MIN_TIME)
    {
        dongle_storage_load_all_encounter(&storage, test_check_entry_age);
    }
    else
    {
        log_error("Cannot test, not enough time has elapsed.\r\n");
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
