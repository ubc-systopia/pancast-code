#ifndef DONGLE__H
#define DONGLE__H

// Dongle Application

#include <assert.h>

#define DONGLE_NO_OP assert(1);

#define DONGLE_PLATFORM__GECKO

#ifdef DONGLE_PLATFORM__ZEPHYR
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>
#else
#include "sl_bt_api.h"
#endif

#include "../../common/src/pancast.h"

// STATIC PARAMETERS
// (Approx) number of time units between each report written to output
#define DONGLE_REPORT_INTERVAL DONGLE_MAX_LOG_AGE

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
typedef struct
{
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
typedef struct
{
    dongle_otp_flags flags;
    dongle_otp_val val;
} dongle_otp_t;

// Count for number of encounters
typedef uint64_t enctr_entry_counter_t;

// Management data structure for referring to locations in the
// raw bluetooth data by name. Pointers are linked when a new
// packet is received.
typedef struct
{
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

// Structure to consolidate all fields contained in an encounter
// Used only during testing as copies to this format are expensive
typedef struct
{
    beacon_location_id_t location_id;
    beacon_id_t beacon_id;
    beacon_timer_t beacon_time;
    dongle_timer_t dongle_time;
    beacon_eph_id_t eph_id;
} dongle_encounter_entry;

// High-level routine structure
#ifndef DONGLE_PLATFORM__ZEPHYR
void dongle_start();
#endif
void dongle_scan(void);
void dongle_init();
void dongle_load();
void dongle_loop();
void dongle_report();
#ifdef DONGLE_PLATFORM__ZEPHYR
void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                struct net_buf_simple *ad);
#else
void dongle_log(bd_addr *addr, int8_t rssi, uint8_t *data, uint8_t data_len);
#endif
void dongle_lock();
void dongle_unlock();
void dongle_info();
void dongle_stats();
void dongle_test();
void dongle_on_clock_update();
void dongle_clock_increment();

#endif
