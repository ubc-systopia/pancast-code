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

#include "src/common/src/constants.h"
#include "src/common/src/util/log.h"
#include "src/common/src/test.h"
#include "src/beacon.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = PER_ADV_HANDLE;

// Risk Broadcast Data
uint8_t risk_data[RISK_DATA_SIZE];
int risk_data_len;

// Current index of periodic data broadcast
// Periodic advertising data to transmit is
// risk_data[index]:risk_data[index+PER_ADV_SIZE]
int adv_index;

uint32_t timer_freq = 0;
uint64_t timer_ticks;

// Channel map is 5 bytes and contains 37 1-bit fields.
// The nth field (in the range 0 to 36) contains the value for the link layer
// channel index n.
const uint8_t chan_map[CHAN_MAP_SIZE] = { 0x04, 0x40, 0x00, 0x00, 0x00 };

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
void set_risk_data(int len, uint8_t *data)
{
  sl_status_t sc = 0;

  if (len <= PER_ADV_SIZE) {
    sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, len, &data[0]);
  }

  if (sc != 0) {
    printf("Error setting periodic advertising data, sc: 0x%lx\r\n", sc);
  }
}

#ifdef PERIODIC_TEST

#define PACKET_REPLICATION 1
#define CHUNK_REPLICATION 1

#define TEST_NUM_PACKETS_PER_FILTER \
  (1 + ((TEST_FILTER_LEN - 1) / MAX_PACKET_SIZE))                       // N

  uint8_t chunk_rep_count = 0;
  uint32_t chunk_num = 0;
  uint8_t pkt_rep_count = 0;
  uint32_t seq_num = 0;
  uint32_t pkt_len;
  uint32_t chunk_len = TEST_FILTER_LEN - 8;
  uint8_t test_data[PER_ADV_SIZE];
  uint8_t test_filter[MAX_FILTER_SIZE];
#endif

void send_test_risk_data()
{
#ifdef PERIODIC_TEST
  float starttime = now();

  if (seq_num == 0) {
    beacon_storage_read_test_filter(get_beacon_storage(), test_filter);
  }

  uint32_t off = 0;
  // sequence number
  memcpy(test_data+off, &seq_num, sizeof(uint32_t));
  off += sizeof(uint32_t);
  // chunk number
  memcpy(test_data+off, &chunk_num, sizeof(uint32_t));
  off += sizeof(uint32_t);
  // chunk length
  memcpy(test_data+off, &chunk_len, sizeof(uint64_t));
  off += sizeof(uint64_t);

  // data
#define min(a,b) (b < a ? b : a)
  pkt_len = min(MAX_PACKET_SIZE, TEST_FILTER_LEN - (seq_num * MAX_PACKET_SIZE));
#undef min

  memcpy(test_data+PACKET_HEADER_LEN,
      test_filter + (seq_num*MAX_PACKET_SIZE), pkt_len);

  // set
  set_risk_data(PACKET_HEADER_LEN + pkt_len, test_data);

  float endtime = now();
  stat_add((endtime - starttime), stats.broadcast_payload_update_duration);

  // update sequence
  pkt_rep_count++;
  if (pkt_rep_count < PACKET_REPLICATION)
    return;

  // all packet retransmissions complete
  pkt_rep_count = 0;
  seq_num++;
  if (seq_num < TEST_NUM_PACKETS_PER_FILTER)
    return;

  // one filter retransmission complete
  seq_num = 0;
  chunk_rep_count++;
  if (chunk_rep_count < CHUNK_REPLICATION)
    return;

  // all filter retransmissions complete
  chunk_rep_count = 0;
  chunk_num++;
  if (chunk_num < TEST_N_FILTERS_PER_PAYLOAD)
    return;

  // payload transmission complete
  chunk_num = 0;
  log_debugf("switched to transmit chunk %lu\r\n", chunk_num);
#endif
}

float adv_start = -1;

void sl_timer_on_expire(
    __attribute__ ((unused)) sl_sleeptimer_timer_handle_t *handle,
    void *data)
{
#define user_handle (*((uint8_t*)data))
  // handle main clock
  if (user_handle == MAIN_TIMER_HANDLE) {
    beacon_clock_increment(1);
  }
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      log_debugf("address: %X %X %X %X %X %X\r\n", address.addr[5], address.addr[4],
    		  address.addr[3], address.addr[2], address.addr[1],
			  address.addr[0]);


      // comment to use random channel selection
      sc = sl_bt_gap_set_data_channel_classification(CHAN_MAP_SIZE, chan_map);

      if (sc != 0) {
    	  log_errorf("Error setting channel map, sc: 0x%X", sc);
      }

      int16_t set_min;
      int16_t set_max;
      sc = sl_bt_system_set_tx_power(MIN_TX_POWER, MAX_TX_POWER, &set_min, &set_max);
      log_debugf("Set min tx power: %d, max tx power: %d", set_min, set_max);

      // Create an advertising set.
      //  printf("Creating advertising set...\r\n");
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set PHY
//        sc = sl_bt_advertiser_set_phy(advertising_set_handle, sl_bt_gap_1m_phy, sl_bt_gap_2m_phy);
//        app_assert_status(sc);

      // Set Power Level
      int16_t set_power;
      sc = sl_bt_advertiser_set_tx_power(advertising_set_handle,
          PER_TX_POWER, &set_power);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(advertising_set_handle,
          MIN_ADV_INTERVAL, // min. adv. interval (milliseconds * 1.6)
          MAX_ADV_INTERVAL, // max. adv. interval (milliseconds * 1.6)
          NO_MAX_DUR,       // adv. duration
          NO_MAX_EVT);      // max. num. adv. events
      app_assert_status(sc);

      log_debugf("%s", "Starting periodic advertising...\r\n");

      adv_start = now();
      sc = sl_bt_advertiser_start_periodic_advertising(advertising_set_handle,
          PER_ADV_INTERVAL, PER_ADV_INTERVAL, PER_FLAGS);

      app_assert_status(sc);

      sc = sl_bt_advertiser_set_data(advertising_set_handle, 8, PER_ADV_SIZE,
          &risk_data[adv_index * PER_ADV_SIZE]);

      app_assert_status(sc);
      log_infof("[Periodic adv] init time: %f ms, adv size: %u\r\n",
          adv_start, PER_ADV_SIZE);

      beacon_start();
      break;

    // Default event handler.
    default:
        break;
  }
}
