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

#define SYNC_BUFFER_SIZE 1000

// Sync handle
static uint16_t sync_handle = 0;


// Testing Variables
uint64_t start_time;
uint64_t timestamp;
uint32_t num_sync_lost;
uint32_t byte_count;
uint64_t ms;

timertest_t sync_test;

uint8_t sync_rx_buffer[SYNC_BUFFER_SIZE];
int sync_data_index;

int log = 0;

void print_data(uint8array data) {
  for (int i = 0; i < data.len; i++) {
      app_log_info(", %d", data.data[i]);
  }
  app_log_info("end data \n");
}

void log_throughput(uint32_t len){
	byte_count = byte_count + len;
	timestamp = sl_sleeptimer_get_tick_count64() - start_time;
	sl_sleeptimer_tick64_to_ms(timestamp, &ms);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  start_time = 0;
  timestamp = 0;
  byte_count = 0;
  ms = 0;
  num_sync_lost = 0;
  sync_data_index = 0;
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

      // Set scanner timing
      app_log_info("Setting scanner timing\r\n");
      sc = sl_bt_scanner_set_timing(SCAN_PHY, SCAN_INTERVAL, SCAN_WINDOW);
      app_assert_status(sc);

      // Set scanner mode
      app_log_info("Setting scanner mode\r\n");
      sc = sl_bt_scanner_set_mode(SCAN_PHY, SCAN_MODE);
      app_assert_status(sc);

      // Set sync parameters
      app_log_info("Setting sync parameters\r\n");
      sc = sl_bt_sync_set_parameters(SYNC_SKIP, SYNC_TIMEOUT, SYNC_FLAGS);
      app_assert_status(sc);

      // Start scanning
      app_log_info("Starting scan\r\n");
      sc = sl_bt_scanner_start(SCAN_PHY, scanner_discover_observation);
      app_assert_status(sc);

      // Set timer
      app_log_info("Setting soft timer\r\n");
      sc = sl_bt_system_set_soft_timer(TIMER_1S, 0, 0);
      if (sc != 0) {
        app_log_info("sc: %d \r\n", sc);
      }
      break;

    case sl_bt_evt_scanner_scan_report_id:
      // check for periodic info in packet
       if (evt->data.evt_scanner_scan_report.periodic_interval != 0) {
    	   // Start test
    	   sync_test.start_ticks = sl_sleeptimer_get_tick_count64();
           app_log_info("Opening sync\r\n");
           // Open sync
           sc = sl_bt_sync_open(evt->data.evt_scanner_scan_report.address,
                           evt->data.evt_scanner_scan_report.address_type,
                           evt->data.evt_scanner_scan_report.adv_sid,
                           &sync_handle);
           if (sc != 0) {
               app_log_info("sc: 0x%x\r\n", sc);
           }
           app_assert_status(sc);
       }
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_sync_opened_id:
      app_log_info("new sync opened!\r\n");
      sync_test.end_ticks = sl_sleeptimer_get_tick_count64();
      sync_test.diff = sync_test.end_ticks - sync_test.start_ticks;
      sl_sleeptimer_tick64_to_ms(timestamp, &sync_test.diff);
      if (start_time == 0) {
          start_time = sl_sleeptimer_get_tick_count64();
          sl_sleeptimer_tick64_to_ms(timestamp, &ms);
      }
      app_log_info("start_time (ms) = %u\r\n", ms);
      sl_bt_scanner_stop();
      break;

    case sl_bt_evt_sync_data_id:
      // Log info
      app_log_info("Received data, len :%d\r\n",
                   evt->data.evt_sync_data.data.len);
      app_log_info("Status: %d\n",
                         evt->data.evt_sync_data.data_status);
      // app_log_info("RSSI: %d\n", evt->data.evt_sync_data.rssi);

     //  Print entire byte array
//            for (int i = 0; i < evt->data.evt_sync_data.data.len; i++) {
//               app_log_info(", %d", evt->data.evt_sync_data.data.data[i]);
//            }
//            app_log_info("\r\n");

      // check if sync data index + evt->data > length
      // if it is larger or equal then print and reset length
      if ((sync_data_index + evt->data.evt_sync_data.data.len) < SYNC_BUFFER_SIZE) {
    	  memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_sync_data.data.data, evt->data.evt_sync_data.data.len);
          sync_data_index += evt->data.evt_sync_data.data.len;
      } else {
    	  // Printf buffer
          for (int i = 0; i < SYNC_BUFFER_SIZE; i++) {
            app_log_info(", %d", sync_rx_buffer[i]);
          }
          app_log_info("\r\n");
          sync_data_index = 0;

        //  memset(&sync_rx_buffer, 0, SYNC_BUFFER_SIZE);
          memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_sync_data.data.data, evt->data.evt_sync_data.data.len);
          sync_data_index += evt->data.evt_sync_data.data.len;
      }

        // Log throughput measurements
    	byte_count = byte_count + evt->data.evt_sync_data.data.len;
    	timestamp = sl_sleeptimer_get_tick_count64() - start_time;
    	sl_sleeptimer_tick64_to_ms(timestamp, &ms);
    	break;

     case sl_bt_evt_sync_closed_id:
        app_log_info("Sync lost...\r\n");
        num_sync_lost = num_sync_lost + 1;
        app_log_info("num_sync_lost %d\r\n", num_sync_lost);

        // Start scanning again
        app_log_info("Starting scan...\r\n");
        sc = sl_bt_scanner_start(1, scanner_discover_observation);
        app_assert_status(sc);
        break;

      case sl_bt_evt_system_soft_timer_id:

        if (ms > TEST_DURATION) {
            app_log_info("timestamp: %u\r\n", ms);
            app_log_info("byte_count: %u\r\n", byte_count);
            app_log_info("num_sync_lost %d\r\n", num_sync_lost);
            sl_bt_sync_close(sync_handle);
            sc = sl_bt_system_set_soft_timer(0, 0, 1);
        }
        break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
