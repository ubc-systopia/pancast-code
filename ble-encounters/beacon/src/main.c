//
// Beacon Application for PanCast Encounter logging
// Early version based on the bluetooth-beacon example from Zephyr
// Acts as a bluetooth beacon, using a full legacy-advertising
// payload to broadcast relevant information. See white-paper for
// details.
//

#include <zephyr.h>
#include <sys/printk.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

typedef struct bt_data bt_data_t;

// Scan data
static const bt_data_t adv_data[] = {
    BT_DATA_BYTES(BT_DATA_UUID32_ALL,
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14,
        0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c,
        0x1d, // 0x1e
        ),
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
