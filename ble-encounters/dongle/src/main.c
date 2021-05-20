//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include "../../common/src/pancast.h"
#include "../../common/src/util.h"

#define LOG_LEVEL__INFO
#include "../../common/src/log.h"

// number of distinct broadcast ids to keep track of at one time
#define DONGLE_MAX_BC_TRACKED 4

static int decode_payload(uint8_t *data)
{
    data[0] = data[1];
    data[1] = data[MAX_BROADCAST_SIZE - 1];
    return 0;
}

// matches the schema defined by encode
static int decode_encounter(encounter_broadcast_t *dat, encounter_broadcast_raw_t *raw)
{
    uint8_t *src = (uint8_t*) raw;
    size_t pos = 0;
#define link(dst, type) (dst = (type*) (src + pos)); pos += sizeof(type)
    link(dat->t, beacon_timer_t);
	link(dat->b, beacon_id_t);
    link(dat->loc, beacon_location_id_t);
	link(dat->eph, beacon_eph_id_t);
#undef link
    return 0;
}

// compares two ephemeral ids
static int ephcmp(beacon_eph_id_t *a, beacon_eph_id_t *b)
{
#define A (a -> bytes)
#define B (b -> bytes)
	for (int i = 0; i < BEACON_EPH_ID_HASH_LEN; i++)
		if (A[i] != B[i])
				return 1;
#undef B
#undef A
	return 0;
}

// GLOBAL DATA
struct k_mutex dongle_mu;

#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);
#define safely(e) LOCK e; UNLOCK

dongle_timer_t dongle_time;								// main dongle timer
beacon_eph_id_t cur_id[DONGLE_MAX_BC_TRACKED];			// currently observed ephemeral id
dongle_timer_t obs_time[DONGLE_MAX_BC_TRACKED];			// time of last new id observation

typedef size_t id_idx_t;
id_idx_t cur_id_idx = 0;

void dongle_time_set(dongle_timer_t *t)
{
	safely(dongle_time = *t);
}

void dongle_time_add(uint32_t *a)
{
	safely(dongle_time += *a);
}

void dongle_time_print()
{
	dongle_timer_t tmp;
	safely(tmp = dongle_time);
	log_debugf("dongle_time = %u\n", tmp);
}

void dongle_time_cpy(dongle_timer_t *t)
{
	dongle_timer_t tmp;
	safely(tmp = dongle_time);
	*t = tmp;
}

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
// Filter mis-sized packets
    if (ad -> len != ENCOUNTER_BROADCAST_SIZE + 1) {
        return;
    }
	log_debugf("Broadcast received from %s (RSSI %d)\n", addr_str, rssi);
    decode_payload(ad -> data);
    encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t*) ad -> data);
// the following alters dongle state, so the lock is obtained
	LOCK
// determine which tracked id, if any, is a match
	id_idx_t i = DONGLE_MAX_BC_TRACKED;
	for (id_idx_t j = 0; j < DONGLE_MAX_BC_TRACKED; j++) {
		if (!ephcmp(en.eph, &cur_id[j])) {
			i = j;
		}
	}
	if (i == DONGLE_MAX_BC_TRACKED) {
// if no match was found, start tracking the new id, replacing the oldest one
// currently tracked
		log_debug("new ephemeral id\n");
		print_bytes(en.eph -> bytes, BEACON_EPH_ID_HASH_LEN, "eph id");
		i = cur_id_idx;
		cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;
		memcpy(&cur_id[i], en.eph -> bytes, BEACON_EPH_ID_HASH_LEN);
		obs_time[i] = dongle_time;
	} else {
		log_debugf("id match at index %d\n", i);
// when a matching ephemeral id is observed
// check conditions for a valid encounter
		if (dongle_time - obs_time[i] >= DONGLE_ENCOUNTER_MIN_TIME) {
// when a valid encounter is detected
// log the encounter
			log_infof("Beacon Encounter (id=%u, t_b=%u, t_d=%u, dev=%s)\n", *en.b, *en.t, dongle_time, addr_str);
// reset the observation time
			obs_time[i] = dongle_time;
		}
	}
	UNLOCK
}

static void dongle_scan(void)
{
// Initialization
// TODO: flash load

	dongle_timer_t t_init = 0; 									// Initial time

	k_mutex_init(&dongle_mu);

// Scan Start
	int err = err = bt_le_scan_start(BT_LE_SCAN_PARAM(
		BT_LE_SCAN_TYPE_PASSIVE,    // passive scan
		BT_LE_SCAN_OPT_NONE,        // no options; in particular, allow duplicates
		BT_GAP_SCAN_FAST_INTERVAL,  // interval
		BT_GAP_SCAN_FAST_WINDOW     // window
	), dongle_log);
	if (err) {
		log_errorf("Scanning failed to start (err %d)\n", err);
		return;
	} else {
		log_debug("Scanning successfully started\n");
	}

// Dongle Loop
// timing and control logic is largely the same as the beacon
// application. The main difference is that scanning does not
// require restart for a new epoch.

	dongle_time_set(&t_init);
	dongle_epoch_counter_t epoch = 0;

	struct k_timer kernel_time;
	k_timer_init(&kernel_time, NULL, NULL);

// Timer zero point
#define DUR K_MSEC(DONGLE_TIMER_RESOLUTION)
	k_timer_start(&kernel_time, DUR, DUR);
#undef DUR
	uint32_t timer_status = 0;

	while (!err) {
// get most updated time
		timer_status = k_timer_status_sync(&kernel_time);
		timer_status += k_timer_status_get(&kernel_time);
		dongle_time_add(&timer_status);
// update epoch
		static dongle_epoch_counter_t old_epoch;
		old_epoch = epoch;
		epoch = epoch_i(dongle_time, t_init);
		if (epoch != old_epoch) {
			log_infof("EPOCH STARTED: %u\n", epoch);
			// TODO: log time to flash
		}
		log_debugf("dongle timer: %u\n", dongle_time);

	}
}


void main(void)
{
	log_infof("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);

	int err;

	err = bt_enable(NULL);
	if (err) {
		log_errorf("Bluetooth init failed (err %d)\n", err);
		return;
	}

	log_info("Bluetooth initialized\n");

	dongle_scan();
}
