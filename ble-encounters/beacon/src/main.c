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

#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

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

static beacon_eph_id_t        test_eph = {
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d }
};

static beacon_location_id_t   test_loc = 0x0e0f101112131415;
static beacon_id_t            test_bid = 0x16171819;
static beacon_timer_t         test_tmr = 0x1a1b1c1d;

static encounter_broadcast_t test_broadcast = {
    &test_eph, &test_loc, &test_bid, &test_tmr
};


// Scan Response
static const bt_data_t adv_res[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void beacon_adv(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

    bt_wrapper_t payload;

// Load actual test broadcast
    encode_encounter(&payload.en_data, &test_broadcast);
    
	print_bytes(printk, (&payload.en_data), MAX_BROADCAST_SIZE);
    
	form_payload(&payload);

    print_bytes(printk, (&payload.en_data), MAX_BROADCAST_SIZE);

// Legacy advertising
// Start
// Using wrapper
	err = bt_le_adv_start(
        BT_LE_ADV_NCONN_IDENTITY,
        payload.bt_data, ARRAY_SIZE(payload.bt_data),
	    adv_res, ARRAY_SIZE(adv_res));
// // Without wrapper (to force legacy)
// // Not working as the legacy adv function is not exposed
//     struct bt_le_ext_adv *adv = adv_new_legacy();
// 	if (!adv) {
// 		err = -ENOMEM;
// 	} else {
//         printk("Starting legacy advertising\n");
// 	    err = bt_le_adv_start_legacy(adv, BT_LE_ADV_NCONN_IDENTITY, 
//             adv_data, ARRAY_SIZE(adv_data), adv_res, ARRAY_SIZE(adv_res));
//     }
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

    int err = bt_enable(beacon_adv);
    if (err) {
        printk("Failure: error code = %d\n", err);
    }
}
