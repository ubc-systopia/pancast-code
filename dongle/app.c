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
#include "src/stats.h"
#include "src/download.h"

#include "src/common/src/util/log.h"
#include "src/common/src/util/util.h"

extern download_t download;
extern dongle_timer_t dongle_time;
extern dongle_stats_t stats;
extern void dongle_start();
int download_complete = 0;
extern float payload_start_ticks, payload_end_ticks;
extern float dongle_hp_timer;

// Sync handle
static uint16_t sync_handle = 0;
static uint16_t prev_sync_handle = -1;
static uint16_t num_sync_open_attempts = 0;
dongle_timer_t last_sync_open_time = 0, last_sync_close_time = 0;

int synced = 0; // no concurrency control but acts as an eventual state signal

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle,
    __attribute__ ((unused)) void *data)
{
  sl_status_t sc;
#define user_handle (*((uint8_t*)(handle->callback_data)))
  if (user_handle == MAIN_TIMER_HANDLE) {
    dongle_clock_increment();
    dongle_log_counters();
    download_complete = dongle_download_complete_status();
#if 0
    log_debugf("dongle_time %u stats.last_download_time: %u min wait: %u "
        "download complete: %d active: %d synced: %d handle: %d\r\n",
        dongle_time, stats.last_download_time, MIN_DOWNLOAD_WAIT,
        download_complete, download.is_active, synced, sync_handle);
#endif
    if (download_complete == 0 && synced == 1) {
      sc = sl_bt_sync_close(sync_handle);
      last_sync_close_time = dongle_time;
      synced = 0;
      log_infof("dongle_time %u stats.last_download_time: %u min wait: %u "
        "download complete: %d active: %d synced: %d handle: %d sc: 0x%0x\r\n",
        dongle_time, stats.last_download_time, MIN_DOWNLOAD_WAIT,
        download_complete, download.is_active, synced, sync_handle, sc);
    }
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
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}

void sl_bt_on_event (sl_bt_msg_t *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      dongle_init();
      dongle_start();

#if DONGLE_UPLOAD
      access_advertise();
#endif
      break;

    case sl_bt_evt_scanner_scan_report_id:
#define report (evt->data.evt_scanner_scan_report)
      // first, scan on legacy advertisement channel
      if ((report.packet_type & SCAN_PDU_TYPE_MASK) == 0) {
        dongle_on_scan_report(&report.address, report.rssi,
          report.data.data, report.data.len);
      } else {
#if MODE__PERIODIC
      if (last_sync_close_time > 0 &&
          (last_sync_close_time - last_sync_open_time) <
          (int) PERIODIC_SYNC_FAIL_LATENCY) {
        num_sync_open_attempts++;
      } else {
        num_sync_open_attempts = 0;
      }

      // then check for periodic info in packet
      if (!synced && !download_complete &&
          evt->data.evt_scanner_scan_report.periodic_interval != 0 &&
          num_sync_open_attempts < NUM_SYNC_ATTEMPTS) {
        // Open sync
        sc = sl_bt_sync_open(evt->data.evt_scanner_scan_report.address,
                       evt->data.evt_scanner_scan_report.address_type,
                       evt->data.evt_scanner_scan_report.adv_sid,
                       &sync_handle);
        last_sync_open_time = dongle_time;
        if (prev_sync_handle != sync_handle) {
          log_infof("open sync addr[%d]: %0x:%0x:%0x:%0x:%0x:%0x "
            "sid: %u pkt type: 0x%0x phy(%d, %d) "
            "tx power: %d rssi: %d chan: %d intvl: %d handle: %d sc: 0x%x\r\n",
            evt->data.evt_scanner_scan_report.address_type,
            evt->data.evt_scanner_scan_report.address.addr[0],
            evt->data.evt_scanner_scan_report.address.addr[1],
            evt->data.evt_scanner_scan_report.address.addr[2],
            evt->data.evt_scanner_scan_report.address.addr[3],
            evt->data.evt_scanner_scan_report.address.addr[4],
            evt->data.evt_scanner_scan_report.address.addr[5],
            evt->data.evt_scanner_scan_report.adv_sid,
            evt->data.evt_scanner_scan_report.packet_type,
            evt->data.evt_scanner_scan_report.primary_phy,
            evt->data.evt_scanner_scan_report.secondary_phy,
            evt->data.evt_scanner_scan_report.tx_power,
            evt->data.evt_scanner_scan_report.rssi,
            evt->data.evt_scanner_scan_report.channel,
            evt->data.evt_scanner_scan_report.periodic_interval,
            sync_handle, sc);
          prev_sync_handle = sync_handle;
        }

        payload_start_ticks = dongle_hp_timer;
#if 0
        if (sc != 0) {
          log_errorf("sync not opened, interval: %u sc: 0x%x\r\n",
              evt->data.evt_scanner_scan_report.periodic_interval, sc);
        }
#endif
      } else if (num_sync_open_attempts >= NUM_SYNC_ATTEMPTS &&
          ((dongle_time - last_sync_open_time) >= MIN_DOWNLOAD_WAIT)) {
        num_sync_open_attempts = 0;
        last_sync_open_time = dongle_time;
      }
#endif
      }

      break;

    case sl_bt_evt_sync_opened_id:
//      log_debugf("new sync opened!\r\n");
      synced = 1;
      break;

    case sl_bt_evt_sync_closed_id:
      log_infof("Sync lost...\r\n");
      dongle_on_sync_lost();
      synced = 0;
      break;

    case sl_bt_evt_sync_data_id:
//      log_debugf("len: %d status: %d rssi: %d\r\n",
//          evt->data.evt_sync_data.data.len,
//          evt->data.evt_sync_data.data_status,
//          evt->data.evt_sync_data.rssi);

#define ERROR_STATUS 0x02
      if (evt->data.evt_sync_data.data_status == ERROR_STATUS) {
        dongle_on_periodic_data_error(evt->data.evt_sync_data.rssi);
      } else {
        dongle_on_periodic_data(evt->data.evt_sync_data.data.data,
            evt->data.evt_sync_data.data.len,
            evt->data.evt_sync_data.rssi);
      }
#undef ERROR_STATUS

      download_complete = dongle_download_complete_status();
      /*
       * second clause for the case when on re-sync, dongle still not able to
       * download periodic data (e.g., len of download is 0). may happen because
       * the periodic intervals were not properly aligned?
       */
      if (download_complete == 1 || (dongle_time - stats.last_download_time > 10)) {
        sc = sl_bt_sync_close(sync_handle);
        last_sync_close_time = dongle_time;
        log_infof("dongle_time %u stats.last_download_time: %u min wait: %u "
          "download complete: %d active: %d synced: %d handle: %d sc: 0x%0x\r\n",
          dongle_time, stats.last_download_time, MIN_DOWNLOAD_WAIT,
          download_complete, download.is_active, synced, sync_handle, sc);
      }
#if 0
      if (dongle_download_complete_status() == 1) {
        sl_bt_sync_close(sync_handle);
        download_complete = 1;
        log_infof("%s", "download completed\r\n");
      }
#endif
      break;

    default:
//      log_debugf("%s", "Unhandled bluetooth event\r\n");
      break;
  }
}
