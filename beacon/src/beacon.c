//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include "./beacon.h"

#define APPL_VERSION "0.1.1"

#define MODE__STAT
#define MODE__TEST_CONFIG

#define LOG_LEVEL__DEBUG

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
#include "app_log.h"
#endif

#include "storage.h"

#include "../../common/src/pancast.h"
#include "../../common/src/util.h"
#include "../../common/src/log.h"
#include "../../common/src/test.h"

//
// ENTRY POINT
//

#ifdef BEACON_PLATFORM__ZEPHYR
void main(void)
#else
void beacon_start()
#endif
{
    log_info("\r\n");
    log_info("Starting Beacon...\r\n");
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
static struct k_timer kernel_time_alternater; // low-precision kernel timer used to alternate packet generation
#endif
//
// Bluetooth
#ifdef BEACON_PLATFORM__ZEPHYR
static bt_data_t adv_res[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1)}; // Advertising response
// #ifdef BEACON_GAEN_ENABLED
static int *transmit_state = 0; // keeps track of what we're advertising rn
// fields for constructing packet
static uint8_t flags_type = 0x01;
static uint8_t service_UUID_type = 0x03;
static uint8_t service_data_type = 0x16;
// #endif
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
#else
    beacon_storage_load_config(&storage, &config);
#endif
}

void _beacon_info_()
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
    log_infof("    Beacon ID:                       %u\r\n", config.beacon_id);
    log_infof("    Location ID:                     %lu\r\n", (uint32_t)config.beacon_location_id);
    log_infof("    Initial clock:                   %u\r\n", config.t_init);
    log_infof("    Backend public key size:         %u bytes\r\n", config.backend_pk_size);
    log_infof("    Secret key size:                 %u bytes\r\n", config.beacon_sk_size);
    log_infof("    Timer Resolution:                %u ms\r\n", BEACON_TIMER_RESOLUTION);
    log_infof("    Epoch Length:                    %u ms\r\n", BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);
    log_infof("    Report Interval:                 %u ms\r\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
    log_info("    Advertising Interval:                  \r\n");
    log_infof("        Min:                         %x ms\r\n", BEACON_ADV_MIN_INTERVAL);
    log_infof("        Max:                         %x ms\r\n", BEACON_ADV_MAX_INTERVAL);
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
}

static void _beacon_stats_()
{
    log_info("\r\n");
    log_info("Statistics: \r\n");
    log_infof("     Time since last report:         %d ms\r\n", stats.duration);
    log_info("     Timer:\r\n");
    log_infof("         Start:                      %u\r\n", stats.start);
    log_infof("         End:                        %u\r\n", stats.end);
    log_infof("     Cycles:                         %u\r\n", stats.cycles);
    log_infof("     Completed Epochs:               %u\r\n", stats.epochs);

    beacon_storage_save_stat(&storage, &stats, sizeof(beacon_stats_t));
    beacon_stats_init();
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
        log_info("\r\n");
        log_info("***          Begin Report          ***\r\n");
        _beacon_info_();
#ifdef MODE__STAT
        beacon_stat_update();
        _beacon_stats_();
        stat_start = beacon_time;
        stat_cycles = 0;
        stat_epochs = 0;
#endif
        log_info("\r\n");
        log_info("***          End Report            ***\r\n");
    }
}

// Intermediary transformer to create a well-formed BT data type
// for using the high-level APIs. Becomes obsolete once advertising
// routine supports a full raw payload
static void _form_payload_()
{
    log_debug("_form_payload_\r\n");
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
    log_debug("_encode_encounter_\r\n");
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
}

static void _beacon_encode_()
{
    log_debug("_beacon_encode_\r\n");
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
    print_bytes(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "new ephemeral id");
}

static int _set_adv_data_gaen_(bt_data_t *flags_param, bt_data_t *uuid, bt_data_t *data)
{
#ifdef BEACON_PLATFORM__ZEPHYR
    char ENS_identifier[2] = "\x6f\xfd";
    char rolling_proximity_identifier[16] = "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
    char associated_encrypted_metadata[4] = "\xaa\xaa\xaa\xaa";
    char service_data_internals[22];
    for (int i = 0; i < 22; i++)
    {
        if (i < 2)
        {
            service_data_internals[i] = ENS_identifier[i];
        }
        else if (i >= 18)
        {
            service_data_internals[i] = associated_encrypted_metadata[i - 18];
        }
        else
        {
            service_data_internals[i] = rolling_proximity_identifier[i - 2];
        }
    }
    bt_data_t flags = BT_DATA_BYTES(flags_type, 0x1a);
    bt_data_t serviceUUID = BT_DATA_BYTES(service_UUID_type, 0x6f, 0xfd);
    bt_data_t serviceData = BT_DATA(service_data_type, service_data_internals, ARRAY_SIZE(service_data_internals));
    memcpy(flags_param, &flags, sizeof(bt_data_t));
    memcpy(uuid, &serviceUUID, sizeof(bt_data_t));
    memcpy(data, &serviceData, sizeof(bt_data_t));
    return 0;
#endif
    return -1;
}

