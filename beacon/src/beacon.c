//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#define APPL_VERSION "0.1.0"

#define LOG_LEVEL__INFO
#define APPL__BEACON
#define MODE__STAT
#define MODE__TEST

#include <zephyr.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <drivers/flash.h>

#include "./beacon.h"

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
#define FLASH_OFFSET 0x12000

#ifdef APPL__BEACON
void main(void)
#else
void _beacon_main_()
#endif
{
    log_info("\n"), log_info("Starting Beacon...\n");
#ifdef MODE__STAT
    log_info("Statistics mode enabled\n");
#endif
    log_infof("Reporting every %d ms\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
    int err = bt_enable(_beacon_broadcast_);
    if (err)
    {
        log_errorf("Bluetooth Enable Failure: error code = %d\n", err);
    }
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
static struct k_timer kernel_time_lp;           // low-precision kernel timer
static struct k_timer kernel_time_hp;           // high-precision kernel timer
//
// Bluetooth
static bt_wrapper_t payload; // container for actual blutooth payload
static bt_data_t adv_res[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1)}; // Advertising response
//
// Reporting
static beacon_timer_t report_time; // Report tracking clock
//
// Statistics
#ifdef MODE__STAT
static uint32_t stat_timer;
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
        log_errorf("Key size read for public key (%u bytes) is larger than max (%u)\n",
                   backend_pk_size, PK_MAX_SIZE);
    }
    read(backend_pk_size, &backend_pk);
    read(sizeof(key_size_t), &beacon_sk_size);
    if (beacon_sk_size > SK_MAX_SIZE)
    {
        log_errorf("Key size read for secret key (%u bytes) is larger than max (%u)\n",
                   beacon_sk_size, SK_MAX_SIZE);
    }
    read(beacon_sk_size, &beacon_sk);
#undef read
#endif
}

static void _beacon_info_()
{
    log_info("\n");
    log_info("Info: \n");
    log_infof("    Platform:                        %s\n", "Zephyr OS");
    log_infof("    Board:                           %s\n", CONFIG_BOARD);
    log_infof("    Application Version:             %s\n", APPL_VERSION);
    log_infof("    Bluetooth device name:           %s\n", CONFIG_BT_DEVICE_NAME);
    log_infof("    Beacon ID:                       %u\n", beacon_id);
    log_infof("    Location ID:                     %llu\n", beacon_location_id);
    log_infof("    Initial clock:                   %u\n", t_init);
    log_infof("    Backend public key size:         %u bytes\n", backend_pk_size);
    log_infof("    Secret key size:                 %u bytes\n", beacon_sk_size);
    log_infof("    Timer Resolution:                %u ms\n", BEACON_TIMER_RESOLUTION);
    log_infof("    Epoch Length:                    %u ms\n", BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);
    log_infof("    Report Interval:                 %u ms\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
    log_info("    Advertising Interval:                \n");
    log_infof("        Min:                         %x ms\n", BEACON_ADV_MIN_INTERVAL);
    log_infof("        Max:                         %x ms\n", BEACON_ADV_MAX_INTERVAL);
}

#ifdef MODE__STAT
static void _beacon_stats_()
{
    log_info("\n");
    log_info("Statistics: \n");
    log_infof("     Time since last report:         %d ms\n", stat_timer);
    log_info("     Timer:\n");
    log_infof("         Start:                      %u\n", stat_start);
    log_infof("         End:                        %u\n", beacon_time);
    log_infof("     Cycles:                         %u\n", stat_cycles);
    log_infof("     Completed Epochs:               %u\n", stat_epochs);
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
        log_info("\n"), log_info("***          Begin Report          ***\n");
        _beacon_info_();
#ifdef MODE__STAT
        _beacon_stats_();
        stat_timer = 0;
#endif
        log_info("\n"), log_info("***          End Report            ***\n");
    }
}

// Intermediary transformer to create a well-formed BT data type
// for using the high-level APIs. Becomes obsolete once advertising
// routine supports a full raw payload
static void _form_payload_()
{
    const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
#define bt (payload.bt_data)
    uint8_t tmp = bt->data_len;
    bt->data_len = len;
#define en (payload.en_data)
    en.bytes[MAX_BROADCAST_SIZE - 1] = tmp;
#undef en
    static uint8_t of[MAX_BROADCAST_SIZE - 2];
    memcpy(of, ((uint8_t *)bt) + 2, MAX_BROADCAST_SIZE - 2);
    bt->data = (uint8_t *)&of;
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
    _encode_encounter_(), _form_payload_();
}

