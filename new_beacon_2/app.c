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

#include "app.h"
#include "sl_bluetooth.h"

#define PER_ADV_HANDLE 0xff
#define PER_ADV_INTERVAL 80
#define PER_FLAGS 0
#define PER_ADV_SIZE 250

#define BEACON_ADV_MIN_INTERVAL 0x30
#define BEACON_ADV_MAX_INTERVAL 0x60

#define LEGACY_ADV_HANDLE 0xf1

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;
static uint8_t legacy_set_handle = LEGACY_ADV_HANDLE;

uint8_t test_data[250];

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
	memset(&test_data, 22, PER_ADV_SIZE);
}

/***************************************************************************//**
* Update risk data after receive from raspberry pi client
******************************************************************************/
void update_risk_data(int len, uint8_t *data)
{
    sl_status_t sc = 0;

    if (len == PER_ADV_SIZE) {
    	sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                   len, &data[0]);
    }

    if (sc != 0)
    {
        printf("Error setting periodic advertising data, sc: 0x%lx\r\n", sc);
    }
}

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  sl_status_t sc;
#define user_handle (*((uint8_t*)data))
    // handle main clock
  if (user_handle == MAIN_TIMER_HANDLE)
    {
        beacon_clock_increment(1);
        printf("timer went off\r\n");
    }
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
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

//        // Create an advertising set.
        sc = sl_bt_advertiser_create_set(&advertising_set_handle);
        if (sc != 0) {
        	printf("error creating advertising set\r\n");
        }

        //        // Set PHY
        //        sc = sl_bt_advertiser_set_phy(advertising_set_handle, sl_bt_gap_1m_phy, sl_bt_gap_2m_phy);
        //
        //        // Set Power Level
        //        int16_t set_power;
        //        sc = sl_bt_advertiser_set_tx_power(advertising_set_handle, PER_TX_POWER, &set_power);
        //
        //        // Set advertising interval to 100ms.
        //        sc = sl_bt_advertiser_set_timing(advertising_set_handle,
        //                                         MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
        //                                         MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
        //                                         NO_MAX_DUR,       // adv. duration
        //                                         NO_MAX_EVT);      // max. num. adv. events

        sc = sl_bt_advertiser_start_periodic_advertising(advertising_set_handle,
                                                                 PER_ADV_INTERVAL, PER_ADV_INTERVAL, PER_FLAGS);

        if (sc != 0) {
            printf("error starting periodic advertising\r\n");
        }

        sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, PER_ADV_SIZE,
                                               &test_data[0]);

        if (sc != 0) {
            printf("error setting advertising data\r\n");
        }

        sc = sl_bt_advertiser_create_set(&legacy_set_handle);
        if (sc != 0)
        {
            printf("error creating legacy set\r\n");
        }

        sc = sl_bt_advertiser_set_timing(
                legacy_set_handle,
                BEACON_ADV_MIN_INTERVAL, // min. adv. interval (milliseconds * 1.6)
                BEACON_ADV_MAX_INTERVAL, // max. adv. interval (milliseconds * 1.6) next: 0x4000
                0,                       // adv. duration, 0 for continuous advertising
                0);
        if (sc != 0) {
            printf("error setting timing\r\n");
        }

        // Start legacy advertising
        sc = sl_bt_advertiser_start(
              legacy_set_handle,
              advertiser_user_data,
              advertiser_non_connectable);
        if (sc != 0) {
           printf("error starting advertising\r\n");
        }

        break;

    default:
        break;
    }
}