static int _alternate_advertisement_content_()
{
    int err = bt_le_adv_stop();
    if (err)
    {
        log_errorf("Advertising failed to stop (err %d)\r\n", err);
        return err;
    }
    if (*transmit_state == 0)
    {
        // currently transmitting pancast data, switch to gaen data
        bt_data_t data[3];
        err = _set_adv_data_gaen_(&data[0], &data[1], &data[2]);
        if (err)
        {
            log_error("Failed to obtain gaen advertisement data");
            return -1;
        }
        err = bt_le_adv_start(
            BT_LE_ADV_PARAM(
                BT_LE_ADV_OPT_USE_IDENTITY, // use random identity address
                BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL,
                NULL // undirected advertising
                ),
            data, ARRAY_SIZE(data), NULL, 0); // has 3 fields
        if (err)
        {
            log_errorf("Advertising failed to stop (err %d)\r\n", err);
            return err;
        }
        *transmit_state = 1;
    }
    else
    {
        // currently transmitting gaen data, switch to pancast data
        err = bt_le_adv_start(
            BT_LE_ADV_PARAM(
                BT_LE_ADV_OPT_USE_IDENTITY,
                BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL,
                NULL),
            payload.bt_data, ARRAY_SIZE(payload.bt_data),
            adv_res, ARRAY_SIZE(adv_res));
        if (err)
        {
            log_errorf("Second attempt at advertising failed to start (err %d)\r\n", err);
            return -1;
        }
        *transmit_state = 0;
    }
    return 0;
}

static void _beacon_init_()
{

    epoch = 0;
    cycles = 0;

    beacon_time = config.t_init;
    report_time = beacon_time;

#ifdef MODE__STAT
    stat_epochs = 0;
    beacon_storage_read_stat(&storage, &stats, sizeof(beacon_stats_t));
    if (!stats.storage_checksum)
    {
        log_info("Existing Statistics Found\r\n");
        _beacon_stats_();
    }
    else
    {
        beacon_stats_init();
    }
#endif
    _beacon_info_();

// Timer Start
#ifdef BEACON_PLATFORM__ZEPHYR
    k_timer_init(&kernel_time_lp, NULL, NULL);
    k_timer_init(&kernel_time_hp, NULL, NULL);
    k_timer_init(&kernel_time_alternater, _alternate_advertisement_content_, NULL);
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
    log_debug("_beacon_epoch_\r\n");
    static beacon_epoch_counter_t old_epoch;
    old_epoch = epoch;
    epoch = epoch_i(beacon_time, config.t_init);
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
    err = bt_le_adv_start(
        BT_LE_ADV_PARAM(
            BT_LE_ADV_OPT_USE_IDENTITY,
            BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL,
            NULL),
        payload.bt_data, ARRAY_SIZE(payload.bt_data),
        adv_res, ARRAY_SIZE(adv_res));
    if (err)
    {
        log_errorf("Second attempt at advertising failed to start (err %d)\r\n", err);
        return -1;
    }
    else
    {
        // obtain and report advertisement address
        char addr_s[BT_ADDR_LE_STR_LEN];
        bt_addr_le_t addr = {0};
        size_t count = 1;

        bt_id_get(&addr, &count);
        bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

        log_debugf("advertising started with address %s\r\n", addr_s);

        k_timer_start(&kernel_time_alternater, K_SECONDS(1), K_SECONDS(1));
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
    if (!err)
    {
        log_info("Success!\r\n");
    }
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
    log_debug("_beacon_update_\r\n");
    _beacon_epoch_();
    _beacon_encode_();
    _set_adv_data_();
}

int beacon_clock_increment(beacon_timer_t time)
{
    log_debug("beacon_clock_increment\r\n");
    beacon_time += time;
    log_debugf("beacon timer: %u\r\n", beacon_time);
    _beacon_update_();
    _beacon_pause_();
    return 0;
}

#ifdef BEACON_PLATFORM__ZEPHYR
int beacon_loop()
{
    log_debug("beacon_loop\r\n");
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
    log_debug("_beacon_broadcast_\r\n");
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
    // beacon_loop(); // can't get this to work somehow :/
    err = _beacon_advertise_();
    if (err)
    {
        return;
    }
    _beacon_update_();
#else

#endif
}

#undef MODE__STAT
#undef APPL_VERSION
#undef MODE__TEST_CONFIG
