//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#define LOG_LEVEL__DEBUG
//#define MODE__TEST

#include <sys/printk.h>
#include <sys/util.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <drivers/flash.h>

#include "./dongle.h"
#include "./storage.h"

#include "../../common/src/log.h"
#include "../../common/src/pancast.h"
#include "../../common/src/test.h"
#include "../../common/src/util.h"

#define DONGLE_REPORT_INTERVAL 30

// number of distinct broadcast ids to keep track of at one time
#define DONGLE_MAX_BC_TRACKED 16

void main(void)
{
    log_infof("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);

    int err;

    err = bt_enable(NULL);
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\n", err);
        return;
    }

    log_info("Bluetooth initialized\n");

    dongle_scan();
}

//
// GLOBAL MEMORY
//

// Mutual exclusion
// Access for both central updates and scan interupts
// is given on a FIFO basis
struct k_mutex dongle_mu;
#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);

// Config
dongle_config_t config;

// Op
dongle_storage storage;
dongle_epoch_counter_t epoch;
struct k_timer kernel_time;
dongle_timer_t dongle_time; // main dongle timer
dongle_timer_t report_time;
beacon_eph_id_t
    cur_id[DONGLE_MAX_BC_TRACKED]; // currently observed ephemeral id
dongle_timer_t
    obs_time[DONGLE_MAX_BC_TRACKED]; // time of last new id observation
size_t cur_id_idx;

// Reporting
enctr_entry_counter_t enctr_entries_offset;

#ifdef MODE__TEST
int test_encounters;
#endif

static int decode_payload(uint8_t *data)
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
static int compare_eph_id(beacon_eph_id_t *a, beacon_eph_id_t *b)
{
#define A (a->bytes)
#define B (b->bytes)
    for (int i = 0; i < BEACON_EPH_ID_HASH_LEN; i++)
        if (A[i] != B[i])
            return 1;
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
    dongle_storage_log_encounter(&storage, enc->loc, enc->b, enc->t, &dongle_time,
                                 enc->eph);
#ifdef MODE__TEST
    if (*en.b == TEST_BEACON_ID)
    {
        test_encounters++;
    }
#endif
    // reset the observation time
    obs_time[i] = dongle_time;
#undef en
}

static void _dongle_track_(encounter_broadcast_t *enc)
{
#define en (*enc)
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

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
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

int _display_encounter_(int i, dongle_encounter_entry *entry)
{
    log_infof("< %u, %u, %u >\n", entry->dongle_time, entry->beacon_id,
              entry->beacon_time);
    return 1;
}

static void _dongle_report_()
{
    // do report
    if (dongle_time - report_time >= DONGLE_REPORT_INTERVAL)
    {
        report_time = dongle_time;
        log_infof("*** Begin Report for %s ***\n", CONFIG_BT_DEVICE_NAME);
        log_infof("ID: %u\n", config.id);
        log_infof("Initial clock: %u\n", config.t_init);
        log_infof("Backend public key (%u bytes)\n", config.backend_pk_size);
        log_infof("Secret key (%u bytes)\n", config.dongle_sk_size);
        log_infof("dongle timer: %u\n", dongle_time);
        // Report OTPs
        log_info("One-Time Passcodes:\n");
        dongle_otp_t otp;
        for (int i = 0; i < NUM_OTP; i++)
        {
            dongle_storage_load_otp(&storage, i, &otp);
            bool used = !((otp.flags & 0x0000000000000001) >> 0);
            log_infof("%.2d. Code: %llu; Used? %s\n", i, otp.val,
                      used ? "yes" : "no");
        }
        // Report logged encounters
        log_info("Encounters logged:\n");
        log_info("----------------------------------------------\n");
        log_info("< Time (dongle), Beacon ID, Time (beacon) >   \n");
        log_info("----------------------------------------------\n");
        dongle_storage_load_encounter(&storage, enctr_entries_offset,
                                      _display_encounter_);
        enctr_entries_offset = dongle_storage_num_encounters(&storage);
#ifdef MODE__TEST
        int err = 0;
        if (test_encounters < 1)
        {
            log_infof("FAILED: Encounter test. encounters logged in window: %d\n",
                      test_encounters);
            err++;
        }
        log_infof("Tests completed: status = %d\n", err);
        test_encounters = 0;
#endif
        log_info("*** End Report ***\n");
    }
}

static void _dongle_load_()
{
    dongle_storage_init(&storage);
#ifdef MODE__TEST
    config.id = TEST_DONGLE_ID;
    config.t_init = TEST_DONGLE_INIT_TIME;
#else
    dongle_storage_load_config(&storage, &config);
#endif
}

static void _dongle_init_()
{
    k_mutex_init(&dongle_mu);

    _dongle_load_();

    dongle_time = config.t_init;
    report_time = dongle_time;
    cur_id_idx = 0;
    epoch = 0;
    enctr_entries_offset = 0;

#ifdef MODE__TEST
    test_encounters = 0;
#endif

    k_timer_init(&kernel_time, NULL, NULL);

// Timer zero point
#define DUR K_MSEC(DONGLE_TIMER_RESOLUTION)
    k_timer_start(&kernel_time, DUR, DUR);
#undef DUR // Initial time
}

static void dongle_scan(void)
{

    _dongle_init_();

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
    }

    // Dongle Loop
    // timing and control logic is largely the same as the beacon
    // application. The main difference is that scanning does not
    // require restart for a new epoch.

    uint32_t timer_status = 0;

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
            log_infof("EPOCH STARTED: %u\n", epoch);
            // TODO: log time to flash
        }
        _dongle_report_();
        UNLOCK
    }
}
