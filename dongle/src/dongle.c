//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "./dongle.h"

#define APPL__DONGLE
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

#include "./storage.h"
#include "./test.h"
#include "./access.h"
#include "./encounter.h"
#include "./telemetry.h"

#include "../../common/src/log.h"
#include "../../common/src/pancast.h"
#include "../../common/src/test.h"
#include "../../common/src/util.h"

//
// GLOBAL MEMORY
// All of the variables used throughout the main program are allocated here

// 1. Mutual exclusion
// Access for both central updates and scan interupts
// is given on a FIFO basis
#ifdef DONGLE_PLATFORM__ZEPHYR
struct k_mutex dongle_mu;
#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);
#else
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
enctr_entry_counter_t enctr_entries_offset;

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
void dongle_main()
#endif
{
    log_info("\n"), log_info("Starting Dongle...\n");

#ifdef MODE__TEST
    log_info("Test mode enabled\n");
#endif
#ifdef MODE__STAT
    log_info("Statistics enabled\n");
#define α 0.1
#define exp_avg(a, x) !a ? x : (α * x) + ((1 - α) * a);
#endif

#ifdef DONGLE_PLATFORM__ZEPHYR
    int err;

    err = bt_enable(NULL);
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\n", err);
        return;
    }

    log_info("Bluetooth initialized\n");
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
            BT_LE_SCAN_TYPE_PASSIVE,   // passive scan
            BT_LE_SCAN_OPT_NONE,       // no options; in particular, allow duplicates
            BT_GAP_SCAN_FAST_INTERVAL, // interval
            BT_GAP_SCAN_FAST_WINDOW    // window
            ),
        dongle_log);
#else
    DONGLE_NO_OP;
    int err = 0;
#endif
    if (err)
    {
        log_errorf("Scanning failed to start (err %d)\n", err);
        return;
    }
    else
    {
        log_debug("Scanning successfully started\n");
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
    enctr_entries_offset = 0;
    signal_id = 0;

    log_info("Dongle initialized\n");

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
        log_debugf("EPOCH STARTED: %u\n", new_epoch);
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
    log_debugf("Beacon Encounter (id=%u, t_b=%u, t_d=%u)\n", *en.b, *en.t,
               dongle_time);
    // Write to storage
    dongle_storage_print(&storage, 0x22000, 32);
    dongle_storage_log_encounter(&storage, enc->loc, enc->b, enc->t, &dongle_time,
                                 enc->eph);
    dongle_storage_print(&storage, 0x22000, 32);
#ifdef MODE__TEST
    if (*en.b == TEST_BEACON_ID)
    {
        test_encounters++;
    }
#define test_en (test_encounter_list[total_test_encounters])
    test_en.location_id = *enc->loc;
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

static void dongle_track(encounter_broadcast_t *enc, int8_t rssi, uint64_t signal_id)
{
#define en (*enc)

    // Check the broadcast UUID
    beacon_id_t service_id = (*en.b & BEACON_SERVICE_ID_MASK) >> 16;
    if (service_id != BROADCAST_SERVICE_ID)
    {
        log_telemf("%02x,%u,%u,%llu\n",
                   TELEM_TYPE_BROADCAST_ID_MISMATCH,
                   dongle_time, epoch,
                   signal_id);
        return;
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
        log_debugf("new ephemeral id observed (beacon=%u), tracking at index %d\n",
                   *en.b, i);
        print_bytes(en.eph->bytes, BEACON_EPH_ID_HASH_LEN, "eph_id");
        cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;
        memcpy(&cur_id[i], en.eph->bytes, BEACON_EPH_ID_HASH_LEN);
        obs_time[i] = dongle_time;

#ifdef MODE__STAT
        num_obs_ids++;
        avg_rssi = exp_avg(avg_rssi, rssi);
#endif
        log_telemf("%02x,%u,%u,%llu,%u,%u\n",
                   TELEM_TYPE_BROADCAST_TRACK_NEW,
                   dongle_time, epoch,
                   signal_id,
                   *en.b, *en.t);
    }
    else
    {
        // when a matching ephemeral id is observed
        log_telemf("%02x,%u,%u,%llu,%u,%u\n",
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
            log_telemf("%02x,%u,%u,%llu,%u,%u,%u\n",
                       TELEM_TYPE_ENCOUNTER,
                       dongle_time, epoch,
                       signal_id,
                       *en.b, *en.t, dur);
        }
    }
#undef en
}

#ifdef DONGLE_PLATFORM__ZEPHYR
void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    // Filter mis-sized packets
    if (ad->len != ENCOUNTER_BROADCAST_SIZE + 1)
    {
        return;
    }
    LOCK
#define add addr->a.val
        log_telemf("%02x,%u,%u,%llu,%02x%02x%02x%02x%02x%02x,%d\n",
                   TELEM_TYPE_SCAN_RESULT,
                   dongle_time, epoch,
                   signal_id,
                   add[0], add[1], add[2], add[3], add[4], add[5], rssi);
#undef add
    decode_payload(ad->data);
    encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t *)ad->data);
    dongle_track(&en, rssi, signal_id);
    signal_id++;
    UNLOCK
}
#else
void dongle_log();
#endif

uint8_t compare_encounter_entry(dongle_encounter_entry a, dongle_encounter_entry b)
{
    uint8_t res = 0;
    log_debug("comparing ids\n");
#define check(val, idx) res |= (val << idx)
    check(!(a.beacon_id == b.beacon_id), 0);
    log_debug("comparing beacon time\n");
    check(!(a.beacon_time == b.beacon_time), 1);
    log_debug("comparing dongle time\n");
    check(!(a.dongle_time == b.dongle_time), 2);
    log_debug("comparing location ids\n");
    check(!(a.location_id == b.location_id), 3);
    log_debug("comparing eph. ids\n");
    check(compare_eph_id(&a.eph_id, &b.eph_id), 4);
#undef check
    return res;
}

