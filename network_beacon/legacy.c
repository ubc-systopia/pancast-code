// Legacy Advertising for Network Beacon

#include <stdio.h>
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "sl_iostream.h"

#define ADV_INTERVAL 0x20

// Advertising handle
static uint8_t legacy_set_handle = 0xf1;

// Ephemeral ID
uint8_t adv_data_legacy[15] = {0x58,0x85,0x89,0x0c,0x63,0xc8,0x46,0x9e,
    0x93,0x46,0x0a,0x34,0xf9,0xd7,0xd7};

int start_legacy_advertising() {
	sl_status_t sc;

    printf ("Creating legacy advertising set\r\n");
    sc = sl_bt_advertiser_create_set(&legacy_set_handle);
    if (sc != 0) {
      printf ("Error, sc: 0x%lx\r\n", sc);
      return -1;
    }

    printf ("Starting legacy advertising...\r\n");
    // Set advertising interval to 100ms.
    sc = sl_bt_advertiser_set_timing(
  		  legacy_set_handle,
			  ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
			  0x20, // max. adv. interval (milliseconds * 1.6) next: 0x4000
			  0,   // adv. duration, 0 for continuous advertising
			  0);  // max. num. adv. events
    if (sc != 0) {
       printf ("Error, sc: 0x%lx\r\n", sc);
       return -1;
    }

    // Start legacy advertising
    sc = sl_bt_advertiser_start(
    legacy_set_handle,
    advertiser_broadcast,
    advertiser_non_connectable);
    if (sc != 0) {
       printf ("Error, sc: 0x%lx\r\n", sc);
       return -1;
    }

    printf("Setting data...\r\n");
    sc = sl_bt_advertiser_set_data(legacy_set_handle, 0, 15, &adv_data_legacy[0]);
    if (sc != 0) {
       printf ("Error, sc: 0x%lx\r\n", sc);
       return -1;
    }
    printf("Success!\r\n");
    return 0;
}
