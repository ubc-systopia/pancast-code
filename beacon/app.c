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
#include <stdio.h>
#include <unistd.h>

#include "app.h"
#include "app_iostream_eusart.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "sl_iostream.h"
#include "em_gpio.h"

#include "../../common/src/pancast.h"
#include "../../common/src/log.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;

// Risk Broadcast Data
uint8_t risk_data[RISK_DATA_SIZE];
int risk_data_len;

// Current index of periodic data broadcast
// Periodic advertising data to transmit is
// risk_data[index]:risk_data[index+PER_ADV_SIZE]
int adv_index;

sl_sleeptimer_timer_handle_t risk_timer;
uint8_t risk_timer_handle = RISK_TIMER_HANDLE;

/* Initialize application */
void app_init(void)
{
    app_iostream_eusart_init();
    risk_data_len = RISK_DATA_SIZE;
    memset(&risk_data, 0, risk_data_len);

    // Set pin PB01 for output
    GPIO_PinModeSet(gpioPortB, 1, gpioModePushPull, 0);
}

/* Update risk data after receive from raspberry pi client */
void update_risk_data(int len, char *data)
{
    sl_status_t sc;

    if (len > RISK_DATA_SIZE)
    {
        //    printf ("len: %d larger than current risk size\r\n", len);
        return;
    }

    // reset data
   // memset(&risk_data, 0, risk_data_len);

    // copy data from risk buffer
    memcpy(&risk_data, data, len);
    risk_data_len = len;

    //printf ("Setting advertising data...\r\n");
    sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                   PER_ADV_SIZE, &risk_data[0]);

    if (sc != 0)
    {
        printf ("Error setting advertising data, sc: 0x%lx", sc);
    }
}

/* Get risk data from raspberry pi client */
void get_risk_data()
{
#ifdef PERIODIC_TEST
  uint8_t test_data[PER_ADV_SIZE];
  memset(test_data, 22, PER_ADV_SIZE);
  update_risk_data(PER_ADV_SIZE, test_data);
#else
#ifndef BATCH_SIZE

    sl_status_t sc;

    sl_sleeptimer_stop_timer(&risk_timer);

    fflush(SL_IOSTREAM_STDIN);

    // set ready pin
    GPIO_PinOutSet(gpioPortB, 1);

    int read_len = 0;
    char buf[PER_ADV_SIZE];

    // TODO: add partial read case
    read_len = read(SL_IOSTREAM_STDIN, &buf, PER_ADV_SIZE);

    // clear pin once data has been received
    GPIO_PinOutClear(gpioPortB, 1);

    // update broadcast data

    uint8_t new_risk_data[RISK_DATA_SIZE];
    memcpy(&new_risk_data, buf, PER_ADV_SIZE);
    sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
    	                                   PER_ADV_SIZE, &new_risk_data[0]);

    if (sc != 0)
    {
        printf ("Error setting advertising data, sc: 0x%lx", sc);
   	}


#else // BATCH_SIZE defined
    if (adv_index * PER_ADV_SIZE == RISK_DATA_SIZE)
    {
        //	get and update new risk data
        fflush(SL_IOSTREAM_STDIN);

        // set ready pin
        GPIO_PinOutSet(gpioPortB, 1);

        int read_len = 0;
        char buf[PER_ADV_SIZE * BATCH_SIZE];
        read_len = read(SL_IOSTREAM_STDIN, &buf, PER_ADV_SIZE * BATCH_SIZE);

        // clear pin once data has been received
        GPIO_PinOutClear(gpioPortB, 1);

        // update broadcast data
        if (read_len == PER_ADV_SIZE * BATCH_SIZE)
        {
            update_risk_data(PER_ADV_SIZE * BATCH_SIZE, buf);
        }
        adv_index = 0;
    }
    else
    {
        // increase index and set data
        sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                  PER_ADV_SIZE, &risk_data[adv_index * PER_ADV_SIZE]);
        adv_index++;
    }
#endif
    sc = sl_sleeptimer_start_timer_ms(&risk_timer, RISK_UPDATE_FREQ * TIMER_1S,
       	                                   sl_timer_on_expire, &risk_timer_handle,
										   RISK_TIMER_PRIORT, 0);
#endif
}

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
#define user_handle (*((uint8_t*)data))

    // handle main clock
    if (user_handle == MAIN_TIMER_HANDLE)
    {
        beacon_clock_increment(1);
    }

#ifdef BEACON_MODE__NETWORK
    // handle data updates
    if (user_handle == RISK_TIMER_HANDLE)
    {
        if (handle->priority != RISK_TIMER_PRIORT) {
            log_error("Timer mismatch\r\n");
        }
        get_risk_data();
    }
#endif
#undef user_handle
}

/* Bluetooth stack event handler.
  This overrides the dummy weak implementation.
  @param[in] evt Event coming from the Bluetooth stack.
*/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
    sl_status_t sc;
    bd_addr address;
    uint8_t address_type;

    switch (SL_BT_MSG_ID(evt->header))
    {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

        // Extract unique ID from BT Address.
        sc = sl_bt_system_get_identity_address(&address, &address_type);
        app_assert_status(sc);

        // Create an advertising set.
        //  printf("Creating advertising set...\r\n");
        sc = sl_bt_advertiser_create_set(&advertising_set_handle);
        app_assert_status(sc);

        // Set PHY
        sc = sl_bt_advertiser_set_phy(advertising_set_handle, sl_bt_gap_1m_phy, sl_bt_gap_2m_phy);
        app_assert_status(sc);

        // Set advertising interval to 100ms.
        sc = sl_bt_advertiser_set_timing(advertising_set_handle,
                                         MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
                                         MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
                                         NO_MAX_DUR,       // adv. duration
                                         NO_MAX_EVT);      // max. num. adv. events
        app_assert_status(sc);

        printf("Starting periodic advertising...\r\n");

        sc = sl_bt_advertiser_start_periodic_advertising(advertising_set_handle,
                                                         PER_ADV_INTERVAL, PER_ADV_INTERVAL, PER_FLAGS);
        app_assert_status(sc);

        // printf("Setting advertising data...\r\n");
        sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, PER_ADV_SIZE,
                                       &risk_data[adv_index * PER_ADV_SIZE]);
        app_assert_status(sc);

        beacon_start();
        break;

    case sl_bt_evt_system_soft_timer_id:
    // Default event handler.
    default:
        break;
    }
}
