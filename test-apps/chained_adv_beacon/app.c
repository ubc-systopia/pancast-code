/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "app_log.h"

#define TEST_DATA_LEN 1650
#define CHAN_MAP_LEN 5

uint8_t test_data[TEST_DATA_LEN];

uint8_t chan_map[] = { 0x00, 0x00, 0x00, 0xFF, 0x00 };

// The advertising set handle allocated from Bluetooth stack.
static uint8_t chained_advertising_set_handle = 0xff;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  memset(&test_data, 0, TEST_DATA_LEN);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      app_log_info("address: 0x%x\r\n", system_id[0]);

      // Create an advertising set.
//      sc = sl_bt_gap_set_data_channel_classification(CHAN_MAP_LEN, chan_map);
//      if (sc != 0) {
//        app_log_error("error setting channel map, sc: 0x%x\r\n", sc);
//      }

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&chained_advertising_set_handle);
      if (sc != 0) {
        app_log_error("error creating adv set, sc: 0x%x\r\n", sc);
      }

#define BUFFER_WRITE_SIZE 250
      for (int i = 0; i <  3; i++) {
    	app_log_info("setting data\r\n");
        sc = sl_bt_system_data_buffer_write(BUFFER_WRITE_SIZE, &test_data[i*BUFFER_WRITE_SIZE]);
        if (sc != 0) {
          app_log_error("error writing to sys buffer, sc: 0x%x\r\n", sc);
        }
      }

#define LAST_WRITE_SIZE 150

//      sc = sl_bt_system_data_buffer_write(LAST_WRITE_SIZE, &test_data[TEST_DATA_LEN - LAST_WRITE_SIZE]);
//      if (sc != 0) {
//        app_log_error("error writing to sys buffer, sc: 0x%x\r\n", sc);
//      }

      app_log_info("about to set adv data\r\n");

      sc = sl_bt_advertiser_set_long_data(chained_advertising_set_handle,
    		  PACKET_TYPE_PERIODIC);
      if (sc != 0) {
        app_log_error("error setting adv data, sc: 0x%x\r\n", sc);
      }

      sc = sl_bt_advertiser_start_periodic_advertising(chained_advertising_set_handle,
    		  PER_ADV_INT_MIN, PER_ADV_INT_MAX, PER_ADV_FLAGS);
      if (sc != 0) {
        app_log_error("error setting adv data, sc: 0x%x\r\n", sc);
      }


      // Start general advertising and enable connections.
//      sc = sl_bt_advertiser_start(
//    	chained_advertising_set_handle,
//		sl_bt_advertiser_broadcast,
//		sl_bt_advertiser_non_connectable);
//      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
    	chained_advertising_set_handle,
        sl_bt_advertiser_general_discoverable,
        sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
