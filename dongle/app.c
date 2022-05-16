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
#include "src/led.h"

#define LOG_LEVEL__DEBUG
#include "src/common/src/util/log.h"
#include "src/common/src/util/util.h"

extern void dongle_start();
int download_complete = 0;
sl_sleeptimer_timer_handle_t led_timer;

// Sync handle
static uint16_t sync_handle = 0;

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle,
    __attribute__ ((unused)) void *data)
{
#define user_handle (*((uint8_t*)(handle->callback_data)))
  if (user_handle == MAIN_TIMER_HANDLE) {
    dongle_clock_increment();
    download_complete = dongle_download_complete_status();
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
  sl_status_t sc;

  // Initialize the main timer
  uint8_t main_timer_handle = MAIN_TIMER_HANDLE;
  sl_sleeptimer_timer_handle_t timer;
  sc = sl_sleeptimer_start_periodic_timer_ms(&timer,
      DONGLE_TIMER_RESOLUTION, sl_timer_on_expire,
      &main_timer_handle, 0, 0);
  if (sc != SL_STATUS_OK) {
    log_errorf("failed periodic timer start main, sc: %d\r\n", sc);
    return sc;
  }

  // Initialize higher precision timer
  uint8_t prec_timer_handle = PREC_TIMER_HANDLE;
  sl_sleeptimer_timer_handle_t precision_timer;
  sc = sl_sleeptimer_start_periodic_timer_ms(&precision_timer,
      PREC_TIMER_TICK_MS, sl_timer_on_expire, &prec_timer_handle, 0, 0);
  if (sc != SL_STATUS_OK) {
    log_errorf("failed periodic timer start hp, sc: %d\r\n", sc);
    return sc;
  }

  sc = sl_sleeptimer_start_periodic_timer_ms(&led_timer, LED_TIMER_MS,
      dongle_led_timer_handler, (void *) NULL, 0, 0);
  if (sc != SL_STATUS_OK) {
    log_errorf("failed period led timer start, sc: %d\r\n", sc);
    return sc;
  }

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
      dongle_init();
      dongle_start();
      log_infof("download complete: %u\r\n", download_complete);


#if DONGLE_UPLOAD
      access_advertise();
#endif
      break;

    case sl_bt_evt_scanner_scan_report_id:
#define report (evt->data.evt_scanner_scan_report)
      // first, scan on legacy advertisement channel
      dongle_on_scan_report(&report.address, report.rssi,
          report.data.data, report.data.len);
      log_debugf("periodic interval: %u synced: %u dwnld complete: %u\r\n",
          evt->data.evt_scanner_scan_report.periodic_interval,
          synced, download_complete);
#undef report

#if MODE__PERIODIC
      // then check for periodic info in packet
      if (!synced && !download_complete
          && evt->data.evt_scanner_scan_report.periodic_interval != 0) {
        log_debugf("%s", "Opening sync\r\n");
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

      break;

    case sl_bt_evt_sync_opened_id:
      log_debugf("new sync opened!\r\n");
      synced = 1;
      break;

    case sl_bt_evt_sync_closed_id:
      app_log_info("Sync lost...\r\n");
      dongle_on_sync_lost();
      synced = 0;
      break;

    case sl_bt_evt_sync_data_id:
      log_debugf("len: %d status: %d rssi: %d\r\n",
          evt->data.evt_sync_data.data.len,
          evt->data.evt_sync_data.data_status,
          evt->data.evt_sync_data.rssi);

#define ERROR_STATUS 0x02
      if (evt->data.evt_sync_data.data_status == ERROR_STATUS) {
        dongle_on_periodic_data_error(evt->data.evt_sync_data.rssi);
      } else {
        dongle_on_periodic_data(evt->data.evt_sync_data.data.data,
            evt->data.evt_sync_data.data.len,
            evt->data.evt_sync_data.rssi);
      }
#undef ERROR_STATUS
      if (dongle_download_complete_status() == 1) {
        sl_bt_sync_close(sync_handle);
        log_infof("%s", "download completed\r\n");
        download_complete = 1;
      }
      break;

    default:
      log_debugf("%s", "Unhandled bluetooth event\r\n");
      break;
  }
}