static void _gen_ephid_()
{
    hash_t h;
#define init() tc_sha256_init(&h)
#define add(data, size) tc_sha256_update(&h, (uint8_t *)data, size)
#define complete(d) tc_sha256_final(d.bytes, &h)
    // Initialize hash
    init();
    // Add relevant data
    add(&beacon_sk, beacon_sk_size);
    add(&beacon_location_id, sizeof(beacon_location_id_t));
    add(&epoch, sizeof(beacon_epoch_counter_t));
    // finalize and copy to id
    digest_t d;
    complete(d);
    memcpy(&beacon_eph_id, &d, BEACON_EPH_ID_HASH_LEN); // Little endian so these are the least significant
#undef complete
#undef add
#undef init
    print_bytes(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "new ephemeral id");
}

static void _beacon_init_()
{

    k_timer_init(&kernel_time_lp, NULL, NULL);
    k_timer_init(&kernel_time_hp, NULL, NULL);

    epoch = 0;
    cycles = 0;

    beacon_time = t_init;
    report_time = beacon_time;

#ifdef MODE__STAT
    stat_timer = 0;
    stat_epochs = 0;
#endif

    _beacon_info_();

// Timer Start
#define DUR_LP K_MSEC(BEACON_TIMER_RESOLUTION)
#define DUR_HP K_MSEC(1)
    k_timer_start(&kernel_time_lp, DUR_LP, DUR_LP);
    k_timer_start(&kernel_time_hp, DUR_HP, DUR_HP);
#undef DUR_HP
#undef DUR_LP
}

static void _beacon_epoch_()
{
    static beacon_epoch_counter_t old_epoch;
    old_epoch = epoch;
    epoch = epoch_i(beacon_time, t_init);
    if (!cycles || epoch != old_epoch)
    {
        log_debugf("EPOCH STARTED: %u\n", epoch);
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

static int _beacon_advertise_()
{
    int err = 0;
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
        log_errorf("Advertising failed to start (err %d)\n", err);
    }
    else
    {
        // obtain and report adverisement address
        char addr_s[BT_ADDR_LE_STR_LEN];
        bt_addr_le_t addr = {0};
        size_t count = 1;

        bt_id_get(&addr, &count);
        bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

        log_debugf("advertising started with address %s\n", addr_s);
    }
    return err;
}

static int _beacon_pause_(uint32_t *hp_timer_status)
{
    // stop current advertising cycle
    int err = bt_le_adv_stop();
    if (err)
    {
        log_errorf("Advertising failed to stop (err %d)\n", err);
        return err;
    }
    cycles++;
#ifdef MODE__STAT
    stat_cycles++;
    // STATISTICS
    if (!stat_timer)
    {
        stat_start = beacon_time;
        stat_cycles = 0;
        stat_epochs = 0;
    }
    stat_timer += *hp_timer_status;
#endif
    _beacon_report_();
    log_debug("advertising stopped\n");
    return err;
}

// Primary broadcasting routine
// Non-zero argument indicates an error setting up the procedure for BT advertising
static void _beacon_broadcast_(int err)
{
    // check initialization
    if (err)
    {
        log_errorf("Bluetooth init failed (err %d)\n", err);
        return;
    }

    log_info("Bluetooth initialized - starting broadcast\n");

    _beacon_load_(), _beacon_init_();

    uint32_t lp_timer_status = 0, hp_timer_status = 0;

    while (!err)
    {
        // get most updated time
        // Low-precision timer is synced, so accumulate status here
        lp_timer_status += k_timer_status_get(&kernel_time_lp);

        // update beacon clock using kernel. The addition is the number of
        // periods elapsed in the internal timer
        beacon_time += lp_timer_status;
        log_debugf("beacon timer: %u\n", beacon_time);

        _beacon_epoch_();
        _beacon_encode_();

        err = _beacon_advertise_();

        // high-precision collects the raw number of expirations
        hp_timer_status = k_timer_status_get(&kernel_time_hp);

        // Wait for a clock update, this blocks until the internal timer
        // period expires, indicating that at least one unit of relevant beacon
        // time has elapsed. timer status is reset here
        lp_timer_status = k_timer_status_sync(&kernel_time_lp);

        _beacon_pause_(&hp_timer_status);
    }
}

#undef MODE__STAT
#undef APPL__BEACON
#undef LOG_LEVEL__INFO
#undef APPL_VERSION
#undef MODE__TEST