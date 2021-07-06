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


/***************************************************************************//**
 * Macros
 ******************************************************************************/

/* Periodic Advertising */
#define PER_ADV_HANDLE 0xff
#define MIN_ADV_INTERVAL 75  // min. adv. interval (milliseconds * 1.6)
#define MAX_ADV_INTERVAL 100 // max. adv. interval (milliseconds * 1.6)
#define PER_ADV_INTERVAL 100
#define PER_ADV_SIZE 250
#define PER_FLAGS 0 // no periodic advertising flags

#define NO_MAX_DUR 0 // 0 for no duration limit
#define NO_MAX_EVT 0 // 0 for no max events

/* Timers */
#define TIMER_1S 32768 // one second in system clock ticks, used for timer
#define RISK_TIMER_HANDLE 1
#define RISK_UPDATE_FREQ 0.08

/* Risk Data */
#define RISK_DATA_SIZE 250
//#define BATCH_SIZE 2

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init (void);

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void
app_process_action (void);

#endif  // APP_H
