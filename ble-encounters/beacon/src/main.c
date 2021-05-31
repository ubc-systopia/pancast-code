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

#define BEACON_REPORT_INTERVAL 30

// Advertising interval settings
// Zephyr-recommended values are used
#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1

// secret key for development
static beacon_sk_t BEACON_SK = {{
0xcb , 0x43 , 0xf7 , 0x56 , 0x16 , 0x25 , 0xb3 , 0xd0 , 0xd0 , 0xbe ,
0xad , 0xf4 , 0xca , 0x6c , 0xa6 , 0xd9 , 0x00 , 0xfd , 0x72 , 0xb2 ,
0x1f , 0xb5 , 0xd8 , 0x4e , 0xea , 0xc3 , 0x3f , 0x19 , 0x77 , 0x8f ,
0x05 , 0xf5 , 0x05 , 0x98 , 0x17 , 0x19 , 0xc8 , 0xfa , 0x49 , 0x9b ,
0x71 , 0xd4 , 0x04 , 0xea , 0x45 , 0x8f , 0xe2 , 0x94 , 0x2e , 0x5b ,
0x61 , 0xdc , 0x96 , 0xb3 , 0x6d , 0xae , 0xa1 , 0x9b , 0x61 , 0x0d ,
0x10 , 0x8f , 0xf1 , 0x4b , 0x83 , 0x91 , 0x74 , 0x57 , 0xf5 , 0x40 ,
0x1a , 0x36 , 0x3d , 0xe6 , 0x5d , 0x41 , 0x2d , 0x67 , 0xf3 , 0xea ,
0x57 , 0x31 , 0x1a , 0x4e , 0x77 , 0xf5 , 0x9d , 0xa1 , 0x25 , 0xdf ,
0xbb , 0xc2 , 0x60 , 0xb7 , 0x7f , 0xf0 , 0x77 , 0x05 , 0xe9 , 0xe4 ,
0x05 , 0x7e , 0x10 , 0x32 , 0x8c , 0xed , 0x1b , 0x77 , 0xac , 0x5c ,
0x17 , 0x11 , 0x49 , 0xbe , 0xfd , 0x28 , 0x83 , 0x99 , 0xfe , 0xf1 ,
0x32 , 0xc3 , 0x2e , 0x7a , 0x8a , 0x55 , 0xa5 , 0x8f , 0xfe , 0xd5 ,
0x05 , 0x00 , 0x09 , 0x51 , 0x18 , 0x90 , 0x3b , 0xa9 , 0x19 , 0x62 ,
0x65 , 0x57 , 0x97 , 0x1a , 0x49 , 0x24 , 0x8c , 0x1e , 0xb9 , 0xe4 ,
0xa8 , 0x19 , 0x04 , 0x2a , 0xa1 , 0x34 , 0xe2 , 0x9e , 0xd3 , 0x73 ,
0x6b , 0x6d , 0x61 , 0xf2 , 0xa2 , 0x04 , 0x66 , 0x23 , 0x06 , 0x11 ,
0xbf , 0xb5 , 0x34 , 0x5e , 0xf9 , 0xb8 , 0x72 , 0xf7 , 0xf6 , 0x00 ,
0x79 , 0x16 , 0x50 , 0xa4 , 0xee , 0x79 , 0x04 , 0xae , 0xe0 , 0x0f ,
0x71 , 0xd7 , 0x8e , 0x71 , 0x6d , 0x7c , 0x11 , 0x72 , 0x7a , 0xfa ,
0x4a , 0x19 , 0x09 , 0xb5 , 0x95 , 0x9c , 0x98 , 0x2f , 0xda , 0x01 ,
0xd5 , 0x0f , 0xc9 , 0xf8 , 0x87 , 0x39 , 0xaa , 0x0f , 0xcf , 0xb6 ,
0x38 , 0xfa , 0xb3 , 0x3b , 0x6e , 0xcc , 0xc3 , 0xd7 , 0x95 , 0x7c ,
0x79 , 0x62 , 0x65 , 0x4b , 0x74 , 0xa5 , 0x9b , 0xcd , 0x74 , 0xac ,
0x02 , 0xf7 , 0x66 , 0x89 , 0x6a , 0xe5 , 0x22 , 0x43 , 0x90 , 0x4f ,
0xc4 , 0x15 , 0x22 , 0xec , 0xca , 0xd7 , 0xc0 , 0x5d , 0x1f , 0x1b ,
0xa3 , 0x95 , 0x48 , 0x43 , 0x9e , 0xe7 , 0x01 , 0xb4 , 0xc3 , 0x7c ,
0x86 , 0x95 , 0x43 , 0x45 , 0xb9 , 0x28 , 0x85 , 0xce , 0x94 , 0xd7 ,
0x1b , 0x09 , 0xb7 , 0x47 , 0xee , 0xc1 , 0xa1 , 0xcb , 0x8e , 0x75 ,
0x6e , 0x6f , 0xbb , 0xf7 , 0x83 , 0x01 , 0x43 , 0x79 , 0xcb , 0x53 ,
0xd9 , 0xdb , 0xc7 , 0x62 , 0x7d , 0x9c , 0x36 , 0xe0 , 0x06 , 0xd7 ,
0x12 , 0xe1 , 0xbf , 0xd7 , 0x10 , 0xed , 0x9a , 0xb7 , 0x86 , 0x1b ,
0xe9 , 0x84 , 0x40 , 0xae , 0xf5 , 0x35 , 0x79 , 0x4e , 0xa3 , 0xc5 ,
0x5c , 0x79 , 0x46 , 0x34 , 0x0f , 0xa0 , 0xe8 , 0x6c , 0x2f , 0x49 ,
0x7d , 0x5d , 0xd3 , 0x9e , 0xe5 , 0xf7 , 0xab , 0xb1 , 0xd5 , 0xd9 ,
0x12 , 0x7c , 0x0f , 0xb0 , 0xa9 , 0xe2 , 0xc7 , 0x1e , 0x2d , 0xad ,
0xf6 , 0xcf , 0x15 , 0xa3 , 0x22 , 0x8a , 0x6c , 0x43 , 0x09 , 0xa2 ,
0x01 , 0xf5 , 0x3a , 0xb9 , 0xec , 0x78 , 0xe6 , 0xe9 , 0x57 , 0x08 ,
0xaf , 0x32 , 0x3c , 0x25 , 0xe9 , 0xfd , 0xbb , 0x1d , 0x10 , 0x9e ,
0x02 , 0xb8 , 0x8b , 0xa3 , 0x72 , 0x1b , 0xd7 , 0xae , 0xa4 , 0xc5 ,
0x0f , 0xb0 , 0x66 , 0x8d , 0x5b , 0x0e , 0xc8 , 0x4f , 0xe9 , 0x62 ,
0xb6 , 0x62 , 0x21 , 0xe0 , 0xb3 , 0x4a , 0xbe , 0xa0 , 0x84 , 0x94 ,
0x4a , 0xdb , 0xb1 , 0x07 , 0x54 , 0x97 , 0x80 , 0xb4 , 0x52 , 0x06 ,
0xdd , 0xc3 , 0x2f , 0x63 , 0xe6 , 0xdc , 0x31 , 0x34 , 0xce , 0x34 ,
0x4b , 0x35 , 0x27 , 0x27 , 0x1e , 0x35 , 0x47 , 0x4f , 0xb4 , 0xf0 ,
0x49 , 0x25 , 0xb6 , 0x17 , 0xd9 , 0xba , 0x21 , 0x66 , 0x28 , 0xbb ,
0xee , 0x59 , 0xbe , 0xed , 0x8c , 0xb4 , 0xf3 , 0x0e , 0x59 , 0x50 ,
0x84 , 0xc2 , 0x25 , 0xf3 , 0xa7 , 0x86 , 0x81 , 0x0e , 0x0f , 0xc4 ,
0xff , 0x03 , 0x18 , 0x22 , 0xb2 , 0x9b , 0x99 , 0x91 , 0x35 , 0xfd ,
0x7f , 0x87 , 0x41 , 0x00 , 0x1e , 0x2e , 0x5f , 0xa1 , 0xe7 , 0x63 ,
0x63 , 0xb6 , 0x1c , 0x1b , 0x09 , 0xaf , 0x3b , 0x05 , 0xe6 , 0x19 ,
0x7c , 0x7a
}};

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
	beacon_id_t beacon_id = BEACON_ID;							// ID
	beacon_location_id_t beacon_location_id = BEACON_LOC_ID;	// Location
	beacon_timer_t t_init = 0; 									// Initial time

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
			beacon_gen_id(&beacon_eph_id, &BEACON_SK, bc.loc, &epoch);
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
