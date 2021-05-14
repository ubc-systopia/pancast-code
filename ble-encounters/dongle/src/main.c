//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include "../../common/src/pancast.h"

static void dongle_decode(uint8_t *data, uint16_t len)
{
    if (len != ENCOUNTER_BROADCAST_SIZE + 1) {
        return;
    }
    printk("Data: 0x");
#define _print_(b) printk(" %x", b)
    for (int i = 0; i < len; i++) {
        _print_(data[i]);
    }
#undef _print_
    printk("\n");
}

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
    dongle_decode(ad -> data, ad -> len);
}

static void dongle_scan(void)
{
	int err;

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, dongle_log);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}


void main(void)
{
	printk("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);

	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	dongle_scan();
}
