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

#define LOG_LEVEL__INFO

#include "../../../common/src/log.h"

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  log_debug("Initialize\r\n");
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  static int i;
  log_debugf("tick: %d\r\n", i);
  i++;
}

void sl_bt_on_event (sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
      case sl_bt_evt_system_boot_id:
        log_info("Bluetooth device booted and ready\r\n");
        break;
      case sl_bt_evt_system_soft_timer_id:
        log_info("Timer tick\r\n");
        break;
      default:
        log_debug("Unhandled bluetooth event\r\n");
        break;
    }
}
