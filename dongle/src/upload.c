#include "./upload.h"

#include <string.h>

#include "encounter.h"
#include "storage.h"

#define LOG_LEVEL__INFO
#include "common/src/util/log.h"
#include "common/src/util/util.h"

#if 0
// Memory
interact_state state;
uint8_t dongle_state;
enctr_entry_counter_t num_recs;
enctr_entry_counter_t next_rec = 0;
dongle_encounter_entry_t send_en;

// STATES
#define DONGLE_UPLOAD_STATE_LOCKED 0x01
#define DONGLE_UPLOAD_STATE_UNLOCKED 0x02
#define DONGLE_UPLOAD_STATE_SEND_DATA_0 0x03
#define DONGLE_UPLOAD_STATE_SEND_DATA_1 0x04
#define DONGLE_UPLOAD_STATE_SEND_DATA_2 0x05
#define DONGLE_UPLOAD_STATE_SEND_DATA_3 0x06
#define DONGLE_UPLOAD_STATE_SEND_DATA_4 0x07

void interact_update()
{
  switch (dongle_state) {
    case DONGLE_UPLOAD_STATE_LOCKED:
      {
        log_debugf("%s", "Dongle is locked\r\n");
        if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS)
            break;

        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_OTP)
            break;

        dongle_storage *storage = get_dongle_storage();
        int otp_idx = dongle_storage_match_otp(storage,
            *((dongle_otp_val *)state.otp.val));
        if (otp_idx <= 0)
          break;

        num_recs = dongle_storage_num_encounters_current(storage);
        if (num_recs != 0)
          dongle_state = DONGLE_UPLOAD_STATE_UNLOCKED;
        state.flags = DONGLE_UPLOAD_DATA_TYPE_NUM_RECS;
        memcpy(state.num_recs.val, &num_recs, sizeof(enctr_entry_counter_t));
      }
      break;

    case DONGLE_UPLOAD_STATE_UNLOCKED:
    case DONGLE_UPLOAD_STATE_SEND_DATA_4:
      {
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS &&
            state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_4)
          break;

        if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS) {
          log_debugf("%s", "Ack for num recs received\r\n");
          log_infof("%s", "Sending records...\r\n");
        }

        if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_4) {
          log_debugf("%s", "Ack for d4 received\r\n");
          next_rec++;
        }

        if (next_rec >= num_recs) {
          dongle_state = DONGLE_UPLOAD_STATE_LOCKED;
          next_rec = 0;
          break;
        }

        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_0;

        // load next encounter
        dongle_storage_load_single_encounter(get_dongle_storage(),
            next_rec, &send_en);

        //_display_encounter_(&send_en);

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_0;
        memcpy(state.data.bytes, &send_en.beacon_id, sizeof(beacon_id_t));
      }
      break;

    case DONGLE_UPLOAD_STATE_SEND_DATA_0:
      {
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_0)
          break;

        log_debugf("%s", "Ack for d0 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_1;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_1;
        memcpy(state.data.bytes, &send_en.beacon_time_start, sizeof(beacon_timer_t));
      }
      break;

    case DONGLE_UPLOAD_STATE_SEND_DATA_1:
      {
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_1)
          break;

        log_debugf("%s", "Ack for d1 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_2;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_2;
        memcpy(state.data.bytes, &send_en.dongle_time_start, sizeof(dongle_timer_t));
      }
      break;

    case DONGLE_UPLOAD_STATE_SEND_DATA_2:
      {
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_2)
          break;

        log_debugf("%s", "Ack for d2 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_3;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_3;
        memcpy(state.data.bytes, send_en.eph_id.bytes, BEACON_EPH_ID_SIZE);
      }
      break;

    case DONGLE_UPLOAD_STATE_SEND_DATA_3:
      {
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_3)
          break;

        log_debugf("%s", "Ack for d3 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_4;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_4;
        memcpy(state.data.bytes, &send_en.location_id, sizeof(beacon_location_id_t));
      }
      break;

    default:
      log_errorf("No match for state! (%d)\r\n", dongle_state);
  }
}

int access_advertise()
{

  dongle_state = DONGLE_UPLOAD_STATE_LOCKED;

  sl_status_t sc;
  uint8_t ad_handle = 0x00;
#define INTERVAL 160 // 100 ms per the example
  sc = sl_bt_advertiser_create_set(&ad_handle);
  if (sc) {
    log_errorf("error creating advertising set: 0x%x\r\n", sc);
    return sc;
  }
  sc = sl_bt_advertiser_set_timing(ad_handle, INTERVAL, INTERVAL, 0, 0);
  if (sc) {
    log_errorf("error setting advertising timing: 0x%x\r\n", sc);
    return sc;
  }
#undef INTERVAL
  sc = sl_bt_advertiser_start(ad_handle, advertiser_general_discoverable,
      advertiser_connectable_scannable);
  if (sc) {
    log_errorf("error starting advertising: 0x%x\r\n", sc);
    return sc;
  }
  log_infof("%s", "Advertising started successfully\r\n");

  return 0;
}
#endif