int _report_encounter_(enctr_entry_counter_t i, dongle_encounter_entry *entry)
{
    //log_infof("%.4llu.", i);
    //_display_encounter_(entry);
#ifdef MODE__TEST
    log_debug("comparing logged encounter against test record\n");
    dongle_encounter_entry test_en = test_encounter_list[i - enctr_entries_offset];
    uint8_t comp = compare_encounter_entry(*entry, test_en);
    if (comp)
    {
        log_info("FAILED: entry mismatch\n");
        log_infof("Comp=%u\n", comp);
        test_errors++;
        log_info("Entry from log:\n");
        _display_encounter_(entry);
        log_info("Test:\n");
        _display_encounter_(&test_en);
    }
    else
    {
        log_debug("Entries MATCH\n");
    }
#endif
    return 1;
}

void dongle_info()
{
    log_info("\n");
    log_info("Info:\n");
#ifdef DONGLE_PLATFORM__ZEPHYR
    log_infof("    Platform:                        %s\n", "Zephyr OS");
    log_infof("    Board:                           %s\n", CONFIG_BOARD);
    log_infof("    Bluetooth device name:           %s\n", CONFIG_BT_DEVICE_NAME);
#else
    log_infof("    Platform:                        %s\n", "Gecko");
#define CONFIG_UNKOWN "Unkown"
    log_infof("    Board:                           %s\n", CONFIG_UNKOWN);
    log_infof("    Bluetooth device name:           %s\n", CONFIG_UNKOWN);
#undef CONFIG_UNKOWN
#endif
    log_infof("    Application Version:             %s\n", APPL_VERSION);
    log_infof("    Dongle ID:                       %u\n", config.id);
    log_infof("    Initial clock:                   %u\n", config.t_init);
    log_infof("    Backend public key size:         %u bytes\n", config.backend_pk_size);
    log_infof("    Secret key size:                 %u bytes\n", config.dongle_sk_size);
    log_infof("    Timer Resolution:                %u ms\n", DONGLE_TIMER_RESOLUTION);
    log_infof("    Epoch Length:                    %u ms\n", BEACON_EPOCH_LENGTH * DONGLE_TIMER_RESOLUTION);
    log_infof("    Report Interval:                 %u ms\n", DONGLE_REPORT_INTERVAL * DONGLE_TIMER_RESOLUTION);
}

void dongle_report()
{
    // do report
    if (dongle_time - report_time >= DONGLE_REPORT_INTERVAL)
    {
        report_time = dongle_time;

        log_info("\n");
        log_info("***          Begin Report          ***\n");

        dongle_info();
        dongle_stats();
        dongle_test();

        log_info("\n");
        log_info("***          End Report            ***\n");
    }
}

void dongle_stats()
{
    enctr_entry_counter_t num = dongle_storage_num_encounters(&storage);
#ifdef MODE__STAT
    log_info("\n");
    log_info("Statistics:\n");
    log_infof("    Dongle timer:                        %u\n", dongle_time);
    log_infof("    Encounters logged since last report: %llu\n", num - enctr_entries_offset);
    log_infof("    Total Encounters logged:             %llu\n", num);
    log_infof("    Distinct Eph. IDs observed:          %d\n", num_obs_ids);
    log_infof("    Avg. Broadcast RSSI:                 %d\n", avg_rssi);
    log_infof("    Avg. Encounter RSSI (logged):        %d\n", avg_encounter_rssi);
#endif
    enctr_entries_offset = num;
}

#undef α

void dongle_test()
{
    // Run Tests
#ifdef MODE__TEST
    log_info("\n");
    log_info("Tests:\n");
    test_errors = 0;
#define FAIL(msg) (log_infof("    FAILURE: %s\n", msg), test_errors++)

    log_info("    ? Testing that OTPs are loaded\n");
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
    log_info("    ? Testing that OTP cannot be re-used\n");
    otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
    if (otp_idx >= 0)
    {
        FAIL("Index 7 was found again");
    }
    // Restore OTP data
    // Need to re-save config as the shared page must be erased
    dongle_storage_save_config(&storage, &config);
    dongle_storage_save_otp(&storage, TEST_OTPS);

    log_info("    ? Testing that logged encounters are correct\n");
    enctr_entry_counter_t num = dongle_storage_num_encounters(&storage);
    if (num > enctr_entries_offset)
    {
        // There are new entries logged
        dongle_storage_load_encounter(&storage, enctr_entries_offset,
                                      _report_encounter_);
    }
    log_info("    ? Testing that correct number of encounters were logged\n");
    int numExpected = (DONGLE_REPORT_INTERVAL / DONGLE_ENCOUNTER_MIN_TIME);
    if (test_encounters != numExpected)
    {
        FAIL("Wrong number of encounters.");
        log_infof("Encounters logged in window: %d; Expected: %d\n",
                  test_encounters, numExpected);
    }
    if (test_errors)
    {
        log_info("\n");
        log_infof("    x Tests Failed: status = %d\n", test_errors);
    }
    else
    {
        log_info("\n");
        log_info("    ✔ Tests Passed\n");
    }
#undef FAIL
    test_encounters = 0;
    total_test_encounters = 0;
#endif
}

#undef APPL__DONGLE
#undef APPL_VERSION
#undef LOG_LEVEL__INFO
#undef MODE__TEST
#undef MODE__STAT