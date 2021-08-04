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
#include "../../common/src/test.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;

// Risk Broadcast Data
uint8_t risk_data[RISK_DATA_SIZE];
int risk_data_len;

// Current index of periodic data broadcast
// Periodic advertising data to transmit is
// risk_data[index]:risk_data[index+PER_ADV_SIZE]
int adv_index;

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
    memset(&risk_data, 0, risk_data_len);

    // copy data from risk buffer
    memcpy(&risk_data, data, len);
    risk_data_len = len;

    //printf ("Setting advertising data...\r\n");
    sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                   risk_data_len, &risk_data[0]);

    if (sc != 0)
    {
        printf ("Error setting advertising data, sc: 0x%lx", sc);
    }
}

#ifdef PERIODIC_TEST
#define NUM_PACKETS PERIODIC_TEST_NUM_PACKETS
  uint32_t seq_num;
  uint8_t test_data[PER_ADV_SIZE];
#endif

/* Get risk data from raspberry pi client */
void get_risk_data()
{
#ifdef PERIODIC_TEST
  log_debug("get risk data\r\n");
  memcpy(test_data, &seq_num, sizeof(uint32_t)); // sequence number
  seq_num++;
  if (seq_num == NUM_PACKETS) {
      seq_num = 0;
  }
  // then a bunch of 22
  memset(&test_data[ sizeof(uint32_t)], 22, PER_ADV_SIZE - sizeof(uint32_t));
  update_risk_data(PER_ADV_SIZE, test_data);
#else
#ifndef BATCH_SIZE

    // set ready pin
    GPIO_PinOutSet(gpioPortB, 1);

    int read_len = 0;
    char buf[PER_ADV_SIZE];

    read_len = read(SL_IOSTREAM_STDIN, &buf, PER_ADV_SIZE);

    // clear pin once data has been received
    GPIO_PinOutClear(gpioPortB, 1);

    // update broadcast data
    if (read_len == PER_ADV_SIZE)
    {
        update_risk_data(PER_ADV_SIZE, buf);
    }
#define MISSING_DATA_BYTE 0x22
#ifdef BEACON_MODE__FILL_MISSING_DOWNLOAD_DATA
    else {
        // fill with bytes for missing data
        memset(&buf[read_len], MISSING_DATA_BYTE, PER_ADV_SIZE - read_len);
        update_risk_data(PER_ADV_SIZE, buf);
    }
#else
    else if (read_len > 0) {
        update_risk_data(read_len, buf);
    } else {
        // fill with bytes for no data
        memset(buf, MISSING_DATA_BYTE, PER_ADV_SIZE);
        update_risk_data(PER_ADV_SIZE, buf);
    }
#endif

#else // BATCH_SIZE defined
    if (adv_index * PER_ADV_SIZE == RISK_DATA_SIZE)
    {

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
#endif
}

float hp_time_now  = 0;
float hp_time_adv_start;

#define now() hp_time_now

int risk_timer_started = 0;

sl_sleeptimer_timer_handle_t hp_timer;

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
#define user_handle (*((uint8_t*)data))

  if (user_handle == HP_TIMER_HANDLE) {
          if (risk_timer_started) {
              return;
          }
          hp_time_now++;
          // start timer when time delta is a multiple of I/2
          // add an additional wait time to prevent problems at start
          if (hp_time_now > TIMER_1S &&
              (((int)(now() - hp_time_adv_start))
                       % (int)((((float)PER_ADV_INTERVAL) * 1.25) / 2.0)) == 0) {
              log_debug("I/2 = %d\r\n",
                        (int)((((float)PER_ADV_INTERVAL) * 1.25) / 2.0));
              // start the risk update timer
#ifdef BEACON_MODE__NETWORK
              // Risk Timer
              uint8_t risk_timer_handle = RISK_TIMER_HANDLE;
              log_info("Starting risk timer\r\n");
              // Interval is the same as the advertising interval
              sl_sleeptimer_timer_handle_t risk_timer;
              sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(&risk_timer,
                                                         PER_ADV_INTERVAL,
                                                         sl_timer_on_expire,
                                                         &risk_timer_handle,
                                         RISK_TIMER_PRIORT, 0);
              risk_timer_started = 1;
              if (sc) {
                  log_errorf("Error starting risk timer: 0x%x\r\n", sc);
                  return;
              }
              log_info("Started\r\n");
              log_info("stopping HP timer\r\n");
              sc = sl_sleeptimer_stop_timer(&hp_timer);
              if (sc) {
                  log_errorf("Error stopping HP timer: 0x%x\r\n", sc);
                  return;
              }
              log_info("Stopped\r\n");
#endif
          }
      }
    // handle main clock
  if (user_handle == MAIN_TIMER_HANDLE)
    {
        beacon_clock_increment(1);
    }

#ifdef BEACON_MODE__NETWORK
    // handle data updates
    else if (user_handle == RISK_TIMER_HANDLE) {
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

        // Set Power Level
        int16_t set_power;
        sc = sl_bt_advertiser_set_tx_power(advertising_set_handle, PER_TX_POWER, &set_power);

        // Set advertising interval to 100ms.
        sc = sl_bt_advertiser_set_timing(advertising_set_handle,
                                         MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
                                         MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
                                         NO_MAX_DUR,       // adv. duration
                                         NO_MAX_EVT);      // max. num. adv. events
        app_assert_status(sc);

        log_info("Starting periodic advertising...\r\n");

        hp_time_adv_start = now();
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
