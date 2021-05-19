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

static int dongle_display(encounter_broadcast_t *bc)
{
// Debug: Display fields
#define data (*bc)
// Filter out by known id
	if (*data.b != (beacon_id_t) BEACON_ID) {
		log_debug("broadcast discarded\n");
		return 1;
	}
    printk("Encounter broadcast: \n"
        "   id = %u\n"
        "   location_id = %llu\n"
        "   beacon_time = %u\n",
    *data.b, *data.loc, *data.t);
#undef data
    return 0;
}

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	log_debugf("Device found: %s (RSSI %d)\n", addr_str, rssi);
// Filter mis-sized packets up front
    if (ad -> len != ENCOUNTER_BROADCAST_SIZE + 1) {
        return;
    }
    decode_payload(ad -> data);
    encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t*) ad -> data);
    dongle_display(&en);
}

static void dongle_scan(void)
{
	int err = 0;

	while (!err) {
		err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, dongle_log);
		if (err) {
			log_errorf("Scanning failed to start (err %d)\n", err);
		} else {
			log_debug("Scanning successfully started\n");
			k_sleep(K_MSEC(DONGLE_SCAN_INTERVAL));
			bt_le_scan_stop();
		}
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
