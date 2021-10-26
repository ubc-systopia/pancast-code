#ifndef DONGLE__H
#define DONGLE__H

// Dongle Application
#include "common/src/settings.h"

// Application Config
//#define MODE__TEST_CONFIG // loads fixed test data instead of from flash
#define MODE__STAT // enables telemetry aggregation
/*
 * config for periodic scanning and syncing
 * 0 - disable
 * 1 - enable
 */
#define MODE__PERIODIC  1
#define MODE__LEGACY_LOG
#define MODE__PERIODIC_FIXED_DATA
//#define CUCKOOFILTER_FIXED_TEST

#if TEST_DONGLE
#define MODE__TEST_CONFIG
#endif

#include <assert.h>

#include "common/src/constants.h"
#include "sl_bt_api.h"

#define DONGLE_NO_OP assert(1);


// STATIC PARAMETERS

// number of distinct broadcast ids to keep track of at one time
#define DONGLE_MAX_BC_TRACKED 16

// Bluetooth Scanning Parameters
#ifdef DONGLE_PLATFORM__ZEPHYR
// These are defined constants
#define DONGLE_SCAN_INTERVAL BT_GAP_SCAN_FAST_INTERVAL
#define DONGLE_SCAN_WINDOW BT_GAP_SCAN_FAST_WINDOW
#else
// These are hard-coded, in ms
#define DONGLE_SCAN_INTERVAL 0x30
#define DONGLE_SCAN_WINDOW 0x60
#endif

// Data Structures

// Fixed configuration info.
// This is typically loaded from non-volatile storage at app
// start and held constant during operation.
typedef struct {
  dongle_id_t id;
  dongle_timer_t t_init;
  key_size_t backend_pk_size; // size of backend public key
  pubkey_t backend_pk;        // Backend public key
  key_size_t dongle_sk_size;  // size of secret key
  seckey_t dongle_sk;         // Secret Key
} dongle_config_t;

// One-Time-Passcode (OTP) representation
// The flags contain metadata such as a 'used' bit
typedef uint64_t dongle_otp_flags;
typedef uint64_t dongle_otp_val;
typedef struct {
  dongle_otp_flags flags;
  dongle_otp_val val;
} dongle_otp_t;

// Count for number of encounters
typedef uint64_t enctr_entry_counter_t;

// Management data structure for referring to locations in the
// raw bluetooth data by name. Pointers are linked when a new
// packet is received.
typedef struct {
  beacon_eph_id_t *eph;
  beacon_location_id_t *loc;
  beacon_id_t *b;
  beacon_timer_t *t;
} encounter_broadcast_t;

// Structure to consolidate all fields contained in an encounter
// Used only during testing as copies to this format are expensive
typedef struct {
  beacon_location_id_t location_id;
  beacon_id_t beacon_id;
  beacon_timer_t beacon_time;
  dongle_timer_t dongle_time;
  beacon_eph_id_t eph_id;
} dongle_encounter_entry;

// Timing Constants
#define MAIN_TIMER_HANDLE 0x00
#define PREC_TIMER_HANDLE 0x01 // high-precision timer
#define PREC_TIMER_TICK_MS 1   // essentially res. of timer

// Periodic Scanning & Synchronization
#define SCAN_PHY 1 // 1M PHY
#define SCAN_WINDOW 320
#define SCAN_INTERVAL 320
#define SCAN_MODE 0 // passive scan

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

// High-level routine structure
void dongle_scan(void);
void dongle_init();
void dongle_load();
void dongle_loop();
void dongle_report();
void dongle_log(bd_addr *addr, int8_t rssi, uint8_t *data, uint8_t data_len);
void dongle_lock();
void dongle_unlock();
void dongle_info();
void dongle_stats();
void dongle_test();
void dongle_download_info();
void dongle_download_test_info();
void dongle_download_fail();
void dongle_on_clock_update();
void dongle_clock_increment();
void dongle_hp_timer_add(uint32_t ticks);
void dongle_on_periodic_data
(uint8_t *data, uint8_t data_len, int8_t rssi);
void dongle_on_periodic_data_error
(int8_t rssi);
void dongle_on_sync_lost();
#endif
