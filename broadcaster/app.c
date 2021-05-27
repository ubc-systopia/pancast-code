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

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// Test Data
const uint8_t periodic_adv_data[253] = {0x58,0x85,0x89,0x0c,0x63,0xc8,0x46,0x9e,
    0x93,0x46,0x0a,0x34,0xf9,0xd7};
uint8_t periodic_adv_data_l[1650] = {0x58,0x85,0x89,0x0c,0x63,0xc8,0x46,0x9e,
    0x93,0x46,0x0a,0x34,0xf9,0xd7};

uint16_t tx_packets;
uint16_t failures;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  for (int i = 0; i < 1650; i++) {
       periodic_adv_data_l[i] = i % 255;
  }
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

      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert_status(sc);

      // Create an advertising set.
      app_log_info("Creating advertising set\n");
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        300, // min. adv. interval (milliseconds * 1.6)
        300, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

      app_log_info("Starting periodic advertising...\n");
      sc = sl_bt_advertiser_start_periodic_advertising(advertising_set_handle,
                                                       0x06, 0x06, 0);
      app_assert_status(sc);

//      // Set long periodic data (chained)
//      for (int i = 0; i < 6; i++) {
//          app_log_info("Writing to buffer...\n");
//          sc = sl_bt_system_data_buffer_write(255, &periodic_adv_data_l[i*255]);
//          if (sc != 0) {
//              app_log_info("sc: %d \n", sc);
//          }
//              app_assert_status(sc);
//
//       }
//
//      // should be total 1650
//      sc = sl_bt_system_data_buffer_write(120, &periodic_adv_data_l[1530]);
//
//      app_log_info("Setting long advertising data...\n");
//      sc = sl_bt_advertiser_set_long_data(advertising_set_handle, 8);

      // Set periodic data
      app_log_info("Setting advertising data...\n");
      sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, 253,
                                           &periodic_adv_data[0]);

      if (sc != 0) {
          app_log_info("sc: %d \n", sc);
      }
      app_assert_status(sc);
      app_log_info("Success!\n");


      app_log_info("Setting soft timer...");
      sc = sl_bt_system_set_soft_timer(32768, 0, 0);
      if (sc != 0) {
        app_log_info("sc: %d \n", sc);
      }
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
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_system_soft_timer_id:
      sl_bt_system_get_counters(0, &tx_packets, 0, 0, &failures);
      app_log_info("tx_packets %d/n", tx_packets);
    //  periodic_adv_data[0]++;
      // add data to buffer
      app_log_info("Setting advertising data...\n");
      sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, 253,
                                                 &periodic_adv_data[0]);

      if (sc != 0) {
         app_log_info("sc: %d \n", sc);
      }
      app_assert_status(sc);
      app_log_info("Success!\n");
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
