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

#include "app.h"
#include "app_iostream_eusart.h"
#include "legacy.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "sl_iostream.h"
#include <stdio.h>

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// Risk Broadcast Data
uint8_t risk_data[4096] = {};
int risk_data_len;

// Current index of periodic data broadcast
// Periodic advertising data to transmit is
// risk_data[index]:risk_data[index+PER_ADV_SIZE]
int adv_index;


/* Initialize application */
void app_init (void)
{
  app_iostream_eusart_init();
  risk_data_len = 4096;
  adv_index = 0;
  memset(&risk_data, 0, risk_data_len);
}


/* Update risk data after receive from backend server */
void update_risk_data (int len)
{
  if (len > risk_data_len)
    {
      // TODO: handle reallocating array
      printf ("larger than current risk size\r\n");
    }

  memcpy(&risk_data, &risk_data_buffer, len);
  risk_data_len = len;

  printf ("Setting advertising data...\r\n");
  sl_status_t sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                              PER_ADV_SIZE, &risk_data[0]);
  if (sc != 0)
    {
      printf ("Error setting advertising data, sc: 0x%lx", sc);
    }
}

/* Process action related to VCOM */
void app_process_action (void)
{
  app_iostream_eusart_process_action();
}

/* Bluetooth stack event handler.
  This overrides the dummy weak implementation.
  @param[in] evt Event coming from the Bluetooth stack.
*/
void sl_bt_on_event (sl_bt_msg_t *evt)
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
      sc = sl_bt_system_get_identity_address (&address, &address_type);
      app_assert_status(sc);


      // Create an advertising set.
      printf("Creating advertising set\r\n");
      sc = sl_bt_advertiser_create_set (&advertising_set_handle);
      app_assert_status(sc);

      // Set PHY
      sc = sl_bt_advertiser_set_phy(advertising_set_handle, 1, 2);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(advertising_set_handle, 75, // min. adv. interval (milliseconds * 1.6) orig. 160
                                        100, // max. adv. interval (milliseconds * 1.6)
                                        0,   // adv. duration
                                        0);  // max. num. adv. events
      app_assert_status(sc);

      printf ("Starting periodic advertising...\r\n");
      sc = sl_bt_advertiser_start_periodic_advertising (advertising_set_handle,
      PER_ADV_INTERVAL, PER_ADV_INTERVAL, 0);
      app_assert_status(sc);

      printf ("Setting advertising data...\r\n");
      sc = sl_bt_advertiser_set_data (advertising_set_handle, 8, PER_ADV_SIZE,
                                      &risk_data[adv_index * PER_ADV_SIZE]);
      app_assert_status(sc);
      printf ("Success!\r\n");

      // Start advertising location ephemeral ID
      start_legacy_advertising();

      printf ("Setting timer\r\n");
      sc = sl_bt_system_set_soft_timer (32768, 0, 0);
      if (sc != 0)
        {
          printf ("Error setting timer, sc: 0x%lx\r\n", sc);
        }
      sc = sl_bt_system_set_soft_timer(32768*2, 1, 0);
      if (sc != 0)
         {
           printf ("Error setting timer, sc: 0x%lx\r\n", sc);
         }

      break;

    case sl_bt_evt_system_soft_timer_id:

      // handle periodic set advertising data
      if (evt->data.handle == 0) {
    	  printf ("Setting advertising data...\r\n");
    	  sc = sl_bt_advertiser_set_data (advertising_set_handle, 8, PER_ADV_SIZE,
                                      &risk_data[adv_index * PER_ADV_SIZE]);
    	  if (sc != 0)
    	  {
    		  printf ("sc: %lx \r\n", sc);
    	  }
    	  app_assert_status(sc);

    	  adv_index++;
    	  if (adv_index * PER_ADV_SIZE > risk_data_len)
    	  {
    		  adv_index = 0;
    	  }
    	  printf ("Success!\r\n");
      }

      // handle data updates
      if (evt->data.handle == 1) {
    	  int update_len = ready_for_update();
       if (update_len != 0)
        {
         printf ("updating risk data\r\n");
         printf ("\r\nrisk_data_buffer[0]: %u\r\n> ", risk_data_buffer[0]);
         update_risk_data(update_len);
        }
       }
      break;

    // Default event handler.
    default:
      break;
    }
}
