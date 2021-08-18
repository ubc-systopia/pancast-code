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

uint32_t timer_freq;
uint64_t timer_ticks;

/* Initialize application */
void app_init(void)
{
    app_iostream_eusart_init();
    risk_data_len = RISK_DATA_SIZE;
    memset(&risk_data, 0, risk_data_len);

    // Set pin PB01 for output
    GPIO_PinModeSet(gpioPortB, 1, gpioModePushPull, 0);

    timer_freq = sl_sleeptimer_get_timer_frequency(); // Hz
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

    // Advertising data update
    // data provided is copied into a 'next' buffer by the API,
    // so risk_data can be overwritten following this
    sc = sl_bt_advertiser_set_data(advertising_set_handle, 8,
                                   risk_data_len, &risk_data[0]);

    if (sc != 0)
    {
        log_error("Error setting periodic advertising data, sc: 0x%lx\r\n", sc);
    }
}

#ifdef PERIODIC_TEST

#define PACKET_REPLICATION 1
#define CHUNK_REPLICATION 10

#define TEST_NUM_PACKETS_PER_FILTER \
  (1 + ((TEST_FILTER_LEN - 1) / MAX_PACKET_SIZE))                       // N

  uint8_t chunk_rep_count = 0;
  uint32_t chunk_num = 0;
  uint8_t pkt_rep_count = 0;
  uint32_t seq_num = 0;
  uint32_t pkt_len;
  uint32_t chunk_len = TEST_FILTER_LEN;
  uint8_t test_data[PER_ADV_SIZE];
  uint8_t test_filter[MAX_FILTER_SIZE];
#endif

/* Get risk data from raspberry pi client */
void get_risk_data()
{
#ifdef PERIODIC_TEST
  float time = now();

  if (seq_num == 0) {
      beacon_storage_read_test_filter(get_beacon_storage(), test_filter);
  }

  // chunk number
  memcpy(test_data, &chunk_num, sizeof(uint32_t));

  // sequence number
  memcpy(test_data + sizeof(uint32_t), &seq_num, sizeof(uint32_t));

  memcpy(test_data + 2*sizeof(uint32_t), &chunk_len, sizeof(uint32_t));

  // data
#define min(a,b) (b < a ? b : a)
   pkt_len = min(MAX_PACKET_SIZE,
                   TEST_FILTER_LEN - (seq_num * MAX_PACKET_SIZE));
#undef min

  memcpy(test_data  + PACKET_HEADER_LEN,
         test_filter + (seq_num*MAX_PACKET_SIZE), pkt_len);

  // set
  update_risk_data(PACKET_HEADER_LEN + pkt_len, test_data);

  // update sequence
  pkt_rep_count++;
  if (pkt_rep_count == PACKET_REPLICATION) {
      // all packet retransmissions complete
      pkt_rep_count = 0;
      seq_num++;
      if (seq_num == TEST_NUM_PACKETS_PER_FILTER) {
          // one filter retransmission complete
          seq_num = 0;
          chunk_rep_count++;
          if (chunk_rep_count == CHUNK_REPLICATION) {
              // all filter retransmissions complete
              chunk_rep_count = 0;
              chunk_num++;
              if (chunk_num == TEST_N_FILTERS_PER_PAYLOAD) {
                  // payload transmission complete
                  chunk_num = 0;
              }
              log_infof("switched to transmit chunk %lu\r\n", chunk_num);
          }
      }
  }
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

void sl_timer_on_expire(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  sl_status_t sc;
#define user_handle (*((uint8_t*)data))
    // handle main clock
  if (user_handle == MAIN_TIMER_HANDLE)
    {
        beacon_clock_increment(1);
    }

#ifdef BEACON_MODE__NETWORK
    // handle data updates
    if (user_handle == RISK_TIMER_HANDLE) {
        if (handle->priority != RISK_TIMER_PRIORT) {
            log_error("Timer mismatch\r\n");
        }
        get_risk_data();
    }
#endif
#undef user_handle
}

float adv_start = -1;

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

        adv_start = now();
        sc = sl_bt_advertiser_start_periodic_advertising(advertising_set_handle,
                                                         PER_ADV_INTERVAL, PER_ADV_INTERVAL, PER_FLAGS);

        app_assert_status(sc);

        log_infof("periodic advertising started at %f ms\r\n", adv_start);

        log_info("setting periodic advertising data...\r\n");

        // printf("Setting advertising data...\r\n");
        sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, PER_ADV_SIZE,
                                       &risk_data[adv_index * PER_ADV_SIZE]);

        app_assert_status(sc);
        log_info("periodic advertising data set.\r\n");

        beacon_start();
        break;

    case sl_bt_evt_system_soft_timer_id:
    // Default event handler.
    default:
        break;
    }
}
