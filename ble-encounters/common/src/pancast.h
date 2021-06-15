#ifndef PANCAST__H
#define PANCAST__H

#include <stdint.h>

#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

#define SK_MAX_SIZE 360 // bytes

typedef struct
{
    uint8_t bytes[SK_MAX_SIZE];
} seckey_t;

typedef seckey_t beacon_sk_t;

#define PK_MAX_SIZE 96

typedef struct
{
    uint8_t bytes[PK_MAX_SIZE];
} pubkey_t;

typedef uint32_t key_size_t;

// hard-coded beacon information for development purposes

#define BEACON_EPOCH_LENGTH \
    15                               // number of timer cycles in one epoch should correspond to 15 min in prod.
#define BEACON_TIMER_RESOLUTION 1000 // in ms TODO: should be 1 min in prod.
#define DONGLE_TIMER_RESOLUTION 1000 // in ms TODO: should be 1 min in prod.

typedef uint32_t beacon_epoch_counter_t;
typedef uint32_t dongle_epoch_counter_t;

#define BEACON_EPH_ID_HASH_LEN 14 // number of trailing bytes used
#define BEACON_EPH_ID_SIZE 16

typedef struct
{
    uint8_t bytes[BEACON_EPH_ID_SIZE];
} beacon_eph_id_t;

typedef uint64_t beacon_location_id_t;

typedef uint32_t beacon_timer_t;
typedef uint32_t dongle_timer_t;

typedef uint32_t beacon_id_t;
typedef uint32_t dongle_id_t;

#define ENCOUNTER_BROADCAST_SIZE                             \
    (BEACON_EPH_ID_HASH_LEN + sizeof(beacon_location_id_t) + \
     sizeof(beacon_id_t) + sizeof(beacon_timer_t))

#define MAX_BROADCAST_SIZE 31

typedef struct
{
    uint8_t bytes[MAX_BROADCAST_SIZE];
} encounter_broadcast_raw_t;

// epoch calculation
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C) / BEACON_EPOCH_LENGTH)

#define DONGLE_ENCOUNTER_MIN_TIME \
    5 // number of time units eph. id must be observed

#define NUM_OTP 16 // number of OTPs given to user and present in the dongle

#define BROADCAST_SERVICE_ID 0x2222
static const beacon_id_t BEACON_SERVICE_ID_MASK = 0xffff0000;

#endif