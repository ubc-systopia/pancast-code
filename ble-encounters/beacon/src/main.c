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

#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

typedef struct bt_data bt_data_t;

static void make_payload(bt_data_t *bt_data, encounter_broadcast_t *bc_data)
{
#define bt (*bt_data)
    const uint8_t *bc = (uint8_t*) bc_data;
    bt.type = bc[0];
    const size_t len = ENCOUNTER_BROADCAST_SIZE - 1;
    printk("%d\n", len);
    bt.data_len = len;
    bt.data = bc + 1;
#undef bt
    return;
}

static const encounter_broadcast_t test_broadcast = {
    {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d }},
    72623859790382848,
    16909060,
    84281096
};

// Scan data
static bt_data_t adv_data[] = {
// This is a raw payload, containing a total of 31 bytes
// The first byte is contained in the 'type' field. Next 29
// are actual data, and the last is in the length field.
// With the current API, we're forced to set the length byte
// to the actual length of the data (29) - so may have to use
// lower level functions to use this for advertisement data
    BT_DATA(0x21, ((uint8_t []){
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14,
        0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c,
        0x1d, // 0x1e
    }), 0x1d),
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

// Load actual test broadcast
    make_payload(adv_data, &test_broadcast);

// Legacy advertising
// Start
// Using wrapper
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, adv_data, ARRAY_SIZE(adv_data),
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
