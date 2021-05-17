//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include <zephyr.h>
#include <stddef.h>
#include <sys/printk.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include "../../common/src/pancast.h"
#include "../../common/src/util.h"

#define BEACON_ID		0xffffffff
#define BEACON_LOC_ID	0xffffffffffffffff

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
    bt -> data = &of;
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
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	encounter_broadcast_t bc;

	beacon_id_t beacon_id = BEACON_ID;
	beacon_location_id_t beacon_location_id = BEACON_LOC_ID;
	beacon_timer_t beacon_time = 0;
	beacon_eph_id_t beacon_eph_id;

	bc.b = &beacon_id;
	bc.loc = &beacon_location_id;
	bc.t = &beacon_time;
	bc.eph = &beacon_eph_id;

    bt_wrapper_t payload;

// Load actual test broadcast
    encode_encounter(&payload.en_data, &bc);

	form_payload(&payload);

// Legacy advertising Start
	err = bt_le_adv_start(
        BT_LE_ADV_NCONN_IDENTITY,
        payload.bt_data, ARRAY_SIZE(payload.bt_data),
	    adv_res, ARRAY_SIZE(adv_res));
    if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
// obtain and report adverisement address
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

    printk("Beacon started, advertising as %s\n", addr_s);
}

void main(void)
{
	printk("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);
    int err = bt_enable(beacon_broadcast);
    if (err) {
        printk("Bluetooth Enable Failure: error code = %d\n", err);
    }
}
