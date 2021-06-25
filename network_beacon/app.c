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
#include <unistd.h>

#include "app.h"
#include "app_iostream_eusart.h"
#include "legacy.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "sl_iostream.h"


// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;

// Risk Broadcast Data
uint8_t risk_data[RISK_DATA_SIZE] = {};
int risk_data_len;

// Current index of periodic data broadcast
// Periodic advertising data to transmit is
// risk_data[index]:risk_data[index+PER_ADV_SIZE]
int adv_index;

/* Initialize application */
void app_init (void)
{
  app_iostream_eusart_init();
  risk_data_len = RISK_DATA_SIZE;
  adv_index = 0;
  memset(&risk_data, 0, risk_data_len);
}


/* Update risk data after receive from raspberry pi client */
void update_risk_data(int len, char* data)
{
  sl_status_t sc;

  if (len > RISK_DATA_SIZE)
    {
      printf ("len: %d larger than current risk size\r\n", len);
      return;
    }

  // reset data
  memset(&risk_data, 0, risk_data_len);

  // copy data from risk buffer
  memcpy(&risk_data, data, len);
  risk_data_len = len;

  printf ("Setting advertising data...\r\n");
  sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                         PER_ADV_SIZE, &risk_data[0]);

  if (sc != 0)
    {
      printf ("Error setting advertising data, sc: 0x%lx", sc);
    }
}

/* Get risk data from rapsberry pi client */
void get_risk_data() {

	// Read length (integer)
	uint64_t data_len = 0;

	read(SL_IOSTREAM_STDIN, &data_len, sizeof(uint64_t));
	if (data_len == 0) {
		printf("No data ready to read\r\n");
		return;
	}

	printf("data_len: %lu\r\n", (long unsigned int)data_len);

	char buf[data_len];

	// Read length bytes from stdin
	read(SL_IOSTREAM_STDIN, &buf, data_len);
    printf("Read: %s\r\n", buf);

	// Update broadcassts data
    update_risk_data(data_len, buf);
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
      printf("Creating advertising set...\r\n");
      sc = sl_bt_advertiser_create_set (&advertising_set_handle);
      app_assert_status(sc);

      // Set PHY
      sc = sl_bt_advertiser_set_phy(advertising_set_handle, sl_bt_gap_1m_phy, sl_bt_gap_2m_phy);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(advertising_set_handle,
    		  MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
			  MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
			  NO_MAX_DUR,   	// adv. duration
			  NO_MAX_EVT);  	// max. num. adv. events
      app_assert_status(sc);

      printf ("Starting periodic advertising...\r\n");
      sc = sl_bt_advertiser_start_periodic_advertising (advertising_set_handle,
      PER_ADV_INTERVAL, PER_ADV_INTERVAL, PER_FLAGS);
      app_assert_status(sc);

      printf ("Setting advertising data...\r\n");
      sc = sl_bt_advertiser_set_data (advertising_set_handle, 8, PER_ADV_SIZE,
                                      &risk_data[adv_index * PER_ADV_SIZE]);
      app_assert_status(sc);

      // Start advertising location ephemeral ID
      start_legacy_advertising();

      printf ("Setting timers...\r\n");
      sc = sl_bt_system_set_soft_timer (PER_UPDATE_FREQ * TIMER_1S, PER_TIMER_HANDLE, 0);
      if (sc != 0)
        {
          printf ("Error setting timer, sc: 0x%lx\r\n", sc);
        }
      sc = sl_bt_system_set_soft_timer(RISK_UPDATE_FREQ * TIMER_1S, RISK_TIMER_HANDLE, 0);
      if (sc != 0)
         {
           printf ("Error setting timer, sc: 0x%lx\r\n", sc);
         }

      printf ("Success!\r\n");

      break;

    case sl_bt_evt_system_soft_timer_id:

      // handle periodic set advertising data
      if (evt->data.handle == PER_TIMER_HANDLE) {
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
      }

      // handle data updates
      if (evt->data.handle == RISK_TIMER_HANDLE) {
    	  get_risk_data();
       }
      break;

    // Default event handler.
    default:
      break;
    }
}

