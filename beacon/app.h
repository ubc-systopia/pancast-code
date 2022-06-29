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
#include "src/storage.h"

#include "src/common/src/constants.h"
#include "src/common/src/settings.h"

/***************************************************************************/ /**
 * Macros
 ******************************************************************************/

#define CHAN_MAP_SIZE 5 // size of array used to map channels

#define NO_MAX_DUR 0 // 0 for no duration limit
#define NO_MAX_EVT 0 // 0 for no max events

/* Risk Data */
#define READ_SIZE 8
#define TICK_DELAY 0
#define DATA_DELAY 68 // determined empirically to sync up with advertising interval

// compute the current time as a float in ms
#define now() ((((float) sl_sleeptimer_get_tick_count64()) /  \
                ((float) sl_sleeptimer_get_timer_frequency())) * 1000)

// stands for the time at which advertising started
#define ADV_START adv_start

beacon_storage *get_beacon_storage();

/***************************************************************************/ /**
 * Initialize application.
 ******************************************************************************/
void app_init(void);

void set_risk_data(int len, uint8_t *data);
void send_test_risk_data();

/***************************************************************************/ /**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void);

/***************************************************************************/ /**
 * Timer expiration callback.
 ******************************************************************************/
void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data);

#endif // APP_H
