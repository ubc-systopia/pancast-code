//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include "./beacon.h"

#define APPL_VERSION "0.1.1"

#define LOG_LEVEL__INFO
#define APPL__BEACON
#define MODE__STAT
#define MODE__TEST

#include <string.h>

#ifdef BEACON_PLATFORM__ZEPHYR
#include <zephyr.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <drivers/flash.h>
#else
#include "sl_bluetooth.h"
#endif

#include "../../common/src/pancast.h"
#include "../../common/src/util.h"
#include "../../common/src/log.h"
#include "../../common/src/test.h"

//
// ENTRY POINT
//

// Offset determines a safe point of read/write beyond the pages used by application
// binaries. For now, determined empirically by doing a compilation pass then adjusting
// the value
#define FLASH_OFFSET 0x31fff

#ifdef BEACON_PLATFORM__ZEPHYR
void main(void)
#else
void beacon_start()
#endif
{
    log_info("\r\n"), log_info("Starting Beacon...\r\n");
#ifdef MODE__STAT
    log_info("Statistics mode enabled\r\n");
#endif
    log_infof("Reporting every %d ms\r\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
#ifdef BEACON_PLATFORM__ZEPHYR
    int err = bt_enable(_beacon_broadcast_);
    if (err)
    {
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
static beacon_timer_t t_init;      // Beacon Clock Start
static key_size_t backend_pk_size; // size of backend public key
static pubkey_t backend_pk;        // Backend public key
static key_size_t beacon_sk_size;  // size of secret key
static beacon_sk_t beacon_sk;      // Secret Key

// Default Operation
static beacon_id_t beacon_id;                   // Beacon ID
static beacon_location_id_t beacon_location_id; // Location ID
static beacon_timer_t beacon_time;              // Beacon Clock
static beacon_eph_id_t beacon_eph_id;           // Ephemeral ID
static beacon_epoch_counter_t epoch;            // track the current time epoch
static beacon_timer_t cycles;                   // total number of updates.
#ifdef BEACON_PLATFORM__ZEPHYR
static struct k_timer kernel_time_lp; // low-precision kernel timer
static struct k_timer kernel_time_hp; // high-precision kernel timer
#endif
//
// Bluetooth
#ifdef BEACON_PLATFORM__ZEPHYR
static bt_data_t adv_res[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1)}; // Advertising response
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
#endif

//
// ROUTINES
//

static void _beacon_load_()
{
// Load data
#ifdef MODE__TEST
    beacon_id = TEST_BEACON_ID;
    beacon_location_id = TEST_BEACON_LOC_ID;
    t_init = TEST_BEACON_INIT_TIME;
    backend_pk_size = TEST_BEACON_BACKEND_KEY_SIZE;
    memcpy(&backend_pk, &TEST_BACKEND_PK, backend_pk_size);
    beacon_sk_size = TEST_BEACON_SK_SIZE;
    memcpy(&beacon_sk, &TEST_BEACON_SK, beacon_sk_size);
#else
    // Read data from flashed storage
    // Format matches the fixed structure which is also used as a protocol when appending non-app
    // data to the device image.
    struct device *flash = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
    off_t off = 0;
#define read(size, dst) (flash_read(flash, FLASH_OFFSET + off, dst, size), off += size)
    flash_read(flash, FLASH_OFFSET, &beacon_id, sizeof(beacon_id_t));
    read(sizeof(beacon_id_t), &beacon_id);
    read(sizeof(beacon_location_id_t), &beacon_location_id);
    read(sizeof(beacon_timer_t), &t_init);
    read(sizeof(key_size_t), &backend_pk_size);
    if (backend_pk_size > PK_MAX_SIZE)
    {
        log_errorf("Key size read for public key (%u bytes) is larger than max (%u)\r\n",
                   backend_pk_size, PK_MAX_SIZE);
    }
    read(backend_pk_size, &backend_pk);
    read(sizeof(key_size_t), &beacon_sk_size);
    if (beacon_sk_size > SK_MAX_SIZE)
    {
        log_errorf("Key size read for secret key (%u bytes) is larger than max (%u)\r\n",
                   beacon_sk_size, SK_MAX_SIZE);
    }
    read(beacon_sk_size, &beacon_sk);
#undef read
#endif
}

static void _beacon_info_()
{
    log_info("\r\n");
    log_info("Info: \r\n");
    log_infof("    Platform:                        %s\r\n", "Zephyr OS");
#ifndef BEACON_PLATFORM__ZEPHYR
#define UK "Unknown"
#define CONFIG_BOARD UK
#define CONFIG_BT_DEVICE_NAME UK
#endif
    log_infof("    Board:                           %s\r\n", CONFIG_BOARD);
    log_infof("    Bluetooth device name:           %s\r\n", CONFIG_BT_DEVICE_NAME);
    log_infof("    Application Version:             %s\r\n", APPL_VERSION);
    log_infof("    Beacon ID:                       %u\r\n", beacon_id);
    log_infof("    Location ID:                     %lu\r\n", (uint32_t)beacon_location_id);
    log_infof("    Initial clock:                   %u\r\n", t_init);
    log_infof("    Backend public key size:         %u bytes\r\n", backend_pk_size);
    log_infof("    Secret key size:                 %u bytes\r\n", beacon_sk_size);
    log_infof("    Timer Resolution:                %u ms\r\n", BEACON_TIMER_RESOLUTION);
    log_infof("    Epoch Length:                    %u ms\r\n", BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);
    log_infof("    Report Interval:                 %u ms\r\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
    log_infof("        Min:                         %x ms\r\n", BEACON_ADV_MIN_INTERVAL);
    log_infof("        Max:                         %x ms\r\n", BEACON_ADV_MAX_INTERVAL);
}

#ifdef MODE__STAT
static void _beacon_stats_()
{
    log_info("\r\n");
    log_info("Statistics: \r\n");
    log_infof("     Time since last report:         %d ms\r\n", beacon_time - stat_start);
    log_info("     Timer:\r\n");
    log_infof("         Start:                      %u\r\n", stat_start);
    log_infof("         End:                        %u\r\n", beacon_time);
    log_infof("     Cycles:                         %u\r\n", stat_cycles);
    log_infof("     Completed Epochs:               %u\r\n", stat_epochs);
}
#endif

static void _beacon_report_()
{
    if (beacon_time - report_time < BEACON_REPORT_INTERVAL)
    {
        return;
    }
    else
    {
        report_time = beacon_time;
        log_info("\r\n"), log_info("***          Begin Report          ***\r\n");
        _beacon_info_();
#ifdef MODE__STAT
        _beacon_stats_();
        stat_start = beacon_time;
        stat_cycles = 0;
        stat_epochs = 0;
#endif
        log_info("\r\n"), log_info("***          End Report            ***\r\n");
    }
}

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
    copy(&beacon_id, sizeof(beacon_id_t));
    copy(&beacon_location_id, sizeof(beacon_location_id_t));
    copy(&beacon_eph_id, sizeof(beacon_eph_id_t));
#undef copy
}

static void _beacon_encode_()
{
    // Load broadcast into bluetooth payload
    _encode_encounter_();
    print_bytes(payload.en_data.bytes, MAX_BROADCAST_SIZE, "adv_data pre-encode");
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
    add(&beacon_sk, beacon_sk_size);
    add(&beacon_location_id, sizeof(beacon_location_id_t));
    add(&epoch, sizeof(beacon_epoch_counter_t));
    // finalize and copy to id
    complete();
    memcpy(&beacon_eph_id, &d, BEACON_EPH_ID_HASH_LEN); // Little endian so these are the least significant
#undef complete
#undef add
#undef init
    print_bytes(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "new ephemeral id");
}

static void _beacon_init_()
{

    epoch = 0;
    cycles = 0;

    beacon_time = t_init;
    report_time = beacon_time;

#ifdef MODE__STAT
    stat_epochs = 0;
#endif

    _beacon_info_();

// Timer Start
#ifdef BEACON_PLATFORM__ZEPHYR
    k_timer_init(&kernel_time_lp, NULL, NULL);
    k_timer_init(&kernel_time_hp, NULL, NULL);
#define DUR_LP K_MSEC(BEACON_TIMER_RESOLUTION)
#define DUR_HP K_MSEC(1)
    k_timer_start(&kernel_time_lp, DUR_LP, DUR_LP);
    k_timer_start(&kernel_time_hp, DUR_HP, DUR_HP);
#undef DUR_HP
#undef DUR_LP
#endif
}

static void _beacon_epoch_()
{
    static beacon_epoch_counter_t old_epoch;
    old_epoch = epoch;
    epoch = epoch_i(beacon_time, t_init);
    if (!cycles || epoch != old_epoch)
    {
        log_debugf("EPOCH STARTED: %u\r\n", epoch);
        // When a new epoch has started, generate a new ephemeral id
        _gen_ephid_();
        if (epoch != old_epoch)
        {
#ifdef MODE__STAT
            stat_epochs++;
#endif
        }
        // TODO: log time to flash
    }
}

int _set_adv_data_()
{
#ifdef BEACON_PLATFORM__GECKO
    log_debug("Setting legacy adv data...\r\n");
    print_bytes(payload.en_data.bytes, MAX_BROADCAST_SIZE, "adv_data");
    sl_status_t sc = sl_bt_advertiser_set_data(legacy_set_handle,
                                               0, 31,
                                               payload.en_data.bytes);
    if (sc != 0)
    {
        log_errorf("Error, sc: 0x%lx\r\n", sc);
        return -1;
    }
    log_debug("Success!\r\n");
#endif
    return 0;
}

static int _beacon_advertise_()
{
    int err = 0;
#ifdef BEACON_PLATFORM__ZEPHYR
    // Start advertising
    err = bt_le_adv_start(
        BT_LE_ADV_PARAM(
            BT_LE_ADV_OPT_USE_IDENTITY, // use random identity address
            BEACON_ADV_MIN_INTERVAL,
            BEACON_ADV_MAX_INTERVAL,
            NULL // undirected advertising
            ),
        payload.bt_data, ARRAY_SIZE(payload.bt_data),
        adv_res, ARRAY_SIZE(adv_res));
    if (err)
    {
        log_errorf("Advertising failed to start (err %d)\r\n", err);
    }
    else
    {
        // obtain and report adverisement address
        char addr_s[BT_ADDR_LE_STR_LEN];
        bt_addr_le_t addr = {0};
        size_t count = 1;

        bt_id_get(&addr, &count);
        bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

        log_debugf("advertising started with address %s\r\n", addr_s);
    }
#else
    sl_status_t sc;
    log_debug("Creating legacy advertising set\r\n");
    sc = sl_bt_advertiser_create_set(&legacy_set_handle);
    if (sc != 0)
    {
        log_errorf("Error, sc: 0x%lx\r\n", sc);
        return -1;
    }
    printf("Starting legacy advertising...\r\n");
    // Set advertising interval to 100ms.
    sc = sl_bt_advertiser_set_timing(
        legacy_set_handle,
        BEACON_ADV_MIN_INTERVAL, // min. adv. interval (milliseconds * 1.6)
        BEACON_ADV_MAX_INTERVAL, // max. adv. interval (milliseconds * 1.6) next: 0x4000
        0,                       // adv. duration, 0 for continuous advertising
        0);                      // max. num. adv. events
    if (sc != 0)
    {
        log_errorf("Error, sc: 0x%lx\r\n", sc);
        return -1;
    }
    // Start legacy advertising
    sc = sl_bt_advertiser_start(
        legacy_set_handle,
        advertiser_user_data,
        advertiser_non_connectable);
    if (sc != 0)
    {
        log_errorf("Error starting advertising, sc: 0x%lx\r\n", sc);
        return -1;
    }
    err = _set_adv_data_();
#endif
    return err;
}

static int _beacon_pause_()
{
    // stop current advertising cycle
#ifdef BEACON_PLATFORM__ZEPHYR
    int err = bt_le_adv_stop();
    if (err)
    {
        log_errorf("Advertising failed to stop (err %d)\r\n", err);
        return err;
    }
    log_debug("advertising stopped\r\n");
#endif
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

    uint32_t lp_timer_status = 0, hp_timer_status = 0;

    int err;
    while (!err)
    {
        // get most updated time
        // Low-precision timer is synced, so accumulate status here
        lp_timer_status += k_timer_status_get(&kernel_time_lp);

        // update beacon clock using kernel. The addition is the number of
        // periods elapsed in the internal timer
        beacon_clock_increment(lp_timer_status);

        // high-precision collects the raw number of expirations
        hp_timer_status = k_timer_status_get(&kernel_time_hp);

        // Wait for a clock update, this blocks until the internal timer
        // period expires, indicating that at least one unit of relevant beacon
        // time has elapsed. timer status is reset here
        lp_timer_status = k_timer_status_sync(&kernel_time_lp);
    }
}
#endif

// Primary broadcasting routine
// Non-zero argument indicates an error setting up the procedure for BT advertising
#ifdef BEACON_PLATFORM__ZEPHYR
static void _beacon_broadcast_(int err)
{
    // check initialization
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\r\n", err);
        return;
    }

    log_info("Bluetooth initialized - starting broadcast\r\n");
#else
void beacon_broadcast()
{
    log_info("Starting broadcast\r\n");
#endif

    _beacon_load_(), _beacon_init_();

#ifdef BEACON_PLATFORM__ZEPHYR
    beacon_loop();
#else
    int err = _beacon_advertise_();
    if (err)
    {
        return;
    }
    _beacon_update_();
#endif
}

#undef MODE__STAT
#undef APPL__BEACON
#undef LOG_LEVEL__INFO
#undef APPL_VERSION
#undef MODE__TEST