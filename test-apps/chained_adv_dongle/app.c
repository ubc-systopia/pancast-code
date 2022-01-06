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

#define SCAN_INTERVAL 0x320
#define SCAN_WINDOW 0x320
#define SYNC_SKIP 0
#define SYNC_TIMEOUT 500
#define SYNC_FLAGS 0
#define SCAN_MODE 0

#define MAIN_TIMER_HANDLE 1
#define TEST_DURATION_MS 300000

// The advertising set handle allocated from Bluetooth stack.
static uint16_t sync_handle = 0xff;

int synced = 0;

uint32_t packets_received;
uint32_t bytes_received;

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  app_log_info("# pkts: %u, #bytes: %u\r\n", packets_received, bytes_received);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
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

      // Set scanner timing
      app_log_info("Setting scanner timing\r\n");
      sc = sl_bt_scanner_set_timing(gap_1m_phy, SCAN_INTERVAL, SCAN_WINDOW);

      // Set scanner mode
      app_log_info("Setting scanner mode\r\n");
      sc = sl_bt_scanner_set_mode(gap_1m_phy, SCAN_MODE);

      // Set sync parameters
      app_log_info("Setting sync parameters\r\n");
      sc = sl_bt_sync_set_parameters(SYNC_SKIP, SYNC_TIMEOUT, SYNC_FLAGS);

      // Start scanning
      app_log_info("Starting scan\r\n");
      sc = sl_bt_scanner_start(gap_1m_phy, scanner_discover_observation);

      break;

    case sl_bt_evt_scanner_scan_report_id:
      if (!synced
    	&& evt->data.evt_scanner_scan_report.periodic_interval != 0) {
    	  // Open sync
        sc = sl_bt_sync_open(evt->data.evt_scanner_scan_report.address,
    	     evt->data.evt_scanner_scan_report.address_type,
    	     evt->data.evt_scanner_scan_report.adv_sid,
    	        &sync_handle);
    	if (sc != 0) {
    	  app_log_error("sync not opened: sc: 0x%x\r\n", sc);
        }
      }
      break;

    case sl_bt_evt_sync_data_id:
      if (evt->data.evt_sync_data.data.len == 0) {
    	app_log_info("data len is 0!\r\n");
      }
      packets_received++;
      bytes_received += evt->data.evt_sync_data.data.len;
//      app_log_info("data len: %u, status: %u\r\n",
//    		  evt->data.evt_sync_data.data.len, evt->data.evt_sync_data.data_status);
      break;

    case sl_bt_evt_sync_opened_id:
      app_log_info("sync opened\r\n");
      synced = 1;
      break;

    case sl_bt_evt_sync_closed_id:
      app_log_info("sync lost...\r\n");
      synced = 0;
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
