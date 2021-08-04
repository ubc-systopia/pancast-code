/***************************************************************************/ /**
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

#include "src/beacon.h"

#include "../common/src/settings.h"

/***************************************************************************/ /**
 * Macros
 ******************************************************************************/

/* Periodic Advertising */
#define PER_ADV_HANDLE 0xff
#define MIN_ADV_INTERVAL 75  // min. adv. interval (milliseconds * 1.6)
#define MAX_ADV_INTERVAL 100 // max. adv. interval (milliseconds * 1.6)
#define PER_ADV_INTERVAL 8 // 10 ms - even no. of ms for ease of halving
#define PER_ADV_SIZE 250
#define PER_FLAGS 0 // no periodic advertising flags
#define PER_TX_POWER GLOBAL_TX_POWER

#define NO_MAX_DUR 0 // 0 for no duration limit
#define NO_MAX_EVT 0 // 0 for no max events

/* Timers */
#define TIMER_1MS 1
#define TIMER_1S 1000 // one second in ms, used for timer
#define MAIN_TIMER_HANDLE 0
#define RISK_TIMER_HANDLE 1
#define HP_TIMER_HANDLE   2
#define MAIN_TIMER_PRIORT 2
#define RISK_TIMER_PRIORT 1
#define HP_TIMER_PRIORT   0

/* Risk Data */
#define RISK_DATA_SIZE 250 // PER_ADV_SIZE * BATCH_SIZE
// #define BATCH_SIZE 2

extern float hp_time_now;
extern float hp_time_adv_start;

#define now() hp_time_now
#define HP_TIME_ADV_START hp_time_adv_start

/***************************************************************************/ /**
 * Initialize application.
 ******************************************************************************/
void app_init(void);

/***************************************************************************/ /**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void);

/***************************************************************************/ /**
 * Timer expiration callback.
 ******************************************************************************/
void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data);

#endif // APP_H
