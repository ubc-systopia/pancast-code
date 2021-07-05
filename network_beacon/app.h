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

#include "sl_sleeptimer.h"


/***************************************************************************//**
 * Variables
 ******************************************************************************/
#define PER_ADV_INTERVAL 6
#define PER_ADV_SIZE 240
#define PER_ADV_UPDATE 1 // interval in seconds to update ad data

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void
app_init (void);

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void
app_process_action (void);

/***************************************************************************//**
 * Timer expiration callback.
 ******************************************************************************/
void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data);

#endif  // APP_H
