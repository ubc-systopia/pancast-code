#ifndef DONGLE__H
#define DONGLE__H

// Dongle Application
#include "common/src/settings.h"

/*
 * 1 - init test config
 * 0 - load config from storage
 */
#define MODE__SL_DONGLE_TEST_CONFIG      0

/*
 * 1 - test encounters and OTPs inline
 * 0 - disable the test
 */
#define TEST_DONGLE 0

/*
 * stats aggregation
 */
#define MODE__STAT  1

/*
 * config for periodic scanning and syncing
 * 0 - disable
 * 1 - enable
 */
#define MODE__PERIODIC  1
//#define CUCKOOFILTER_FIXED_TEST

#include <assert.h>

#include "common/src/constants.h"
#include "sl_bt_api.h"
#include "sl_sleeptimer.h"

// compute the current time as a float in ms
#define now() (((float) sl_sleeptimer_get_tick_count64()  \
                / (float) sl_sleeptimer_get_timer_frequency()) * 1000)

// STATIC PARAMETERS

/*
 * number of distinct broadcast ids to keep track of at one time.
 *
 * for each correct beacon encountered, there is at most one active encounter.
 * this number indicates max number of beacons encountered simultaneously,
 * which the dongle can track and log. value is affected by size of memory
 * allocations possible in silabs
 */
#define DONGLE_MAX_BC_TRACKED 16

/*
 * time to wait after not seeing an ID before persisting encounter to storage
 * this value should be equal to the epoch length, as after turn of one complete
 * epoch, this encounter id should definitely have "expired".
 */
#define LOG_MIN_WAIT BEACON_EPOCH_LENGTH

/*
 * time to wait after before trying to download again on periodic adv. channel
 */
#define MIN_DOWNLOAD_WAIT 5
/*
 * threshold between consecutive sync open and sync close events,
 * below which there is a strong chance that the dongle is not able to
 * sync because there is no network beacon nearby
 */
#define PERIODIC_SYNC_FAIL_LATENCY  1
/*
 * max # of periodic adv. sync attempts tried in close succession before
 * pausing because there is probably no network beacon nearby
 */
#define NUM_SYNC_ATTEMPTS  5

// Bluetooth Scanning Parameters
// These are hard-coded, in ms
#define DONGLE_SCAN_INTERVAL 0x30
#define DONGLE_SCAN_WINDOW 0x60

// Data Structures

// Count for number of encounters
typedef uint32_t enctr_entry_counter_t;

// LED timer
extern sl_sleeptimer_timer_handle_t *led_timer_handle;

// Fixed configuration info.
// This is typically loaded from non-volatile storage at app
// start and held constant during operation.
typedef struct {
  dongle_id_t id;
  dongle_timer_t t_init;
  dongle_timer_t t_cur;
  key_size_t backend_pk_size;     // size of backend public key
  pubkey_t *backend_pk;           // Backend public key
  key_size_t dongle_sk_size;      // size of secret key
  seckey_t *dongle_sk;            // Secret Key
  enctr_entry_counter_t en_tail;  // Encounter cursor tail
  enctr_entry_counter_t en_head;  // Encounter cursor head
} dongle_config_t;

// One-Time-Passcode (OTP) representation
// The flags contain metadata such as a 'used' bit
typedef uint64_t dongle_otp_flags;
typedef uint64_t dongle_otp_val;
typedef struct {
  dongle_otp_flags flags;
  dongle_otp_val val;
} dongle_otp_t;


// Management data structure for referring to locations in the
// raw bluetooth data by name. Pointers are linked when a new
// packet is received.
typedef struct {
  beacon_eph_id_t *eph;
  beacon_location_id_t *loc;
  beacon_id_t *b;
  beacon_timer_t *t;
} encounter_broadcast_t;

typedef struct {
  beacon_location_id_t location_id;
  beacon_id_t beacon_id;
  beacon_timer_t beacon_time_start;
  dongle_timer_t dongle_time_start;
  uint8_t beacon_time_int;
  uint8_t dongle_time_int;
  int8_t rssi;
  beacon_eph_id_t eph_id;
} dongle_encounter_entry_t;

// Timing Constants
#define MAIN_TIMER_HANDLE 0x00
#define PREC_TIMER_HANDLE 0x01 // high-precision timer
#define PREC_TIMER_TICK_MS 1   // essentially res. of timer
#define LED_TIMER_MS 1000 // 1 s

// Periodic Scanning & Synchronization
#define SCAN_PHY 1 // 1M PHY
#define SCAN_DISCOVER_MODE sl_bt_scanner_discover_observation
#define SCAN_WINDOW 320
#define SCAN_INTERVAL 320
#define SCAN_MODE 0 // passive scan

/*
 * evt_scanner_scan_report.packet_type: bit 7
 * 0 - legacy PDU
 * 1 - extended PDU
 */
#define SCAN_PDU_TYPE_MASK  0x80

#define SYNC_SKIP 0
#define SYNC_TIMEOUT 500 // Unit: 10 ms
#define SYNC_FLAGS 0

#define TIMER_1S 32768

#define TEST_DURATION 1800000

typedef struct {
  uint64_t start_ticks;
  uint64_t end_ticks;
  uint64_t diff;
  uint64_t diff_ms;
} timertest_t;

extern int download_complete;

// High-level routine structure
void dongle_init();
void dongle_init_scan();
void dongle_load();
void dongle_report();
void dongle_on_scan_report(bd_addr *addr, int8_t rssi, uint8_t *data, uint8_t data_len);
void dongle_lock();
void dongle_unlock();
void dongle_info();
void dongle_test();
void dongle_download_info();
void dongle_download_test_info();
void dongle_download_fail();
void dongle_on_clock_update();
void dongle_clock_increment();
void dongle_save_encounters();
void dongle_hp_timer_add(uint32_t ticks);
void dongle_on_periodic_data
(uint8_t *data, uint8_t data_len, int8_t rssi);
void dongle_on_periodic_data_error
(int8_t rssi);
void dongle_on_sync_lost();
int dongle_download_complete_status();
void dongle_update_download_time();
int dongle_print_encounter(enctr_entry_counter_t i, dongle_encounter_entry_t *entry);
void dongle_log_counters();
#endif
