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
#include "src/common/src/pancast/riskinfo.h"

/*
 * Channel map is 5 bytes and contains 37 1-bit fields.
 * The nth field (in the range 0 to 36) contains the value
 * for the link layer channel index n.
 */
const uint8_t chan_map[CHAN_MAP_SIZE] = { 0x04, 0x40, 0x00, 0x00, 0x00 };

/*
 * Initialize application
 */
void app_init(void)
{
  app_iostream_eusart_init();

  // Set pin PB01 for output
  GPIO_PinModeSet(gpioPortB, 1, gpioModePushPull, 0);
}

#if PERIODIC_TEST

#define PACKET_REPLICATION 1
#define CHUNK_REPLICATION 1

#define TEST_NUM_PACKETS_PER_FILTER \
  (1 + ((TEST_FILTER_LEN - 1) / MAX_PAYLOAD_SIZE))                       // N

uint8_t chunk_rep_count = 0;
uint32_t chunk_num = 0;
uint8_t pkt_rep_count = 0;
uint32_t seq_num = 0;
uint32_t pkt_len;
uint32_t chunk_len = TEST_FILTER_LEN - HDR_SIZE_BYTES;
uint8_t test_data[PER_ADV_SIZE];
uint8_t test_filter[MAX_FILTER_SIZE];

void send_test_risk_data()
{
  float starttime = now();

  if (seq_num == 0) {
    beacon_storage_read_test_filter(get_beacon_storage(), test_filter);
  }

  rpi_ble_hdr *rbh = (rpi_ble_hdr *) test_data;
  rbh->pkt_seq = seq_num;
  rbh->chunkid = chunk_num;
  rbh->chunklen = chunk_len;

  // data
#define min(a,b) ((b) < (a) ? (b) : (a))
  pkt_len = min(TEST_FILTER_LEN - (seq_num * MAX_PAYLOAD_SIZE),
      MAX_PAYLOAD_SIZE);
#undef min

  memcpy(test_data+sizeof(rpi_ble_hdr),
      test_filter + (seq_num*MAX_PAYLOAD_SIZE), pkt_len);

  // set in BLE payload
  set_risk_data(sizeof(rpi_ble_hdr)+pkt_len, test_data);

  float endtime = now();
  stat_add((endtime - starttime),
      stats.broadcast_payload_update_duration);

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
}
#endif /* PERIODIC_TEST */

float adv_start = -1;

void sl_timer_on_expire(
    __attribute__ ((unused)) sl_sleeptimer_timer_handle_t *handle,
    void *data)
{
#define user_handle (*((uint8_t *) data))
  // handle main clock
  if (user_handle == MAIN_TIMER_HANDLE) {
    beacon_clock_increment(1);
  }
#undef user_handle
}

/*
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 * @param[in] evt Event coming from the Bluetooth stack.
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

      /*
       * extract unique ID from BT address.
       */
      sc = sl_bt_system_get_identity_address(&address, &address_type);

      log_debugf("address: %X %X %X %X %X %X\r\n",
          address.addr[5], address.addr[4], address.addr[3],
          address.addr[2], address.addr[1], address.addr[0]);

      /*
       * comment to use random channel selection
       */
      sc = sl_bt_gap_set_data_channel_classification(CHAN_MAP_SIZE,
          chan_map);

      if (sc != 0) {
        log_errorf("error setting channel map, sc: 0x%X", sc);
      }

      /*
       * set system tx power level
       */
      int16_t set_min;
      int16_t set_max;
      sc = sl_bt_system_set_tx_power(MIN_TX_POWER, MAX_TX_POWER,
          &set_min, &set_max);
      log_debugf("min tx power: %d, max tx power: %d", set_min, set_max);
      if (sc != 0) {
        log_errorf("error setting system tx power, sc: 0x%X", sc);
      }

      /*
       * XXX: Do not comment out!
       * The beacon config and storage get initialized in this function.
       * To disable legacy advertising, comment out the specific advertising
       * functions within this function.
       * Code refactoring should decouple beacon initialization from
       * advertising code.
       */

      beacon_init();

//#if BEACON_MODE__NETWORK
      // set up periodic advertising
      beacon_periodic_advertise();
      adv_start = now();
      log_infof("[periodic adv] init time: %f ms, adv size: %u\r\n",
          adv_start, PER_ADV_SIZE);
//#else
#if BEACON_MODE__NETWORK == 0
      /*
       * legacy advertising
       */
      sc = beacon_legacy_advertise();
      if (sc != 0) {
          log_errorf("Error starting legacy adv: %d\r\n", sc);
      }
      beacon_on_clock_update();
#endif

      break;

    // Default event handler.
    default:
      break;
  }
}
