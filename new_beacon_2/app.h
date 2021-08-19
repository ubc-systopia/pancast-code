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

#ifndef APP_H
#define APP_H

#include <unistd.h>
#include "sl_sleeptimer.h"
#include "src/beacon.h"

#define TIMER_1MS 1
#define MAIN_TIMER_HANDLE 0
#define MAIN_TIMER_PRIORT 0
#define BEACON_TIMER_RESOLUTION 60000

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void);

/***************************************************************************//**
* Update risk data after receive from raspberry pi client
******************************************************************************/
void update_risk_data(int len, uint8_t *data);

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data);

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void);

#endif  // APP_H
