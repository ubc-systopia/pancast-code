//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include "./dongle.h"

#define LOG_LEVEL__INFO
#define MODE__TEST

#include <sys/printk.h>
#include <sys/util.h>
#include <zephyr.h>
#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>
#include <drivers/flash.h>

#include "./storage.h"
#include "./test.h"
#include "./access.h"
#include "./encounter.h"

#include "../../common/src/log.h"
#include "../../common/src/pancast.h"
#include "../../common/src/test.h"
#include "../../common/src/util.h"

// STATIC PARAMETERS
// (Approx) number of time units between each report written to output
#define DONGLE_REPORT_INTERVAL 30

// number of distinct broadcast ids to keep track of at one time
#define DONGLE_MAX_BC_TRACKED 16

//
// GLOBAL MEMORY
// All of the variables used throughout the main program are allocated here

// 1. Mutual exclusion
// Access for both central updates and scan interupts
// is given on a FIFO basis
struct k_mutex dongle_mu;
#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);

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
struct k_timer kernel_time;
dongle_timer_t dongle_time; // main dongle timer
dongle_timer_t report_time;
beacon_eph_id_t
    cur_id[DONGLE_MAX_BC_TRACKED]; // currently observed ephemeral id
dongle_timer_t
    obs_time[DONGLE_MAX_BC_TRACKED]; // time of last new id observation
size_t cur_id_idx;

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

//
// ROUTINES
//

// MAIN
// Entrypoint of the application
// Initialize bluetooth, then call advertising and scan routines
void main(void)
{
    log_infof("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);

#ifdef MODE__TEST
    log_info("RUNNING IN TEST MODE\n");
#endif

    int err;

    err = bt_enable(NULL);
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\n", err);
        return;
    }

    log_info("Bluetooth initialized\n");

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
    int err = err = bt_le_scan_start(
        BT_LE_SCAN_PARAM(
            BT_LE_SCAN_TYPE_PASSIVE,   // passive scan
            BT_LE_SCAN_OPT_NONE,       // no options; in particular, allow duplicates
            BT_GAP_SCAN_FAST_INTERVAL, // interval
            BT_GAP_SCAN_FAST_WINDOW    // window
            ),
        dongle_log);
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
    k_mutex_init(&dongle_mu);

    dongle_load();

    dongle_time = config.t_init;
    report_time = dongle_time;
    cur_id_idx = 0;
    epoch = 0;
    enctr_entries_offset = 0;

    k_timer_init(&kernel_time, NULL, NULL);

// Timer zero point
#define DUR K_MSEC(DONGLE_TIMER_RESOLUTION)
    k_timer_start(&kernel_time, DUR, DUR);
#undef DUR // Initial time
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

// MAIN LOOP
// timing and control logic is largely the same as the beacon
// application. The main difference is that scanning does not
// require restart for a new epoch.
void dongle_loop()
{
    uint32_t timer_status = 0;

    int err = 0;
    while (!err)
    {
        // get most updated time
        timer_status = k_timer_status_sync(&kernel_time);
        timer_status += k_timer_status_get(&kernel_time);
        LOCK dongle_time += timer_status;

        // update epoch
        static dongle_epoch_counter_t old_epoch;
        old_epoch = epoch;
#define t_init config.t_init
        epoch = epoch_i(dongle_time, t_init);
#undef t_init
        if (epoch != old_epoch)
        {
            log_debugf("EPOCH STARTED: %u\n", epoch);
            // TODO: log time to flash
        }
        dongle_report();
        UNLOCK
    }
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

static void _dongle_track_(encounter_broadcast_t *enc)
{
#define en (*enc)

    // Check the broadcast UUID
    beacon_id_t service_id = (*en.b & BEACON_SERVICE_ID_MASK) >> 16;
    if (service_id != BROADCAST_SERVICE_ID)
    {
        log_debugf("Broadcast skipped; service id 0x%x does not match\n", service_id);
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
    }
    else
    {
        // when a matching ephemeral id is observed
        // check conditions for a valid encounter
        dongle_timer_t dur = dongle_time - obs_time[i];
        if (dur >= DONGLE_ENCOUNTER_MIN_TIME)
        {
            _dongle_encounter_(&en, i);
        }
    }
#undef en
}

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
    decode_payload(ad->data);
    LOCK encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t *)ad->data);
    _dongle_track_(&en);
    UNLOCK
}

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

void dongle_report()
{
    // do report
    if (dongle_time - report_time >= DONGLE_REPORT_INTERVAL)
    {
        report_time = dongle_time;
        log_infof("\n*** Begin Report for %s ***\n\n", CONFIG_BT_DEVICE_NAME);
        log_infof("ID: %u\n", config.id);
        log_infof("Initial clock: %u\n", config.t_init);
        log_infof("Backend public key (%u bytes)\n", config.backend_pk_size);
        log_infof("Secret key (%u bytes)\n", config.dongle_sk_size);
        log_infof("dongle timer: %u\n", dongle_time);

        // Run Tests
#ifdef MODE__TEST
        log_info("\nTests:\n");
        test_errors = 0;
#define FAIL(msg) (log_infof("FAILURE: %s\n", msg), test_errors++)

        log_info("? Testing that OTPs are loaded\n");
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
        log_info("? Testing that OTP cannot be re-used\n");
        otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
        if (otp_idx >= 0)
        {
            FAIL("Index 7 was found again");
        }
        // Restore OTP data
        // Need to re-save config as the shared page must be erased
        dongle_storage_save_config(&storage, &config);
        dongle_storage_save_otp(&storage, TEST_OTPS);

        log_info("? Testing that logged encounters are correct\n");
        dongle_storage_load_encounter(&storage, enctr_entries_offset,
                                      _report_encounter_);
        log_info("? Testing that beacon broadcast was received\n");
        if (test_encounters < 1)
        {
            FAIL("Not enough encounters.");
            log_infof("Encounters logged in window: %d\n",
                      test_encounters);
        }
        if (test_errors)
        {
            log_infof("\n   x Tests Failed: status = %d\n\n", test_errors);
        }
        else
        {

            log_info("\n    ✔ Tests Passed\n\n");
        }
#undef FAIL
        test_encounters = 0;
        total_test_encounters = 0;
#endif
        enctr_entry_counter_t num = dongle_storage_num_encounters(&storage);
        log_infof("Encounters logged since last report: %llu\n", num - enctr_entries_offset);
        log_infof("Total Encounters logged: %llu\n", num);
        enctr_entries_offset = num;
        log_info("\n*** End Report ***\n\n");
    }
}
