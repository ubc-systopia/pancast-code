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

#include "src/dongle.h"

#define LOG_LEVEL__INFO
#include "../../common/src/log.h"


void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  log_debugf("Timer expiration; handle = %p, data = %p\r\n", handle, data);
  dongle_clock_increment();
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
sl_status_t app_init(void)
{
  log_debug("Initialize\r\n");
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
  switch (SL_BT_MSG_ID(evt->header)) {
      case sl_bt_evt_system_boot_id:
        app_log_info("Bluetooth start\r\n");
        log_debug("Bluetooth device booted and ready\r\n");
        dongle_start();
        break;
      case sl_bt_evt_scanner_scan_report_id:
#define report (evt->data.evt_scanner_scan_report)
        dongle_log(&report.address,
                   report.rssi, report.data.data, report.data.len);
#undef report
        break;
      case sl_bt_evt_system_soft_timer_id:
      default:
        log_debug("Unhandled bluetooth event\r\n");
        break;
    }
}
