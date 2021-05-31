//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#define LOG_LEVEL__INFO
#define MODE__STAT

#include <zephyr.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <tinycrypt/sha256.h>

#include "../../common/src/pancast.h"
#include "../../common/src/util.h"
#include "../../common/src/log.h"
#include "../../common/src/test.h"

#define BEACON_REPORT_INTERVAL 30

// Advertising interval settings
// Zephyr-recommended values are used
#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1

// sha256-hashing

typedef struct tc_sha256_state_struct hash_t;

typedef struct {
	uint8_t bytes[TC_SHA256_DIGEST_SIZE];
} digest_t;


static int beacon_gen_id(beacon_eph_id_t *id,
				beacon_sk_t *sk, beacon_location_id_t *loc, beacon_epoch_counter_t *i)
{
	hash_t h;
#define init() 			tc_sha256_init(&h)
#define add(data,size) 	tc_sha256_update(&h, (uint8_t*)data, size)
#define complete(d)		tc_sha256_final(d.bytes, &h)
// Initialize hash
	init();
// Add relevant data
	add(sk, 	BEACON_SK_SIZE);
	add(loc, 	sizeof(beacon_location_id_t));
	add(i, 		sizeof(beacon_epoch_counter_t));
// finalize and copy to id
	digest_t d;
	complete(d);
	memcpy(id, &d, BEACON_EPH_ID_HASH_LEN); // Little endian so these are the least significant
#undef complete
#undef add
#undef init
	return 0;
}


typedef struct bt_data bt_data_t;

typedef union {
    encounter_broadcast_raw_t en_data;
    bt_data_t bt_data[1];
} bt_wrapper_t;

// pack a raw byte payload by copying from the high-level type
// order is important here so as to avoid unaligned access on the
// receiver side
static int encode_encounter(encounter_broadcast_raw_t *raw, encounter_broadcast_t *dat)
{
    uint8_t *dst = (uint8_t*) raw;
    size_t pos = 0;
#define copy(src, size) memcpy(dst + pos, src, size); pos += size
    copy(dat->t, sizeof(beacon_timer_t));
    copy(dat->b, sizeof(beacon_id_t));
    copy(dat->loc, sizeof(beacon_location_id_t));
    copy(dat->eph, sizeof(beacon_eph_id_t));
#undef copy
    return pos;
}

// Intermediary transformer to create a well-formed BT data type
// for using the high-level APIs. Becomes obsolete once advertising
// routine supports a full raw payload
static int form_payload(bt_wrapper_t *d)
{
    const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
#define bt (d->bt_data)
    uint8_t tmp = bt->data_len;
    bt -> data_len = len;
#define en (d->en_data)
    en.bytes[MAX_BROADCAST_SIZE - 1] = tmp;
#undef en
	static uint8_t of[MAX_BROADCAST_SIZE - 2];
	memcpy(of, ((uint8_t*) bt) + 2, MAX_BROADCAST_SIZE - 2);
    bt -> data = (uint8_t*) &of;
#undef bt
    return 0;
}


