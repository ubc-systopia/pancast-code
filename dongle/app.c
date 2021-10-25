/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>

#include "sl_bluetooth.h"
#include "app_log.h"
#include "app_assert.h"

#include "src/dongle.h"
#include "src/time.h"

#define LOG_LEVEL__DEBUG
#include "src/common/src/util/log.h"
#include "src/common/src/util/util.h"

// Sync handle
static uint16_t sync_handle = 0;

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
#define user_handle (*((uint8_t*)(handle->callback_data)))
  if (user_handle == MAIN_TIMER_HANDLE) {
      dongle_clock_increment();
  }
  if (user_handle == PREC_TIMER_HANDLE) {
      dongle_hp_timer_add(1);
  }
#undef user_handle
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
sl_status_t app_init(void)
{
  log_debugf("%s", "Initialize\r\n");
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}

int synced = 0; // no concurrency control but acts as an eventual state signal

void sl_bt_on_event (sl_bt_msg_t *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      dongle_time_init();
      log_debugf("Bluetooth start\r\n");
      log_debugf("%s", "Bluetooth device booted and ready\r\n");
      dongle_start();
      log_debugf("%s", "Dongle started\r\n");
      break;

    case sl_bt_evt_scanner_scan_report_id:
#define report (evt->data.evt_scanner_scan_report)
//        log_debugf("%s", "Scan result\r\n");
//#define addr (report.address.addr)
//        log_debugf("Packet Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
//                         addr[0], addr[1], addr[2], addr[3], addr[4], addr[4]);
//#undef addr
#ifdef MODE__LEGACY_LOG
      // First, log into the legacy decode pipeline
      dongle_log(&report.address,
                 report.rssi, report.data.data, report.data.len);
#endif
#ifdef MODE__PERIODIC
      // then check for periodic info in packet
      if (!synced
          && evt->data.evt_scanner_scan_report.periodic_interval != 0) {
//         // Start test
//         sync_test.start_ticks = sl_sleeptimer_get_tick_count64();
        log_debugf("Opening sync\r\n");
        // Open sync
        sc = sl_bt_sync_open(evt->data.evt_scanner_scan_report.address,
                       evt->data.evt_scanner_scan_report.address_type,
                       evt->data.evt_scanner_scan_report.adv_sid,
                       &sync_handle);
        if (sc != 0) {
          log_errorf("sync not opened: sc: 0x%x\r\n", sc);
        }
      }
#endif
#undef report
      break;

    case sl_bt_evt_sync_opened_id:
      log_debugf("new sync opened!\r\n");
//        sync_test.end_ticks = sl_sleeptimer_get_tick_count64();
//        sync_test.diff = sync_test.end_ticks - sync_test.start_ticks;
//        sl_sleeptimer_tick64_to_ms(timestamp, &sync_test.diff);
//        if (start_time == 0) {
//            start_time = sl_sleeptimer_get_tick_count64();
//            sl_sleeptimer_tick64_to_ms(timestamp, &ms);
//        }
//        app_log_info("start_time (ms) = %u\r\n", ms);
        // sl_bt_scanner_stop();
      synced = 1;
      break;

    case sl_bt_evt_sync_closed_id:
      app_log_info("Sync lost...\r\n");
//        num_sync_lost = num_sync_lost + 1;
//        app_log_info("num_sync_lost %d\r\n", num_sync_lost);
//
//        // Start scanning again
//        app_log_info("Starting scan...\r\n");
//        sc = sl_bt_scanner_start(1, scanner_discover_observation);
//        app_assert_status(sc);
      dongle_on_sync_lost();
      synced = 0;
      break;

    case sl_bt_evt_sync_data_id:
      // Log info
      log_debugf("Received data, len :%d\r\n",
                   evt->data.evt_sync_data.data.len);
      log_debugf("Status: %d\r\n",
                         evt->data.evt_sync_data.data_status);
//        evt->data.evt_sync_data.

      // app_log_info("RSSI: %d\n", evt->data.evt_sync_data.rssi);

//       //  Print entire byte array
//        for (int i = 0; i < evt->data.evt_sync_data.data.len; i++) {
//           app_log_debug(", %d\r\n", evt->data.evt_sync_data.data.data[i]);
//        }
//        app_log_debug("\r\n");

#define ERROR_STATUS 0x02
      if (evt->data.evt_sync_data.data_status == ERROR_STATUS) {
        dongle_on_periodic_data_error(evt->data.evt_sync_data.rssi);
      }
#undef ERROR_STATUS
      else {
        dongle_on_periodic_data(evt->data.evt_sync_data.data.data,
            evt->data.evt_sync_data.data.len,
            evt->data.evt_sync_data.rssi);
      }

//        // check if sync data index + evt->data > length
//        // if it is larger or equal then print and reset length
//        if ((sync_data_index + evt->data.evt_sync_data.data.len) < SYNC_BUFFER_SIZE) {
//          memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_sync_data.data.data, evt->data.evt_sync_data.data.len);
//            sync_data_index += evt->data.evt_sync_data.data.len;
//        } else {
//          // Printf buffer
//            for (int i = 0; i < SYNC_BUFFER_SIZE; i++) {
//              app_log_info(", %d", sync_rx_buffer[i]);
//            }
//            app_log_info("\r\n");
//            sync_data_index = 0;
//
//          //  memset(&sync_rx_buffer, 0, SYNC_BUFFER_SIZE);
//            memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_sync_data.data.data, evt->data.evt_sync_data.data.len);
//            sync_data_index += evt->data.evt_sync_data.data.len;
//        }
//
//          // Log throughput measurements
//        byte_count = byte_count + evt->data.evt_sync_data.data.len;
//        timestamp = sl_sleeptimer_get_tick_count64() - start_time;
//        sl_sleeptimer_tick64_to_ms(timestamp, &ms);
      break;

    case sl_bt_evt_system_soft_timer_id:
    default:
      log_debugf("%s", "Unhandled bluetooth event\r\n");
      break;
  }
}
