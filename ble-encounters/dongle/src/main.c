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

#define LOG_LEVEL__DEBUG
#include "../../common/src/log.h"

#define DONGLE_SCAN_INTERVAL 5000 // ms

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

// GLOBAL DATA
struct k_mutex dongle_mu;

#define safely(e) k_mutex_lock(&dongle_mu, K_FOREVER); e; k_mutex_unlock(&dongle_mu)

dongle_timer_t dongle_time;

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


static int dongle_display(encounter_broadcast_t *bc)
{
// Debug: Display fields
#define data (*bc)
// Filter out by known id
	if (*data.b != (beacon_id_t) BEACON_ID) {
		return 1;
	}
    printk("Encounter broadcast: \n"
        "   id = %u\n"
        "   location_id = %llu\n"
        "   beacon_time = %u\n",
    *data.b, *data.loc, *data.t);
	dongle_time_print();
#undef data
    return 0;
}

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
// Filter mis-sized packets up front
    if (ad -> len != ENCOUNTER_BROADCAST_SIZE + 1) {
        return;
    }
	log_debugf("Broadcast received from %s (RSSI %d)\n", addr_str, rssi);
    decode_payload(ad -> data);
    encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t*) ad -> data);
    dongle_display(&en);
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
