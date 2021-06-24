#ifndef DONGLE__H
#define DONGLE__H

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>

#include "../../common/src/pancast.h"

typedef struct
{
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

typedef uint64_t enctr_entry_counter_t;

typedef uint64_t dongle_otp_flags;
typedef uint64_t dongle_otp_val;

typedef struct
{
    dongle_otp_flags flags;
    dongle_otp_val val;
} dongle_otp_t;

typedef dongle_otp_t otp_set[NUM_OTP];

typedef struct
{
    dongle_id_t id;
    dongle_timer_t t_init;
    key_size_t backend_pk_size; // size of backend public key
    pubkey_t backend_pk;        // Backend public key
    key_size_t dongle_sk_size;  // size of secret key
    seckey_t dongle_sk;         // Secret Key
} dongle_config_t;

typedef struct
{
    beacon_location_id_t location_id;
    beacon_id_t beacon_id;
    beacon_timer_t beacon_time;
    dongle_timer_t dongle_time;
    beacon_eph_id_t eph_id;
} dongle_encounter_entry;

void dongle_scan(void);
void dongle_init();
void dongle_load();
void dongle_loop();
void dongle_report();
void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                struct net_buf_simple *ad);

void dongle_lock();
void dongle_unlock();

#endif