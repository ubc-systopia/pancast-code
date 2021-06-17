#ifndef DONGLE__H
#define DONGLE__H

#include "../../common/src/pancast.h"

// SERVICE UUID: e7f72a03-803b-410a-98d4-4be5fad8e217
#define DONGLE_INTERACT_SERVICE_ID_0 0xe7f72a03
#define DONGLE_INTERACT_SERVICE_ID_1 0x803b
#define DONGLE_INTERACT_SERVICE_ID_2 0x410a
#define DONGLE_INTERACT_SERVICE_ID_3 0x98d4
#define DONGLE_INTERACT_SERVICE_ID_4 0x4be5fad8e217

#define DONGLE_SERVICE_UUID BT_UUID_128_ENCODE( \
    DONGLE_INTERACT_SERVICE_ID_0,               \
    DONGLE_INTERACT_SERVICE_ID_1,               \
    DONGLE_INTERACT_SERVICE_ID_2,               \
    DONGLE_INTERACT_SERVICE_ID_3,               \
    DONGLE_INTERACT_SERVICE_ID_4)

#define DONGLE_CHARACTERISTIC_UUID BT_UUID_128_ENCODE( \
    DONGLE_INTERACT_SERVICE_ID_0,                      \
    DONGLE_INTERACT_SERVICE_ID_1,                      \
    DONGLE_INTERACT_SERVICE_ID_2,                      \
    DONGLE_INTERACT_SERVICE_ID_3,                      \
    DONGLE_INTERACT_SERVICE_ID_4 + 1)

#define DONGLE_CHARACTERISTIC2_UUID BT_UUID_128_ENCODE( \
    DONGLE_INTERACT_SERVICE_ID_0,                       \
    DONGLE_INTERACT_SERVICE_ID_1,                       \
    DONGLE_INTERACT_SERVICE_ID_2,                       \
    DONGLE_INTERACT_SERVICE_ID_3,                       \
    DONGLE_INTERACT_SERVICE_ID_4 + 2)

typedef struct
{
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

typedef uint64_t enctr_entry_counter_t;

typedef struct
{
    uint64_t flags;
    uint64_t val;
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

// typedef union
// {
//     uint8_t flags;
//     uint8_t data[20];
// } interact_state;

typedef struct
{
    uint8_t flags;
    // union
    // {
    //     dongle_otp_t otp;
    //     dongle_encounter_entry rec;
    // } data;
} interact_state;

void dongle_scan(void);
int dongle_advertise();
void _bas_notify_();
void _peer_update_();

#endif