// Scan Response
static const bt_data_t adv_res[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

// Primary broadcasting routine
// Non-zero argument indicates an error setting up the procedure for BT advertising
static void beacon_broadcast(int err)
{
// check initialization
	if (err) {
		log_errorf("Bluetooth init failed (err %d)\n", err);
		return;
	}

	log_info("Bluetooth initialized\n");

// Load data
// this is a placeholder for flash load
	beacon_id_t beacon_id = TEST_BEACON_ID;							// ID
	beacon_location_id_t beacon_location_id = TEST_BEACON_LOC_ID;	// Location
	beacon_timer_t t_init = TEST_BEACON_INIT_TIME; 					// Initial time
    beacon_sk_t beacon_sk = TEST_BEACON_SK;                         // secret key

// ephemeral id container
	beacon_eph_id_t beacon_eph_id;
// beacon timer
	beacon_timer_t beacon_time = t_init;

	beacon_timer_t report_time = beacon_time;

// store references in a single struct
	encounter_broadcast_t bc;

	bc.b = &beacon_id;
	bc.loc = &beacon_location_id;
	bc.t = &beacon_time;
	bc.eph = &beacon_eph_id;

// container for actual blutooth payload
    bt_wrapper_t payload;

// track the current time epoch
	beacon_epoch_counter_t epoch = 0;

// beacon kernel timer
	struct k_timer kernel_time_lp;
	k_timer_init(&kernel_time_lp, NULL, NULL);
	struct k_timer kernel_time_hp;
	k_timer_init(&kernel_time_hp, NULL, NULL);

// total number of updates. This is guaranteed to not exceed the
// scale of the beacon timer
	beacon_timer_t cycles = 0;

// BEACON BROADCASTING

	log_info("starting broadcast\n");

// 1. Timer zero point
#define DUR_LP K_MSEC(BEACON_TIMER_RESOLUTION)
#define DUR_HP K_MSEC(1)
	k_timer_start(&kernel_time_lp, DUR_LP, DUR_LP);
	k_timer_start(&kernel_time_hp, DUR_HP, DUR_HP);
#undef DUR_HP
#undef DUR_LP

	uint32_t lp_timer_status = 0;
	uint32_t hp_timer_status = 0;

#ifdef MODE__STAT
// Statistics data
    uint32_t stat_timer = 0;
    beacon_timer_t stat_start;
	beacon_timer_t stat_cycles;
	beacon_timer_t stat_epochs = 0;
#endif

#define BEACON_STATS \
    log_info(   "Statistics: \n");                                                                                  \
    log_infof(  "     Time since last report:         %d ms\n", stat_timer);                            \
    log_infof(  "     Timer:\n"         \
                "         Start:                      %u\n" \
                "         End:                        %u\n", stat_start, beacon_time); \
    log_infof(  "     Cycles:                         %u\n", stat_cycles); \
    log_infof(  "     Completed Epochs:               %u\n", stat_epochs);

#define BEACON_INFO \
    log_info("Info: \n");                                                                                           \
    log_infof("    Board:                           %s\n", CONFIG_BOARD);                                           \
    log_infof("    Beacon ID:                       %u\n", beacon_id);                                              \
    log_infof("    Timer Resolution:                %u ms\n", BEACON_TIMER_RESOLUTION);                             \
    log_infof("    Epoch Length:                    %u ms\n", BEACON_EPOCH_LENGTH * BEACON_TIMER_RESOLUTION);                                    \
    log_infof("    Report Interval:                 %u ms\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);                                    \
    log_infof("    Advertising Interval: \n"                                                                        \
              "        Min:                         %u ms\n"                                                        \
              "        Max:                         %u ms\n", BEACON_ADV_MIN_INTERVAL, BEACON_ADV_MAX_INTERVAL);


    BEACON_INFO

// 2. Main loop, this is primarily controlled by timing functions
// and terminates only in the event of an error
	while (!err) {

// get most updated time
// Low-precision timer is synced, so accumulate status here
		lp_timer_status += k_timer_status_get(&kernel_time_lp);

// update beacon clock using kernel. The addition is the number of
// periods elapsed in the internal timer
		beacon_time += lp_timer_status;
		static beacon_epoch_counter_t old_epoch;
		old_epoch = epoch;
		epoch = epoch_i(beacon_time, t_init);
		if (!cycles || epoch != old_epoch) {
			log_debugf("EPOCH STARTED: %u\n", epoch);
// When a new epoch has started, generate a new ephemeral id
			beacon_gen_id(&beacon_eph_id, &beacon_sk, bc.loc, &epoch);
			print_bytes(beacon_eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "new ephemeral id");
			if (epoch != old_epoch) {
#ifdef MODE__STAT
				stat_epochs ++;
#endif
			}
			// TODO: log time to flash
		}
		log_debugf("beacon timer: %u\n", beacon_time);

// Load broadcast into bluetooth payload
		encode_encounter(&payload.en_data, &bc);

		form_payload(&payload);

// Start advertising
		err = bt_le_adv_start(
						BT_LE_ADV_PARAM(
                            BT_LE_ADV_OPT_USE_IDENTITY,     // use random identity address
                            BEACON_ADV_MIN_INTERVAL,
                            BEACON_ADV_MAX_INTERVAL,
                            NULL                            // undirected advertising
                        ),
						payload.bt_data, ARRAY_SIZE(payload.bt_data),
						adv_res, ARRAY_SIZE(adv_res));
		if (err) {
			log_errorf("Advertising failed to start (err %d)\n", err);
		} else {
// obtain and report adverisement address
			char addr_s[BT_ADDR_LE_STR_LEN];
			bt_addr_le_t addr = {0};
			size_t count = 1;

			bt_id_get(&addr, &count);
			bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

			log_debugf("advertising started with address %s\n", addr_s);
		}

// high-precision collects the raw number of expirations
		hp_timer_status = k_timer_status_get(&kernel_time_hp);


// Wait for a clock update, this blocks until the internal timer
// period expires, indicating that at least one unit of relevant beacon
// time has elapsed. timer status is reset here
		lp_timer_status = k_timer_status_sync(&kernel_time_lp);

// stop current advertising cycle
		err = bt_le_adv_stop();
		if (err) {
			log_errorf("Advertising failed to stop (err %d)\n", err);
			return;
		}
		cycles++;
#ifdef MODE__STAT
		stat_cycles++;
// STATISTICS
        if (!stat_timer) {
			stat_start = beacon_time;
			stat_cycles = 0;
			stat_epochs = 0;
        }
        stat_timer += hp_timer_status;
#endif
        if (beacon_time - report_time >= BEACON_REPORT_INTERVAL) {
            report_time = beacon_time;
			log_infof("*** Begin Report for %s ***\n", CONFIG_BT_DEVICE_NAME);
            BEACON_INFO
#ifdef MODE__STAT
            BEACON_STATS
            stat_timer = 0;
#endif
            log_info(   "*** End Report ***\n");
        }
		log_debug("advertising stopped\n");
	}
}

void main(void)
{
	log_infof("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);
#ifdef MODE__STAT
    log_info("Statistics mode enabled\n");
#endif
    log_infof("Reporting every %d ms\n", BEACON_REPORT_INTERVAL * BEACON_TIMER_RESOLUTION);
    int err = bt_enable(beacon_broadcast);
    if (err) {
        log_errorf("Bluetooth Enable Failure: error code = %d\n", err);
    }
}
